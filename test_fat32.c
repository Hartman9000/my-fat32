#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "fat32.h"

#define OFT_SIZE 128

// 打印目录内容测试结果
void print_dir_content(struct FilesInfo* info) {
    if (info == NULL) {
        printf("  - 目录读取失败\n");
        return;
    }
    
    printf("  - 目录共包含 %d 个条目:\n", info->size);
    for (int i = 0; i < info->size; i++) {
        char name_buf[12] = {0};
        memcpy(name_buf, info->files[i].DIR_Name, 11);
        printf("    %d: 名称=\"%s\", 大小=%u 字节\n", 
               i, name_buf, info->files[i].DIR_FileSize);
    }
    
    // 释放内存
    free(info->files);
    free(info);
}

// 测试一个文件的读取，检查期望的内容
void test_file_content(int fd, char* expected_content, int length, int offset) {
    char* buffer = (char*)malloc(length + 1);
    memset(buffer, 0, length + 1);
    
    int bytes_read = fat_pread(fd, buffer, length, offset);
    printf("  - 读取 %d 字节: \"%s\"\n", bytes_read, buffer);
    
    if (bytes_read != length) {
        printf("  - 错误: 期望读取 %d 字节, 实际读取 %d 字节\n", length, bytes_read);
    } else if (strncmp(buffer, expected_content, length) != 0) {
        printf("  - 错误: 内容不匹配\n");
        printf("    期望: \"%s\"\n", expected_content);
        printf("    实际: \"%s\"\n", buffer);
    } else {
        printf("  - 成功: 内容匹配\n");
    }
    
    free(buffer);
}

