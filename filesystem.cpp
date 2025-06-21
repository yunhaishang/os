// FileSystem.cpp
#include "FileSystem.h"
#include <stack>
#include <cstring>
#include <sstream>
#include <algorithm>

FileSystem::FileSystem() : memory(new char[BLOCK_COUNT * BLOCK_SIZE]) {
    bitmap = reinterpret_cast<uint8_t*>(memory);
    fat = reinterpret_cast<uint16_t*>(memory + BITMAP_SIZE);

    // 初始化根目录
    root.name = "/";
    root.isDirectory = true;
    root.startBlock = -1;  // 目录不使用数据块
    root.size = 0;
    currentDir = &root;
}

FileSystem::~FileSystem() {
    delete[] memory;
}

int FileSystem::allocateBlock() {
    for (int i = 0; i < BLOCK_COUNT; i++) {
        int byteIndex = i / 8;
        int bitIndex = i % 8;
        if (!(bitmap[byteIndex] & (1 << bitIndex))) {
            bitmap[byteIndex] |= (1 << bitIndex); // 标记为已用
            fat[i] = 0xFFFF; // 文件结束标记
            return i;
        }
    }
    return -1; // 无可用块
}

void FileSystem::freeBlockChain(int startBlock) {
    int block = startBlock;
    while (block != -1 && block != 0xFFFF) {
        int next = fat[block];
        int byteIndex = block / 8;
        int bitIndex = block % 8;
        bitmap[byteIndex] &= ~(1 << bitIndex); // 标记为空闲
        fat[block] = 0;
        block = next;
    }
}

// 路径解析函数
bool FileSystem::findEntry(const std::string& path, DirEntry** entry, DirEntry** parent) {
    if (path.empty()) {
        if (entry) *entry = currentDir;
        if (parent) *parent = nullptr;
        return true;
    }

    std::vector<std::string> parts;
    std::istringstream iss(path);
    std::string part;

    // 分割路径
    while (std::getline(iss, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }

    DirEntry* cur = currentDir;
    if (path[0] == '/') {
        cur = &root; // 绝对路径从根开始
    }

    if (parent) *parent = nullptr;

    for (size_t i = 0; i < parts.size(); i++) {
        const std::string& name = parts[i];

        if (name == ".") {
            // 当前目录，不改变
            continue;
        }

        if (name == "..") {
            // 父目录
            if (cur == &root) {
                // 根目录没有父目录
                continue;
            }

            if (parent) {
                // 找到父目录的父目录
                DirEntry* temp = cur;
                cur = nullptr;
                for (auto& p : root.children) {
                    for (auto& c : p.second.children) {
                        if (&c.second == temp) {
                            cur = &p.second;
                            break;
                        }
                    }
                    if (cur) break;
                }
                if (!cur) return false;
            }
            continue;
        }

        auto it = cur->children.find(name);
        if (it == cur->children.end()) {
            return false; // 未找到
        }

        if (i == parts.size() - 1) {
            if (entry) *entry = &it->second;
            return true;
        }

        if (!it->second.isDirectory) {
            return false; // 路径中间不能是文件
        }

        if (parent) *parent = cur;
        cur = &it->second;
    }

    if (entry) *entry = cur;
    return true;
}

void FileSystem::format() {
    // 清空位图
    memset(bitmap, 0, BITMAP_SIZE);

    // 初始化FAT表
    for (int i = 0; i < FAT_ENTRY_COUNT; i++) {
        fat[i] = 0;
    }

    // 重建根目录
    root.children.clear();
    currentDir = &root;
    openFiles.clear();
}

void FileSystem::serializeDir(std::ofstream& ofs, DirEntry* dir) {
    size_t count = dir->children.size();
    ofs.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& child : dir->children) {
        const std::string& name = child.first;
        const DirEntry& entry = child.second;

        // 写入文件名
        size_t nameLen = name.size();
        ofs.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
        ofs.write(name.c_str(), nameLen);

        // 写入目录项
        ofs.write(reinterpret_cast<const char*>(&entry.isDirectory), sizeof(bool));
        ofs.write(reinterpret_cast<const char*>(&entry.startBlock), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&entry.size), sizeof(int));

        // 如果是目录，递归序列化
        if (entry.isDirectory) {
            serializeDir(ofs, const_cast<DirEntry*>(&dir->children.at(name)));
        }
    }
}

