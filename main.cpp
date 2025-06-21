#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Multiline_Output.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Native_File_Chooser.H>
#include "FileSystem.h"

FileSystem fs;

// 全局控件指针
Fl_Output* status_output = nullptr;
Fl_Multiline_Output* content_output = nullptr;
Fl_Input* path_input = nullptr;
Fl_Input* data_input = nullptr;
Fl_Choice* mode_choice = nullptr;

// 文件系统操作状态
std::string current_file;
bool file_open = false;

void update_status(const std::string& message) {
    status_output->value(message.c_str());
}

void update_content(const std::string& content) {
    content_output->value(content.c_str());
}

void clear_content() {
    content_output->value("");
}

void format_cb(Fl_Widget*, void*) {
    fs.format();
    clear_content();
    update_status("File system formatted");
    file_open = false;
    current_file.clear();
}

void list_cb(Fl_Widget*, void*) {
    auto list = fs.listDir();
    std::string content;
    for (auto& item : list) {
        content += item + "\n";
    }
    if (content.empty()) {
        content = "Directory is empty";
    }
    update_content(content);
    update_status("Directory listed");
}

void mkdir_cb(Fl_Widget*, void*) {
    const char* path = path_input->value();
    if (strlen(path) == 0) {
        update_status("Please enter a directory path");
        return;
    }

    if (fs.mkdir(path)) {
        update_status("Directory created: " + std::string(path));
    }
    else {
        update_status("Failed to create directory: " + std::string(path));
    }
}

void rmdir_cb(Fl_Widget*, void*) {
    const char* path = path_input->value();
    if (strlen(path) == 0) {
        update_status("Please enter a directory path");
        return;
    }

    if (fs.rmdir(path)) {
        update_status("Directory removed: " + std::string(path));
    }
    else {
        update_status("Failed to remove directory: " + std::string(path));
    }
}

void chdir_cb(Fl_Widget*, void*) {
    const char* path = path_input->value();
    if (strlen(path) == 0) {
        update_status("Please enter a directory path");
        return;
    }

    if (fs.changeDir(path)) {
        update_status("Changed to directory: " + std::string(path));
    }
    else {
        update_status("Failed to change directory: " + std::string(path));
    }
}

void create_file_cb(Fl_Widget*, void*) {
    const char* path = path_input->value();
    if (strlen(path) == 0) {
        update_status("Please enter a file path");
        return;
    }

    if (fs.createFile(path)) {
        update_status("File created: " + std::string(path));
    }
    else {
        update_status("Failed to create file: " + std::string(path));
    }
}

void delete_file_cb(Fl_Widget*, void*) {
    const char* path = path_input->value();
    if (strlen(path) == 0) {
        update_status("Please enter a file path");
        return;
    }

    if (fs.deleteFile(path)) {
        update_status("File deleted: " + std::string(path));
        if (current_file == path) {
            file_open = false;
            current_file.clear();
        }
    }
    else {
        update_status("Failed to delete file: " + std::string(path));
    }
}

void open_file_cb(Fl_Widget*, void*) {
    const char* path = path_input->value();
    if (strlen(path) == 0) {
        update_status("Please enter a file path");
        return;
    }

    if (fs.openFile(path)) {
        file_open = true;
        current_file = path;
        update_status("File opened: " + std::string(path));
    }
    else {
        update_status("Failed to open file: " + std::string(path));
    }
}

void close_file_cb(Fl_Widget*, void*) {
    if (!file_open) {
        update_status("No file is currently open");
        return;
    }

    if (fs.closeFile(current_file)) {
        update_status("File closed: " + current_file);
        file_open = false;
        current_file.clear();
    }
    else {
        update_status("Failed to close file: " + current_file);
    }
}

void write_file_cb(Fl_Widget*, void*) {
    if (!file_open) {
        update_status("Please open a file first");
        return;
    }

    const char* data = data_input->value();
    if (strlen(data) == 0) {
        update_status("Please enter data to write");
        return;
    }

    if (fs.writeFile(current_file, data)) {
        update_status("Data written to: " + current_file);
    }
    else {
        update_status("Failed to write to file: " + current_file);
    }
}

void read_file_cb(Fl_Widget*, void*) {
    if (!file_open) {
        update_status("Please open a file first");
        return;
    }

    std::string content = fs.readFile(current_file);
    if (!content.empty()) {
        update_content(content);
        update_status("File content read: " + current_file);
    }
    else {
        update_status("Failed to read file: " + current_file);
    }
}

void save_fs_cb(Fl_Widget*, void*) {
    Fl_Native_File_Chooser chooser;
    chooser.title("Save File System");
    chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    chooser.filter("FS Files\t*.fs");

    if (chooser.show() == 0) {
        std::string filename = chooser.filename();
        if (filename.find(".fs") == std::string::npos) {
            filename += ".fs";
        }
        fs.saveToDisk(filename);
        update_status("File system saved to: " + filename);
    }
}

