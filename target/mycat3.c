#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdint.h>

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

// 分配内存页对齐的内存
char* align_alloc(size_t size) {
    size_t pagesize = io_blocksize();
    size_t total_size = size + pagesize;
    
    // 使用 mmap 分配内存，确保对齐到内存页边界
    void *ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, 
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (ptr == MAP_FAILED) {
        error_exit("mmap");
    }
    
    // 计算下一个内存页边界地址
    char *aligned_ptr = (char *)(((uintptr_t)ptr + pagesize) & ~(pagesize - 1));
    // 存储原始指针以便释放
    *((void **)(aligned_ptr - sizeof(void *))) = ptr;
    
    return aligned_ptr;
}

// 释放内存页对齐分配的内存
void align_free(void* ptr) {
    if (ptr == NULL) {
        return;
    }
    
    size_t pagesize = io_blocksize();
    // 获取原始mmap分配的地址
    void *original_ptr = *((void **)((char *)ptr - sizeof(void *)));
    
    // 计算实际分配的大小（从原始指针到当前指针的距离）
    size_t actual_size = (char *)ptr - (char *)original_ptr;
    // 我们之前分配的最小是pagesize，所以至少有一个页面大小
    // 实际使用中应该记录分配的总大小，这里为了简化示例使用最小值
    size_t total_size = pagesize;
    
    if (munmap(original_ptr, total_size) == -1) {
        error_exit("munmap");
    }
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

    // 分配内存页对齐的缓冲区
    char *buffer = align_alloc(block_size);
    if (!buffer) {
        error_exit("align_alloc");
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

    // 释放对齐的缓冲区
    align_free(buffer);

    // 关闭文件
    if (close(fd) == -1) {
        error_exit("close");
    }

    return 0;
}