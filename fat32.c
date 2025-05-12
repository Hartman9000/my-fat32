#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h> 
#include "fat32.h"

struct Fat32BPB *hdr; // 指向 BPB 的数据
int mounted = -1; // 是否已挂载成功

// 挂载磁盘镜像
int fat_mount(const char *path) {
    // 只读模式打开磁盘镜像
    int fd = open(path, O_RDWR);
    if (fd < 0){
        // 打开失败
        return -1;
    }
    // 获取磁盘镜像的大小
    off_t size = lseek(fd, 0, SEEK_END);
    if (size == -1){
        // 获取失败
        return -1;
    }
    // 将磁盘镜像映射到内存
    hdr = (struct Fat32BPB *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (hdr == (void *)-1){
        // 映射失败
        return -1;
    }
    // 关闭文件
    close(fd);

    assert(hdr->Signature_word == 0xaa55); // MBR 的最后两个字节应该是 0x55 和 0xaa
    assert((hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec) == size / hdr->BPB_BytsPerSec / hdr->BPB_SecPerTrk * hdr->BPB_BytsPerSec * hdr->BPB_SecPerTrk); // 不相等表明文件的 FAT32 文件系统参数可能不正确

    // 打印 BPB 的部分信息
    printf("Some of the information about BPB is as follows\n");
    printf("OEM-ID \"%s\", \n", hdr->BS_oemName);
    printf("sectors/cluster %d, \n", hdr->BPB_SecPerClus);
    printf("bytes/sector %d, \n", hdr->BPB_BytsPerSec);
    printf("sectors %d, \n", hdr->BPB_TotSec32);
    printf("sectors/FAT %d, \n", hdr->BPB_FATSz32);
    printf("FATs %d, \n", hdr->BPB_NumFATs);
    printf("reserved sectors %d, \n", hdr->BPB_RsvdSecCnt);

    // 挂载成功
    mounted = 0;
    return 0;
}

// 打开文件
int fat_open(const char *path) {
    // TODO
    return -1;
}

// 关闭文件
int fat_close(int fd) {
    // TODO
    return -1;
}

// 读取普通文件内容
int fat_pread(int fd, void *buffer, int count, int offset) {
    // TODO
    return -1;
}

// 读取目录文件内容 (目录项)
struct FilesInfo* fat_readdir(const char *path) {
    // TODO
    return NULL;
}
