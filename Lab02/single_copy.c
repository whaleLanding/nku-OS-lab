#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

void copyDir(const char *source, const char *destination) {
    struct stat sourceStat;
    if (stat(source, &sourceStat)!= 0) {
        perror("Error getting source directory info");
        return;
    }
    if (mkdir(destination, sourceStat.st_mode)!= 0) {
        perror("Error creating destination directory");
        return;
    }
    DIR *dir = opendir(source);
    if (dir == NULL) {
        perror("Error opening source directory");
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir))!= NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char sourcePath[1024];
        char destinationPath[1024];
        snprintf(sourcePath, sizeof(sourcePath), "%s/%s", source, entry->d_name);
        snprintf(destinationPath, sizeof(destinationPath), "%s/%s", destination, entry->d_name);
        struct stat entryStat;
        if (stat(sourcePath, &entryStat) == 0) {
            if (S_ISDIR(entryStat.st_mode)) {
                copyDir(sourcePath, destinationPath);
            } else if (S_ISREG(entryStat.st_mode)) {
                copyFile(sourcePath, destinationPath);
            }
        }
    }
    closedir(dir);
}

void copyFile(const char *sourceFilePath, const char *destinationFilePath) {
    int source = open(sourceFilePath, O_RDONLY);
    if (source == -1) {
        perror("Error opening source file");
        return;
    }
    int destination = open(destinationFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
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

int main(int argc, char *argv[]) {
    if (argc!= 3) {
        fprintf(stderr, "Usage: %s <source_directory> <destination_directory>\n", argv[0]);
        return 1;
    }
    copyDir(argv[1], argv[2]);
    return 0;
}