void FileSystem::deserializeDir(std::ifstream& ifs, DirEntry* dir) {
    size_t count;
    ifs.read(reinterpret_cast<char*>(&count), sizeof(count));

    for (size_t i = 0; i < count; i++) {
        // 读取文件名
        size_t nameLen;
        ifs.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
        std::vector<char> nameBuf(nameLen);
        ifs.read(nameBuf.data(), nameLen);
        std::string name(nameBuf.begin(), nameBuf.end());

        // 创建新目录项
        DirEntry entry;
        ifs.read(reinterpret_cast<char*>(&entry.isDirectory), sizeof(bool));
        ifs.read(reinterpret_cast<char*>(&entry.startBlock), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&entry.size), sizeof(int));
        entry.name = name;

        // 添加到当前目录
        dir->children[name] = entry;

        // 如果是目录，递归加载
        if (entry.isDirectory) {
            deserializeDir(ifs, &dir->children[name]);
        }
    }
}


void FileSystem::saveToDisk(const std::string& filename) {
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) return;

    // 保存元数据
    ofs.write(reinterpret_cast<char*>(bitmap), BITMAP_SIZE);
    ofs.write(reinterpret_cast<char*>(fat), FAT_ENTRY_COUNT * sizeof(uint16_t));

    // 保存目录结构
    serializeDir(ofs, &root);
    ofs.close();
}

void FileSystem::loadFromDisk(const std::string& filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) return;

    // 加载元数据
    ifs.read(reinterpret_cast<char*>(bitmap), BITMAP_SIZE);
    ifs.read(reinterpret_cast<char*>(fat), FAT_ENTRY_COUNT * sizeof(uint16_t));

    // 加载目录结构
    root.children.clear();
    deserializeDir(ifs, &root);
    currentDir = &root;
    openFiles.clear();
}

// 目录操作实现
bool FileSystem::mkdir(const std::string& path) {
    // 分离目录名和父目录路径
    size_t pos = path.find_last_of('/');
    std::string dirName, parentPath;

    if (pos == std::string::npos) {
        // 在当前目录创建
        dirName = path;
        parentPath = "";
    }
    else {
        dirName = path.substr(pos + 1);
        parentPath = path.substr(0, pos);
    }

    // 查找父目录
    DirEntry* parent = nullptr;
    if (!findEntry(parentPath, &parent, nullptr) || !parent || !parent->isDirectory) {
        return false;
    }

    // 检查是否已存在
    if (parent->children.find(dirName) != parent->children.end()) {
        return false;
    }

    // 创建新目录
    DirEntry newDir;
    newDir.name = dirName;
    newDir.isDirectory = true;
    newDir.startBlock = -1; // 目录不使用数据块
    newDir.size = 0;

    parent->children[dirName] = newDir;
    return true;
}

bool FileSystem::rmdir(const std::string& path) {
    DirEntry* dir = nullptr;
    DirEntry* parent = nullptr;

    if (!findEntry(path, &dir, &parent) || !dir || !dir->isDirectory) {
        return false;
    }

    // 检查目录是否为空
    if (!dir->children.empty()) {
        return false;
    }

    // 从父目录删除
    if (parent) {
        parent->children.erase(dir->name);
    }
    return true;
}

std::vector<std::string> FileSystem::listDir(const std::string& path) {
    std::vector<std::string> result;
    DirEntry* target = currentDir;

    if (!path.empty()) {
        if (!findEntry(path, &target, nullptr) || !target || !target->isDirectory) {
            result.push_back("Invalid directory: " + path);
            return result;
        }
    }

    // 修复：使用C++11兼容的迭代方式
    for (auto it = target->children.begin(); it != target->children.end(); ++it) {
        const std::string& name = it->first;
        const DirEntry& entry = it->second;
        result.push_back((entry.isDirectory ? "[DIR] " : "[FILE] ") + name);
    }

    return result;
}

bool FileSystem::changeDir(const std::string& path) {
    DirEntry* newDir = nullptr;
    if (!findEntry(path, &newDir, nullptr) || !newDir || !newDir->isDirectory) {
        return false;
    }

    currentDir = newDir;
    return true;
}

