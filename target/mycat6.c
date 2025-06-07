#define _GNU_SOURCE

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdint.h>
#include <limits.h>
#include <sys/statvfs.h>
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

// 获取文件所在文件系统的块大小
size_t get_fs_blocksize(const char *filename) {
    struct statvfs statbuf;
    if (statvfs(filename, &statbuf) == -1) {
        // 如果获取失败，返回一个合理的默认值512字节
        // 512是传统磁盘块大小，也是许多文件系统的基本单位
        return 512;
    }
    
    // statvfs中的f_bsize表示文件系统基本块大小
    // f_frsize为根本块大小，如果可用的话更推荐使用
    if (statbuf.f_frsize > 0) {
        return statbuf.f_frsize;
    } else {
        return statbuf.f_bsize;
    }
}

// 获取系统页面大小
// 获取最佳缓冲区大小（根据实验结果）
size_t io_blocksize(const char *filename) {
    (void)filename; // 不再使用文件信息
    return 32768;   // 固定使用实验得出的最佳值：32KB
}

// 分配内存页对齐的内存
char* align_alloc(size_t size) {
    size_t pagesize = getpagesize();
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
    
    size_t pagesize = getpagesize();
    // 获取原始mmap分配的地址
    void *original_ptr = *((void **)((char *)ptr - sizeof(void *)));
    
    // 计算实际分配的大小（从原始指针到当前指针的距离）
    size_t actual_size = (char *)ptr - (char *)original_ptr;
    // 实际使用中应该记录分配的总大小，这里为了简化示例使用最小值
    size_t total_size = pagesize;
    
    if (munmap(original_ptr, pagesize) == -1) {
        error_exit("munmap");
    }
}

int main(int argc, char *argv[]) {
    // 检查命令行参数
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *filename = argv[1];
    
    // 获取文件大小和缓冲区大小
    off_t file_size = get_file_size(filename);
    size_t block_size = io_blocksize(filename);
    
    // 打开文件
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        error_exit("open");
    }

    // 使用 posix_fadvise 告诉内核我们是顺序读取
    if (posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL) != 0) {
        // 可以选择忽略错误或打印警告
        fprintf(stderr, "Warning: posix_fadvise failed\n");
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