int main() {
    printf("========== FAT32 API 测试 ==========\n\n");
    
    // 1. 测试 fat_mount
    printf("测试 1: 挂载 fat32.img\n");
    int mount_result = fat_mount("fat32.img");
    assert(mount_result == 0);
    printf("  - 挂载成功\n\n");
    
    // 2. 测试 fat_readdir 读取根目录
    printf("测试 2: 读取根目录\n");
    struct FilesInfo* root_dir = fat_readdir("/");
    print_dir_content(root_dir);
    printf("\n");
    
    // 3. 测试 fat_open 打开文件
    printf("测试 3: 打开文件 /exam_1.txt\n");
    int fd1 = fat_open("/exam_1.txt");
    if (fd1 < 0) {
        printf("  - 打开文件失败\n");
        return -1;
    }
    printf("  - 文件打开成功，文件描述符: %d\n\n", fd1);
    
    // 4. 测试 fat_pread 读取文件内容
    printf("测试 4: 读取 /exam_1.txt 文件内容\n");
    test_file_content(fd1, "The", 3, 0);
    printf("\n");
    
    // 6. 测试 fat_close 关闭文件
    printf("测试 6: 关闭文件 /exam_1.txt\n");
    int close_result = fat_close(fd1);
    assert(close_result == 0);
    printf("  - 文件关闭成功\n\n");
    
    // 7. 测试 fat_readdir 读取子目录
    printf("测试 7: 读取子目录 /DIR_1\n");
    struct FilesInfo* dir1 = fat_readdir("/DIR_1");
    print_dir_content(dir1);
    printf("\n");
    
    // 8. 测试打开子目录中的文件
    printf("测试 8: 打开文件 /DIR_1/111.txt\n");
    int fd2 = fat_open("/DIR_1/111.txt");
    if (fd2 < 0) {
        printf("  - 打开文件失败\n");
        return -1;
    }
    printf("  - 文件打开成功，文件描述符: %d\n\n", fd2);
    
    // 9. 测试读取子目录中的文件内容
    printf("测试 9: 读取 /DIR_1/111.txt 文件内容\n");
    test_file_content(fd2, "This", 4, 0);
    printf("\n");
    
    // 10. 测试嵌套目录操作
    printf("测试 10: 读取嵌套目录 /DIR_2/PEOPLE\n");
    struct FilesInfo* people_dir = fat_readdir("/DIR_2/PEOPLE");
    print_dir_content(people_dir);
    printf("\n");
    
    // 11. 测试打开和读取嵌套目录中的文件
    printf("测试 11: 打开并读取 /DIR_2/PEOPLE/EXAMPLE2.TXT\n");
    int fd3 = fat_open("/DIR_2/PEOPLE/EXAMPLE2.TXT");
    if (fd3 < 0) {
        printf("  - 打开文件失败\n");
        return -1;
    }
    printf("  - 文件打开成功，文件描述符: %d\n", fd3);
    test_file_content(fd3, "The con", 7, 0);
    fat_close(fd3);
    printf("\n");
    
    // 12. 测试打开空目录
    printf("测试 12: 读取空目录 /DIR_3\n");
    struct FilesInfo* dir3 = fat_readdir("/DIR_3");
    print_dir_content(dir3);
    printf("\n");
    
    // 13. 测试错误情况
    printf("测试 13: 错误情况测试\n");
    
    // 13.1 打开不存在的文件
    printf("  13.1: 打开不存在的文件\n");
    int fd_invalid = fat_open("/not_exist.txt");
    printf("    - 返回值: %d (期望: -1)\n", fd_invalid);
    
    // 13.2 读取目录作为文件
    printf("  13.2: 打开目录作为文件\n");
    int fd_dir = fat_open("/DIR_1");
    printf("    - 返回值: %d (期望: -1)\n", fd_dir);
    
    // 13.3 读取无效文件描述符
    printf("  13.3: 读取无效的文件描述符\n");
    char buf[10];
    int read_result = fat_pread(999, buf, 10, 0);
    printf("    - 返回值: %d (期望: -1)\n", read_result);
    
    // 13.4 关闭已关闭的文件
    printf("  13.4: 关闭已关闭的文件\n");
    int close_again = fat_close(fd1);
    printf("    - 返回值: %d (期望: -1)\n", close_again);
    
    // 13.5 文件偏移超出边界
    printf("  13.5: 文件偏移超出文件大小\n");
    int fd4 = fat_open("/exam_2.txt");
    if (fd4 >= 0) {
        int read_outofbound = fat_pread(fd4, buf, 10, 10000);
        printf("    - 返回值: %d (期望: 0)\n", read_outofbound);
        fat_close(fd4);
    } else {
        printf("    - 打开文件失败，无法测试\n");
    }


    // 13.7 打开太多文件（超过OFT_SIZE）
    printf("  13.7: 打开过多文件\n");
    int max_fds[OFT_SIZE + 1];
    int i = 0;
    for (i = 0; i < OFT_SIZE; i++) {
        max_fds[i] = fat_open("/exam_1.txt");
        if (max_fds[i] < 0) {
            printf("    - 在打开第%d个文件时失败\n", i+1);
            break;
        }
    }
    // 尝试再打开一个文件，应该失败
    if (i == OFT_SIZE) {
        max_fds[OFT_SIZE] = fat_open("/exam_1.txt");
        printf("    - 打开第%d个文件返回值: %d (期望: -1)\n", OFT_SIZE+1, max_fds[OFT_SIZE]);
    }
    // 关闭所有已打开的文件
    for (int j = 0; j < i; j++) {
        fat_close(max_fds[j]);
    }

    // 13.8 缓冲区为NULL
    printf("  13.8: 读取时缓冲区为NULL\n");
    int fd_null = fat_open("/exam_1.txt");
    if (fd_null >= 0) {
        int read_null = fat_pread(fd_null, NULL, 10, 0);
        printf("    - 返回值: %d (期望: -1)\n", read_null);
        fat_close(fd_null);
    } else {
        printf("    - 打开文件失败，无法测试\n");
    }

    // 13.9 读取长度为0
    printf("  13.9: 读取长度为0\n");
    int fd_zero = fat_open("/exam_1.txt");
    if (fd_zero >= 0) {
        char buffer[10];
        int read_zero = fat_pread(fd_zero, buffer, 0, 0);
        printf("    - 返回值: %d (期望: 0)\n", read_zero);
        fat_close(fd_zero);
    } else {
        printf("    - 打开文件失败，无法测试\n");
    }

    // 13.10 读取偏移为负数
    printf("  13.10: 读取偏移为负数\n");
    int fd_neg = fat_open("/exam_1.txt");
    if (fd_neg >= 0) {
        char buffer[10];
        int read_neg = fat_pread(fd_neg, buffer, 10, -5);
        printf("    - 返回值: %d (期望: -1)\n", read_neg);
        fat_close(fd_neg);
    } else {
        printf("    - 打开文件失败，无法测试\n");
    }

    // 13.11 读取长度为负数
    printf("  13.11: 读取长度为负数\n");
    int fd_neg_len = fat_open("/exam_1.txt");
    if (fd_neg_len >= 0) {
        char buffer[10];
        int read_neg_len = fat_pread(fd_neg_len, buffer, -5, 0);
        printf("    - 返回值: %d (期望: -1)\n", read_neg_len);
        fat_close(fd_neg_len);
    } else {
        printf("    - 打开文件失败，无法测试\n");
    }

    // 13.12 读取目录内容作为文件
    printf("  13.12: 读取目录内容\n");
    int fd_read_dir = fat_open("/DIR_1");  // 应返回-1
    if (fd_read_dir >= 0) {
        char buffer[100];
        int read_dir = fat_pread(fd_read_dir, buffer, 100, 0);
        printf("    - 能打开目录为文件: fd=%d, 读取返回: %d\n", fd_read_dir, read_dir);
        fat_close(fd_read_dir);
    } else {
        printf("    - 正确: 不能打开目录作为文件\n");
    }
    
    printf("\n测试完成！\n");
    return 0;
}