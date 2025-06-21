// FileSystem.cpp
#include "FileSystem.h"
#include <stack>
#include <cstring>
#include <sstream>
#include <algorithm>

FileSystem::FileSystem() : memory(new char[BLOCK_COUNT * BLOCK_SIZE]) {
    bitmap = reinterpret_cast<uint8_t*>(memory);
    fat = reinterpret_cast<uint16_t*>(memory + BITMAP_SIZE);

    // ��ʼ����Ŀ¼
    root.name = "/";
    root.isDirectory = true;
    root.startBlock = -1;  // Ŀ¼��ʹ�����ݿ�
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
            bitmap[byteIndex] |= (1 << bitIndex); // ���Ϊ����
            fat[i] = 0xFFFF; // �ļ��������
            return i;
        }
    }
    return -1; // �޿��ÿ�
}

void FileSystem::freeBlockChain(int startBlock) {
    int block = startBlock;
    while (block != -1 && block != 0xFFFF) {
        int next = fat[block];
        int byteIndex = block / 8;
        int bitIndex = block % 8;
        bitmap[byteIndex] &= ~(1 << bitIndex); // ���Ϊ����
        fat[block] = 0;
        block = next;
    }
}

// ·����������
bool FileSystem::findEntry(const std::string& path, DirEntry** entry, DirEntry** parent) {
    if (path.empty()) {
        if (entry) *entry = currentDir;
        if (parent) *parent = nullptr;
        return true;
    }

    std::vector<std::string> parts;
    std::istringstream iss(path);
    std::string part;

    // �ָ�·��
    while (std::getline(iss, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }

    DirEntry* cur = currentDir;
    if (path[0] == '/') {
        cur = &root; // ����·���Ӹ���ʼ
    }

    if (parent) *parent = nullptr;

    for (size_t i = 0; i < parts.size(); i++) {
        const std::string& name = parts[i];

        if (name == ".") {
            // ��ǰĿ¼�����ı�
            continue;
        }

        if (name == "..") {
            // ��Ŀ¼
            if (cur == &root) {
                // ��Ŀ¼û�и�Ŀ¼
                continue;
            }

            if (parent) {
                // �ҵ���Ŀ¼�ĸ�Ŀ¼
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
            return false; // δ�ҵ�
        }

        if (i == parts.size() - 1) {
            if (entry) *entry = &it->second;
            return true;
        }

        if (!it->second.isDirectory) {
            return false; // ·���м䲻�����ļ�
        }

        if (parent) *parent = cur;
        cur = &it->second;
    }

    if (entry) *entry = cur;
    return true;
}

void FileSystem::format() {
    // ���λͼ
    memset(bitmap, 0, BITMAP_SIZE);

    // ��ʼ��FAT��
    for (int i = 0; i < FAT_ENTRY_COUNT; i++) {
        fat[i] = 0;
    }

    // �ؽ���Ŀ¼
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

        // д���ļ���
        size_t nameLen = name.size();
        ofs.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
        ofs.write(name.c_str(), nameLen);

        // д��Ŀ¼��
        ofs.write(reinterpret_cast<const char*>(&entry.isDirectory), sizeof(bool));
        ofs.write(reinterpret_cast<const char*>(&entry.startBlock), sizeof(int));
        ofs.write(reinterpret_cast<const char*>(&entry.size), sizeof(int));

        // �����Ŀ¼���ݹ����л�
        if (entry.isDirectory) {
            serializeDir(ofs, const_cast<DirEntry*>(&dir->children.at(name)));
        }
    }
}

void FileSystem::deserializeDir(std::ifstream& ifs, DirEntry* dir) {
    size_t count;
    ifs.read(reinterpret_cast<char*>(&count), sizeof(count));

    for (size_t i = 0; i < count; i++) {
        // ��ȡ�ļ���
        size_t nameLen;
        ifs.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
        std::vector<char> nameBuf(nameLen);
        ifs.read(nameBuf.data(), nameLen);
        std::string name(nameBuf.begin(), nameBuf.end());

        // ������Ŀ¼��
        DirEntry entry;
        ifs.read(reinterpret_cast<char*>(&entry.isDirectory), sizeof(bool));
        ifs.read(reinterpret_cast<char*>(&entry.startBlock), sizeof(int));
        ifs.read(reinterpret_cast<char*>(&entry.size), sizeof(int));
        entry.name = name;

        // ��ӵ���ǰĿ¼
        dir->children[name] = entry;

        // �����Ŀ¼���ݹ����
        if (entry.isDirectory) {
            deserializeDir(ifs, &dir->children[name]);
        }
    }
}


void FileSystem::saveToDisk(const std::string& filename) {
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) return;

    // ����Ԫ����
    ofs.write(reinterpret_cast<char*>(bitmap), BITMAP_SIZE);
    ofs.write(reinterpret_cast<char*>(fat), FAT_ENTRY_COUNT * sizeof(uint16_t));

    // ����Ŀ¼�ṹ
    serializeDir(ofs, &root);
    ofs.close();
}

