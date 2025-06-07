#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

void error_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    // 检查命令行参数
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // 打开文件
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        error_exit("open");
    }

    char c;
    ssize_t bytes_read, bytes_written;

    // 逐个字符读取并写入标准输出
    while ((bytes_read = read(fd, &c, 1)) > 0) {
        bytes_written = write(STDOUT_FILENO, &c, 1);
        if (bytes_written == -1) {
            error_exit("write");
        }
        if (bytes_written != 1) {
            fprintf(stderr, "Short write\n");
            exit(EXIT_FAILURE);
        }
    }

    if (bytes_read == -1) {
        error_exit("read");
    }

    // 关闭文件
    if (close(fd) == -1) {
        error_exit("close");
    }

    return 0;
}