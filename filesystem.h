// FileSystem.h
#pragma once
#include <vector>
#include <string>
#include <map>
#include <fstream>

const int BLOCK_SIZE = 512;      // ���С
const int BLOCK_COUNT = 1024;    // �ܿ���
const int FAT_ENTRY_COUNT = BLOCK_COUNT;
const int BITMAP_SIZE = (BLOCK_COUNT + 7) / 8; // λͼ�ֽ���

struct DirEntry {
    std::string name;
    bool isDirectory;
    int startBlock;
    int size;
    std::map<std::string, DirEntry> children; // ��Ŀ¼/�ļ�
};

class FileSystem {
private:
    char* memory;                // �ڴ��ļ�ϵͳ�ռ�
    uint8_t* bitmap;             // ���п�λͼ
    uint16_t* fat;               // FAT��
    DirEntry root;               // ��Ŀ¼
    DirEntry* currentDir;        // ��ǰĿ¼
    std::map<int, bool> openFiles; // ���ļ���: <��ʼ��, �Ƿ��>

    // ��������
    int allocateBlock();
    void freeBlockChain(int startBlock);
    bool findEntry(const std::string& path, DirEntry** entry, DirEntry** parent);
    void serializeDir(std::ofstream& ofs, DirEntry* dir);
    void deserializeDir(std::ifstream& ifs, DirEntry* dir);

public:
    FileSystem();
    ~FileSystem();

    // ���̲���
    void format();
    void saveToDisk(const std::string& filename);
    void loadFromDisk(const std::string& filename);

    // Ŀ¼����
    bool mkdir(const std::string& path);
    bool rmdir(const std::string& path);
    std::vector<std::string> listDir(const std::string& path = "");
    bool changeDir(const std::string& path);

    // �ļ�����
    bool createFile(const std::string& path);
    bool openFile(const std::string& path);
    bool closeFile(const std::string& path);
    bool writeFile(const std::string& path, const std::string& data);
    std::string readFile(const std::string& path, int size = -1);
    bool deleteFile(const std::string& path);
};