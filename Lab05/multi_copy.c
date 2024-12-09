#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

// 递归拷贝目录函数声明
void copy_directory(const char *src, const char *dst);
// 文件拷贝函数
void copy_file(const char *src, const char *dst) {
    int source = open(src, O_RDONLY);
    if (source == -1) {
        perror("Error opening source file");
        return;
    }
    int destination = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (destination == -1) {
        perror("Error opening destination file");
        close(source);
        return;
    }
    char buffer[4096];
    ssize_t bytesRead;
    while ((bytesRead = read(source, buffer, sizeof(buffer))) > 0) {
        if (write(destination, buffer, bytesRead)!= bytesRead) {
            perror("Error writing to destination file");
        }
    }
    close(source);
    close(destination);
}

// 递归拷贝目录函数（子进程执行）
void copy_subdirectory(const char *src, const char *dst) {
    copy_directory(src, dst);
}

// 递归拷贝目录函数（父进程调用）
void copy_directory(const char *src, const char *dst) {
    struct stat st;
    if (stat(src, &st)!= 0) {
        perror("Error stating source directory");
        return;
    }

    // 创建目标目录
    if (mkdir(dst, st.st_mode)!= 0) {
        perror("Error creating destination directory");
        return;
    }

    DIR *dir = opendir(src);
    if (dir == NULL) {
        perror("Error opening source directory");
        return;
    }

    struct dirent *entry;
    int subdir_count = 0;
    while ((entry = readdir(dir))!= NULL) {
        // 跳过 "." 和 ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 构建源和目标路径
        char src_path[1024];
        char dst_path[1024];
        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, entry->d_name);
        if (stat(src_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                // 记录子目录名称和增加总数
                printf("Found subdirectory: %s\n", src_path);
                subdir_count++;
            } else if (S_ISREG(st.st_mode)) {
                // 拷贝文件
                copy_file(src_path, dst_path);
            } else if (S_ISLNK(st.st_mode)) {
                // 处理链接文件
                printf("Found symbolic link: %s\n", src_path);
            }
        }
    }
    closedir(dir);

    // 创建子进程来拷贝子目录
    pid_t *child_pids = malloc(subdir_count * sizeof(pid_t));
    int current_subdir = 0;
    dir = opendir(src);
    while ((entry = readdir(dir))!= NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char src_path[1024];
        char dst_path[1024];
        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, entry->d_name);
        if (stat(src_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            pid_t pid = fork();
            if (pid == 0) {
                // 子进程
                printf("Child process %d copying subdirectory: %s\n", getpid(), src_path);
                copy_subdirectory(src_path, dst_path);
                exit(0);
            } else if (pid > 0) {
                // 父进程
                child_pids[current_subdir++] = pid;
            } else {
                perror("Fork failed");
            }
        }
    }
    closedir(dir);

    // 等待所有子进程完成
    for (int i = 0; i < subdir_count; i++) {
        waitpid(child_pids[i], NULL, 0);
    }
    free(child_pids);
}

int main(int argc, char *argv[]) {
    if (argc!= 3) {
        fprintf(stderr, "Usage: %s <source_directory> <destination_directory>\n", argv[0]);
        return 1;
    }
    copy_directory(argv[1], argv[2]);
    return 0;
}