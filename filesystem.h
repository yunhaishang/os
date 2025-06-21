// FileSystem.h
#pragma once
#include <vector>
#include <string>
#include <map>
#include <fstream>

const int BLOCK_SIZE = 512;      // 块大小
const int BLOCK_COUNT = 1024;    // 总块数
const int FAT_ENTRY_COUNT = BLOCK_COUNT;
const int BITMAP_SIZE = (BLOCK_COUNT + 7) / 8; // 位图字节数

struct DirEntry {
    std::string name;
    bool isDirectory;
    int startBlock;
    int size;
    std::map<std::string, DirEntry> children; // 子目录/文件
};

class FileSystem {
private:
    char* memory;                // 内存文件系统空间
    uint8_t* bitmap;             // 空闲块位图
    uint16_t* fat;               // FAT表
    DirEntry root;               // 根目录
    DirEntry* currentDir;        // 当前目录
    std::map<int, bool> openFiles; // 打开文件表: <起始块, 是否打开>

    // 辅助函数
    int allocateBlock();
    void freeBlockChain(int startBlock);
    bool findEntry(const std::string& path, DirEntry** entry, DirEntry** parent);
    void serializeDir(std::ofstream& ofs, DirEntry* dir);
    void deserializeDir(std::ifstream& ifs, DirEntry* dir);

public:
    FileSystem();
    ~FileSystem();

    // 磁盘操作
    void format();
    void saveToDisk(const std::string& filename);
    void loadFromDisk(const std::string& filename);

    // 目录操作
    bool mkdir(const std::string& path);
    bool rmdir(const std::string& path);
    std::vector<std::string> listDir(const std::string& path = "");
    bool changeDir(const std::string& path);

    // 文件操作
    bool createFile(const std::string& path);
    bool openFile(const std::string& path);
    bool closeFile(const std::string& path);
    bool writeFile(const std::string& path, const std::string& data);
    std::string readFile(const std::string& path, int size = -1);
    bool deleteFile(const std::string& path);
};