void FileSystem::loadFromDisk(const std::string& filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) return;

    // ����Ԫ����
    ifs.read(reinterpret_cast<char*>(bitmap), BITMAP_SIZE);
    ifs.read(reinterpret_cast<char*>(fat), FAT_ENTRY_COUNT * sizeof(uint16_t));

    // ����Ŀ¼�ṹ
    root.children.clear();
    deserializeDir(ifs, &root);
    currentDir = &root;
    openFiles.clear();
}

// Ŀ¼����ʵ��
bool FileSystem::mkdir(const std::string& path) {
    // ����Ŀ¼���͸�Ŀ¼·��
    size_t pos = path.find_last_of('/');
    std::string dirName, parentPath;

    if (pos == std::string::npos) {
        // �ڵ�ǰĿ¼����
        dirName = path;
        parentPath = "";
    }
    else {
        dirName = path.substr(pos + 1);
        parentPath = path.substr(0, pos);
    }

    // ���Ҹ�Ŀ¼
    DirEntry* parent = nullptr;
    if (!findEntry(parentPath, &parent, nullptr) || !parent || !parent->isDirectory) {
        return false;
    }

    // ����Ƿ��Ѵ���
    if (parent->children.find(dirName) != parent->children.end()) {
        return false;
    }

    // ������Ŀ¼
    DirEntry newDir;
    newDir.name = dirName;
    newDir.isDirectory = true;
    newDir.startBlock = -1; // Ŀ¼��ʹ�����ݿ�
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

    // ���Ŀ¼�Ƿ�Ϊ��
    if (!dir->children.empty()) {
        return false;
    }

    // �Ӹ�Ŀ¼ɾ��
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

    // �޸���ʹ��C++11���ݵĵ�����ʽ
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

// �ļ�����ʵ��
bool FileSystem::createFile(const std::string& path) {
    // �����ļ����͸�Ŀ¼·��
    size_t pos = path.find_last_of('/');
    std::string fileName, parentPath;

    if (pos == std::string::npos) {
        // �ڵ�ǰĿ¼����
        fileName = path;
        parentPath = "";
    }
    else {
        fileName = path.substr(pos + 1);
        parentPath = path.substr(0, pos);
    }

    // ���Ҹ�Ŀ¼
    DirEntry* parent = nullptr;
    if (!findEntry(parentPath, &parent, nullptr) || !parent || !parent->isDirectory) {
        return false;
    }

    // ����Ƿ��Ѵ���
    if (parent->children.find(fileName) != parent->children.end()) {
        return false;
    }

    // �����һ����
    int block = allocateBlock();
    if (block == -1) return false;

    // �����ļ�
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

    // ����ļ��Ƿ��
    if (openFiles.find(file->startBlock) == openFiles.end()) {
        return false;
    }

    // �ͷ�ԭ�п���
    freeBlockChain(file->startBlock);

    int size = data.size();
    int blocksNeeded = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    int prevBlock = -1;
    int firstBlock = -1;

    // �����¿���
    for (int i = 0; i < blocksNeeded; i++) {
        int block = allocateBlock();
        if (block == -1) {
            // ����ʧ�ܣ��ͷ��ѷ����
            if (firstBlock != -1) freeBlockChain(firstBlock);
            return false;
        }

        if (i == 0) {
            firstBlock = block;
        }

        if (prevBlock != -1) {
            fat[prevBlock] = block;
        }

        // ��������
        int offset = i * BLOCK_SIZE;
        int bytesToCopy = std::min(BLOCK_SIZE, size - offset);
        memcpy(memory + block * BLOCK_SIZE, data.data() + offset, bytesToCopy);

        prevBlock = block;
    }

    // �����ļ���Ϣ
    file->startBlock = firstBlock;
    file->size = size;
    fat[prevBlock] = 0xFFFF; // �������

    return true;
}

std::string FileSystem::readFile(const std::string& path, int size) {
    DirEntry* file = nullptr;
    if (!findEntry(path, &file, nullptr) || !file || file->isDirectory) {
        return "";
    }

    // ����ļ��Ƿ��
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

    // ����ļ��Ƿ��
    if (openFiles.find(file->startBlock) != openFiles.end()) {
        return false;
    }

    // �ͷ����ݿ�
    freeBlockChain(file->startBlock);

    // �Ӹ�Ŀ¼ɾ��
    if (parent) {
        parent->children.erase(file->name);
    }

    return true;
}