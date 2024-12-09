#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define MAX_THREADS 4

typedef struct {
    char *src;
    char *dst;
} thread_args;

void *copy_file(void *args) {
    thread_args *targs = (thread_args *)args;
    int src_fd, dst_fd;
    char buffer[BUFFER_SIZE];
    ssize_t bytes;

    src_fd = open(targs->src, O_RDONLY);
    if (src_fd < 0) {
        perror("Error opening source file");
        return NULL;
    }

    dst_fd = open(targs->dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
        perror("Error creating destination file");
        close(src_fd);
        return NULL;
    }

    while ((bytes = read(src_fd, buffer, BUFFER_SIZE)) > 0) {
        if (write(dst_fd, buffer, bytes) != bytes) {
            perror("Error writing to destination file");
            close(src_fd);
            close(dst_fd);
            return NULL;
        }
    }

    if (bytes < 0) {
        perror("Error reading source file");
        close(src_fd);
        close(dst_fd);
        return NULL;
    }

    close(src_fd);
    close(dst_fd);
    printf("Copied file from %s to %s\n", targs->src, targs->dst);
    free(targs->src);
    free(targs->dst);
    free(targs);
    return NULL;
}

int create_directory(const char *path) {
    char tmp[256];
    char *p;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);

    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0777) != 0 && errno != EEXIST) {
                perror("mkdir failed");
                return -1;
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, 0777) != 0 && errno != EEXIST) {
        perror("mkdir failed");
        return -1;
    }

    return 0;
}

void *copy_directory(void *args) {
    thread_args *targs = (thread_args *)args;
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char src_path[1024];
    char dst_path[1024];
    pthread_t threads[MAX_THREADS];
    int thread_count = 0;

    if (!(dir = opendir(targs->src))) {
        perror("Failed to open directory");
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(src_path, sizeof(src_path), "%s/%s", targs->src, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", targs->dst, entry->d_name);

        if (stat(src_path, &statbuf) == -1) {
            perror("Failed to stat file");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            if (create_directory(dst_path) == -1) {
                continue;
            }
            thread_args *new_args = malloc(sizeof(thread_args));
            new_args->src = strdup(src_path);
            new_args->dst = strdup(dst_path);
            if (pthread_create(&threads[thread_count], NULL, copy_directory, new_args) != 0) {
                perror("Failed to create thread");
                free(new_args->src);
                free(new_args->dst);
                free(new_args);
            } else {
                thread_count++;
                if (thread_count >= MAX_THREADS) {
                    for (int i = 0; i < MAX_THREADS; i++) {
                        pthread_join(threads[i], NULL);
                    }
                    thread_count = 0;
                }
            }
        } else {
            thread_args *new_args = malloc(sizeof(thread_args));
            new_args->src = strdup(src_path);
            new_args->dst = strdup(dst_path);
            if (pthread_create(&threads[thread_count], NULL, copy_file, new_args) != 0) {
                perror("Failed to create thread");
                free(new_args->src);
                free(new_args->dst);
                free(new_args);
            } else {
                thread_count++;
                if (thread_count >= MAX_THREADS) {
                    for (int i = 0; i < MAX_THREADS; i++) {
                        pthread_join(threads[i], NULL);
                    }
                    thread_count = 0;
                }
            }
        }
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    closedir(dir);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source> <destination>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    thread_args *args = malloc(sizeof(thread_args));
    args->src = strdup(argv[1]);
    args->dst = strdup(argv[2]);

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, copy_directory, args) != 0) {
        perror("Failed to create thread");
        free(args->src);
        free(args->dst);
        free(args);
        exit(EXIT_FAILURE);
    }

    pthread_join(thread_id, NULL);

    printf("Directory copied successfully.\n");
    return 0;
}