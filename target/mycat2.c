#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

void error_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// 获取文件大小
off_t get_file_size(const char *filename) {
    struct stat statbuf;
    if (stat(filename, &statbuf) == -1) {
        error_exit("stat");
    }
    return statbuf.st_size;
}

// 获取系统页面大小
size_t io_blocksize() {
    long pagesize = sysconf(_SC_PAGESIZE);
    if (pagesize == -1) {
        // 如果获取失败，使用默认值4096
        return 4096;
    }
    return (size_t)pagesize;
}

int main(int argc, char *argv[]) {
    // 检查命令行参数
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // 获取输入文件的大小和缓冲区大小
    off_t file_size = get_file_size(argv[1]);
    size_t block_size = io_blocksize();
    
    // 打开文件
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        error_exit("open");
    }

    // 动态分配缓冲区内存
    char *buffer = malloc(block_size);
    if (!buffer) {
        error_exit("malloc");
    }

    ssize_t bytes_read, bytes_written;
    off_t total_read = 0;

    // 读取并写入标准输出
    while (total_read < file_size) {
        size_t to_read = (size_t)(file_size - total_read);
        if (to_read > block_size) {
            to_read = block_size;
        }

        bytes_read = read(fd, buffer, to_read);
        if (bytes_read == -1) {
            error_exit("read");
        }
        
        if (bytes_read == 0) {
            break; // 文件读取完毕
        }

        bytes_written = write(STDOUT_FILENO, buffer, (size_t)bytes_read);
        if (bytes_written == -1) {
            error_exit("write");
        }
        
        if ((size_t)bytes_written != (size_t)bytes_read) {
            fprintf(stderr, "Short write\n");
            exit(EXIT_FAILURE);
        }

        total_read += bytes_read;
    }

    // 释放缓冲区
    free(buffer);

    // 关闭文件
    if (close(fd) == -1) {
        error_exit("close");
    }

    return 0;
}