void load_fs_cb(Fl_Widget*, void*) {
    Fl_Native_File_Chooser chooser;
    chooser.title("Load File System");
    chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);
    chooser.filter("FS Files\t*.fs");

    if (chooser.show() == 0) {
        std::string filename = chooser.filename();
        fs.loadFromDisk(filename);
        update_status("File system loaded from: " + filename);
        file_open = false;
        current_file.clear();
        clear_content();
    }
}

void help_cb(Fl_Widget*, void*) {
    std::string help_text =
        "Simple File System Help\n\n"
        "1. Format: Initialize the file system\n"
        "2. List: Show current directory contents\n"
        "3. Create Dir: Create a new directory\n"
        "4. Remove Dir: Delete an empty directory\n"
        "5. Change Dir: Navigate to a directory\n"
        "6. Create File: Create a new file\n"
        "7. Delete File: Delete a file\n"
        "8. Open File: Open a file for read/write\n"
        "9. Close File: Close the current file\n"
        "10. Write: Save data to open file\n"
        "11. Read: Display content of open file\n"
        "12. Save FS: Save entire file system to disk\n"
        "13. Load FS: Load file system from disk\n\n"
        "Note: Files must be opened before read/write operations";

    update_content(help_text);
    update_status("Displaying help information");
}

void create_ui(Fl_Window* window) {
    // 创建控件
    Fl_Button* format_btn = new Fl_Button(20, 20, 100, 30, "Format");
    Fl_Button* list_btn = new Fl_Button(130, 20, 100, 30, "List");

    // 状态输出
    status_output = new Fl_Output(290, 20, 280, 30, "Status:");
    status_output->color(FL_WHITE);

    // 内容显示
    content_output = new Fl_Multiline_Output(80, 70, 500, 250, "Content:");
    content_output->textsize(14);

    // 路径输入
    path_input = new Fl_Input(100, 330, 480, 30, "Path:");
    path_input->value("/");

    // 目录操作按钮
    Fl_Button* mkdir_btn = new Fl_Button(20, 370, 100, 30, "Create Dir");
    Fl_Button* rmdir_btn = new Fl_Button(130, 370, 100, 30, "Remove Dir");
    Fl_Button* chdir_btn = new Fl_Button(240, 370, 100, 30, "Change Dir");

    // 文件操作按钮
    Fl_Button* create_btn = new Fl_Button(350, 370, 100, 30, "Create File");
    Fl_Button* delete_btn = new Fl_Button(460, 370, 100, 30, "Delete File");

    // 文件IO操作
    Fl_Button* open_btn = new Fl_Button(20, 410, 100, 30, "Open File");
    Fl_Button* close_btn = new Fl_Button(130, 410, 100, 30, "Close File");
    Fl_Button* write_btn = new Fl_Button(240, 410, 100, 30, "Write");
    Fl_Button* read_btn = new Fl_Button(350, 410, 100, 30, "Read");

    // 数据输入
    data_input = new Fl_Input(100, 450, 460, 30, "Data:");

    // 系统操作
    Fl_Button* save_btn = new Fl_Button(20, 490, 100, 30, "Save FS");
    Fl_Button* load_btn = new Fl_Button(130, 490, 100, 30, "Load FS");
    Fl_Button* help_btn = new Fl_Button(350, 490, 100, 30, "Help");

    // 设置按钮颜色
    format_btn->color(FL_DARK_RED);
    save_btn->color(FL_DARK_GREEN);
    load_btn->color(FL_DARK_BLUE);
    help_btn->color(FL_DARK_CYAN);

    // 注册回调
    format_btn->callback(format_cb);
    list_btn->callback(list_cb);
    mkdir_btn->callback(mkdir_cb);
    rmdir_btn->callback(rmdir_cb);
    chdir_btn->callback(chdir_cb);
    create_btn->callback(create_file_cb);
    delete_btn->callback(delete_file_cb);
    open_btn->callback(open_file_cb);
    close_btn->callback(close_file_cb);
    write_btn->callback(write_file_cb);
    read_btn->callback(read_file_cb);
    save_btn->callback(save_fs_cb);
    load_btn->callback(load_fs_cb);
    help_btn->callback(help_cb);

    // 状态标签
    Fl_Box* status_box = new Fl_Box(20, 530, 560, 20);
    if (file_open) {
        status_box->label(("Current File: " + current_file).c_str());
    }
    else {
        status_box->label("No file open");
    }

    // 标题
    Fl_Box* title = new Fl_Box(200, 0, 200, 20, "Simple File System");
    title->labelfont(FL_BOLD);
    title->labelsize(16);
}

int main() {
    Fl_Window* window = new Fl_Window(600, 560, "Simple File System");
    window->color(FL_LIGHT2);

    create_ui(window);

    window->end();
    window->show();

    // 初始化状态
    update_status("File system ready. Click Format to initialize.");

    return Fl::run();
}