// 文件操作实现
bool FileSystem::createFile(const std::string& path) {
    // 分离文件名和父目录路径
    size_t pos = path.find_last_of('/');
    std::string fileName, parentPath;

    if (pos == std::string::npos) {
        // 在当前目录创建
        fileName = path;
        parentPath = "";
    }
    else {
        fileName = path.substr(pos + 1);
        parentPath = path.substr(0, pos);
    }

    // 查找父目录
    DirEntry* parent = nullptr;
    if (!findEntry(parentPath, &parent, nullptr) || !parent || !parent->isDirectory) {
        return false;
    }

    // 检查是否已存在
    if (parent->children.find(fileName) != parent->children.end()) {
        return false;
    }

    // 分配第一个块
    int block = allocateBlock();
    if (block == -1) return false;

    // 创建文件
    DirEntry newFile;
    newFile.name = fileName;
    newFile.isDirectory = false;
    newFile.startBlock = block;
    newFile.size = 0;

    parent->children[fileName] = newFile;
    return true;
}

bool FileSystem::openFile(const std::string& path) {
    DirEntry* file = nullptr;
    if (!findEntry(path, &file, nullptr) || !file || file->isDirectory) {
        return false;
    }

    openFiles[file->startBlock] = true;
    return true;
}

bool FileSystem::closeFile(const std::string& path) {
    DirEntry* file = nullptr;
    if (!findEntry(path, &file, nullptr) || !file || file->isDirectory) {
        return false;
    }

    openFiles.erase(file->startBlock);
    return true;
}

bool FileSystem::writeFile(const std::string& path, const std::string& data) {
    DirEntry* file = nullptr;
    if (!findEntry(path, &file, nullptr) || !file || file->isDirectory) {
        return false;
    }

    // 检查文件是否打开
    if (openFiles.find(file->startBlock) == openFiles.end()) {
        return false;
    }

    // 释放原有块链
    freeBlockChain(file->startBlock);

    int size = data.size();
    int blocksNeeded = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    int prevBlock = -1;
    int firstBlock = -1;

    // 分配新块链
    for (int i = 0; i < blocksNeeded; i++) {
        int block = allocateBlock();
        if (block == -1) {
            // 分配失败，释放已分配块
            if (firstBlock != -1) freeBlockChain(firstBlock);
            return false;
        }

        if (i == 0) {
            firstBlock = block;
        }

        if (prevBlock != -1) {
            fat[prevBlock] = block;
        }

        // 复制数据
        int offset = i * BLOCK_SIZE;
        int bytesToCopy = std::min(BLOCK_SIZE, size - offset);
        memcpy(memory + block * BLOCK_SIZE, data.data() + offset, bytesToCopy);

        prevBlock = block;
    }

    // 更新文件信息
    file->startBlock = firstBlock;
    file->size = size;
    fat[prevBlock] = 0xFFFF; // 结束标记

    return true;
}

std::string FileSystem::readFile(const std::string& path, int size) {
    DirEntry* file = nullptr;
    if (!findEntry(path, &file, nullptr) || !file || file->isDirectory) {
        return "";
    }

    // 检查文件是否打开
    if (openFiles.find(file->startBlock) == openFiles.end()) {
        return "";
    }

    if (size == -1 || size > file->size) {
        size = file->size;
    }

    std::string content;
    int block = file->startBlock;
    int bytesRead = 0;

    while (block != 0xFFFF && bytesRead < size) {
        int offset = block * BLOCK_SIZE;
        int bytesToRead = std::min(BLOCK_SIZE, size - bytesRead);

        content.append(memory + offset, bytesToRead);
        bytesRead += bytesToRead;
        block = fat[block];
    }

    return content;
}

bool FileSystem::deleteFile(const std::string& path) {
    DirEntry* file = nullptr;
    DirEntry* parent = nullptr;

    if (!findEntry(path, &file, &parent) || !file || file->isDirectory) {
        return false;
    }

    // 检查文件是否打开
    if (openFiles.find(file->startBlock) != openFiles.end()) {
        return false;
    }

    // 释放数据块
    freeBlockChain(file->startBlock);

    // 从父目录删除
    if (parent) {
        parent->children.erase(file->name);
    }

    return true;
}