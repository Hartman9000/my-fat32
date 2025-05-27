#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h> 
#include "fat32.h"

#define OFT_SIZE 128
#define MAX_PATH_LEN 128
#define FAT_MUSK 0x0FFFFFFF
#define MAX_FILESINFO_SIZE 1024

enum EntryType {
    TYPE_FILE = 0x01,
    TYPE_DIR = 0x02,
    TYPE_VOLUME = 0x04,
    TYPE_ANY = 0xFF
};

typedef struct OFTNode{
    int fd;
    int first_clus;
    int offset;
    int file_size;
    struct OFTNode *next;
} OFTNode;

OFTNode *oft_head = NULL;
int opened_files = 0;
int next_fd_g = 3;

static void reset_oft_and_fd_state() {
    OFTNode *current = oft_head;
    OFTNode *next_node;
    while (current != NULL) {
        next_node = current->next;
        free(current);
        current = next_node;
    }
    oft_head = NULL;
    opened_files = 0;
    next_fd_g = 3;
}

// 参数: first_clus - 文件的第一个簇号
// 返回: 成功时返回分配的文件描述符，失败时返回-1
int oft_add(int first_clus, int file_size) {
    // 检查是否已达到最大打开文件数
    if (opened_files >= OFT_SIZE) {
        return -1;  // 打开文件数量已达上限
    }
    
    // 分配新节点
    OFTNode *new_node = (OFTNode *)malloc(sizeof(OFTNode));
    if (new_node == NULL) {
        return -1;  // 内存分配失败
    }
    
    // 分配文件描述符（简单递增方式）
    int fd = next_fd_g++;
    
    // 初始化新节点
    new_node->fd = fd;
    new_node->first_clus = first_clus;
    new_node->offset = 0;  // 初始偏移量为0
    new_node->file_size = file_size;
    new_node->next = NULL;
    
    // 将节点添加到链表头部
    if (oft_head == NULL) {
        oft_head = new_node;
    } else {
        new_node->next = oft_head;
        oft_head = new_node;
    }
    // 更新打开文件计数
    opened_files++;
    return fd;
}

// 从OFT中查找指定文件描述符的节点
// 参数: fd - 要查找的文件描述符
// 返回: 成功时返回节点指针，失败时返回NULL
OFTNode* oft_find(int fd) {
    OFTNode *current = oft_head;
    
    while (current != NULL) {
        if (current->fd == fd) {
            return current;  // 找到匹配的节点
        }
        current = current->next;
    }
    
    return NULL;  // 未找到匹配的节点
}

// 从OFT中删除指定文件描述符的节点
// 参数: fd - 要删除的文件描述符
// 返回: 成功时返回0，失败时返回-1
int oft_remove(int fd) {
    if (oft_head == NULL) {
        return -1;  // 链表为空
    }
    
    OFTNode *current = oft_head;
    OFTNode *prev = NULL;
    
    // 查找要删除的节点
    while (current != NULL && current->fd != fd) {
        prev = current;
        current = current->next;
    }
    
    if (current == NULL) {
        return -1;  // 未找到匹配的节点
    }
    
    // 从链表中移除节点
    if (prev == NULL) {
        // 删除的是头节点
        oft_head = current->next;
    } else {
        // 删除的是中间或尾节点
        prev->next = current->next;
    }
    
    // 释放节点内存
    free(current);
    
    // 更新打开文件计数
    opened_files--;
    
    return 0;  // 删除成功
}

typedef struct DirEntry Dir_entry;

struct Fat32BPB *hdr = NULL; // 指向 BPB 的数据
int mounted = -1; // 是否已挂载成功
int fat_first_sec;
int data_first_sec;
int bytes_per_clus;
int current_mapped_size = 0;

// 将数据区簇号转换为对应的第一个扇区号
static inline int clus_to_sec(int clus_d) {
    if (clus_d < 2) {
        return -1;
    }
    return data_first_sec + (clus_d - 2) * hdr->BPB_SecPerClus;
}

// 给定扇区号和扇区内偏移，返回对应指针
static inline uint8_t *ptr_at_sec(int sec_num, int sec_offset) {
    return (uint8_t *)hdr + sec_num * hdr->BPB_BytsPerSec + sec_offset;
}

// 给定数据区簇号和簇内扇区偏移，返回对应指针
static inline uint8_t *ptr_at_clus(int clus_d, int sec) {
    if (clus_d < 2) {
        return NULL;
    }
    // 确保扇区偏移在有效范围内
    if (sec >= hdr->BPB_SecPerClus) {
        return NULL;
    }
    
    int first_sec = clus_to_sec(clus_d);
    return ptr_at_sec(first_sec + sec, 0);
}

//标准化文件名
static inline char *standardize_filename(char *filename) {
    char *std_filename = (char *)malloc(12 * sizeof(char));
    if (std_filename == NULL) return NULL;
    std_filename[11] = '\0';
    // 初始化为全空格
    memset(std_filename, ' ', 11);
    // 文件名部分 (前8个字符)
    int i = 0;
    int j = 0;
    // 寻找'.'或结束符前的字符作为文件名
    while(filename[i] != '\0' && filename[i] != '.' && j < 8) {
        // 转换为大写
        std_filename[j++] = toupper(filename[i++]);
    }
    // 找到扩展名的位置(如果有的话)
    while(filename[i] != '\0' && filename[i] != '.') {
        i++;
    }
    // 处理扩展名
    if(filename[i] == '.') {
        i++; // 跳过'.'
        j = 8; // 扩展名从第8个位置开始
        // 最多取3个字符作为扩展名
        while(filename[i] != '\0' && j < 11) {
            std_filename[j++] = toupper(filename[i++]);
        }
    }
    return std_filename;
}

//给定当前簇号，返回下一簇号，异常返回-2，结尾返回-1
static inline int next_clus(int current_clus_d) {
    if (hdr == NULL) return -2;
    if (current_clus_d < 0 || current_clus_d >= (hdr->BPB_FATSz32 * hdr->BPB_BytsPerSec / 4)) {
         return -2; // Index out of FAT bounds or invalid
    }
    
    // 计算FAT表的指针位置
    uint32_t *fat_table = (uint32_t *)ptr_at_sec(fat_first_sec, 0);
    if (fat_table == NULL) return -2;
    
    // 获取FAT表中对应簇的条目值
    uint32_t fat_value = fat_table[current_clus_d] & FAT_MUSK;
    
    // 解析FAT条目值
    if (fat_value >= 0x0FFFFFF8) return -1;      // EOC (End Of Chain)
    else if (fat_value == 0x0FFFFFF7) return -2; // Bad cluster
    else if (fat_value == 0x00000000 && current_clus_d !=0 && current_clus_d !=1) return -2;
    else return (int)fat_value;                  // 下一个簇号
}

//接受当前目录文件的第一个簇号和目标文件名
//返回目标文件的第一簇号，并修改filesize
int locate_first_cluster(int current_dir_clus, char *filename, int *filesize, int expected_type) {
    if (hdr == NULL) return -1;
    char *std_filename = standardize_filename(filename);
    if (std_filename == NULL) return -1;

    Dir_entry *dir_entry_base;
    int original_search_start_clus = current_dir_clus;

    while(current_dir_clus >= 2) { // Iterate through clusters of the directory
        dir_entry_base = (Dir_entry *)(ptr_at_clus(current_dir_clus, 0));
        if (dir_entry_base == NULL) {
            free(std_filename);
            return -1; // Error accessing cluster
        }
        
        int entries_per_cluster = bytes_per_clus / sizeof(Dir_entry);
        for (int i = 0; i < entries_per_cluster; i++) {
            Dir_entry *current_entry = dir_entry_base + i;
            uint8_t first_byte = current_entry->DIR_Name[0];

            if (first_byte == 0x00) { // End of directory marker
                free(std_filename);
                return -1; // Not found
            }
            if (first_byte == 0xE5 || (current_entry->DIR_Attr & LONG_NAME_MASK) == LONG_NAME) {
                continue; // Deleted or LFN entry
            }
            if (current_entry->DIR_Attr & VOLUME_ID) { // Skip Volume ID unless specifically looking for it
                if (!(expected_type & TYPE_VOLUME)) {
                    continue;
                }
            }

            if (memcmp(std_filename, current_entry->DIR_Name, 11) == 0) { // Name matches
                int entry_actual_type = 0;
                if (current_entry->DIR_Attr & DIRECTORY) {
                    entry_actual_type = TYPE_DIR;
                } else if (current_entry->DIR_Attr & VOLUME_ID) {
                    entry_actual_type = TYPE_VOLUME;
                } else {
                    entry_actual_type = TYPE_FILE;
                }

                if (!(entry_actual_type & expected_type)) {
                    free(std_filename);
                    return -1; // Indicate type mismatch failure
                }

                *filesize = (int)current_entry->DIR_FileSize;
                uint32_t target_clus = ((uint32_t)current_entry->DIR_FstClusHI << 16) | ((uint32_t)current_entry->DIR_FstClusLO);
                
                if (target_clus == 0 && (entry_actual_type & TYPE_DIR)) { // "." entry in root dir might point to cluster 0
                    target_clus = hdr->BPB_RootClus; // Correctly point to root cluster
                }


                free(std_filename);
                return (target_clus < 2 && !(entry_actual_type & TYPE_FILE && *filesize == 0)) ? -1 : (int)target_clus;
            }
        }
        current_dir_clus = next_clus(current_dir_clus); // Move to the next cluster of this directory
    }
    
    free(std_filename);
    return -1; // Not found or end of directory chain
}

// 拆分路径字符串为多个组件
// path - 要拆分的路径
// components - 用于存储路径组件的字符串数组
// 返回：组件数量
int split_path(const char *path, char **components) {
    if (path == NULL || components == NULL) {
        return 0;
    }
    
    char *path_copy = strdup(path);
    if (path_copy == NULL) {
        return 0; // Malloc failure
    }
    
    int count = 0;
    char *token;
    char *current_pos = path_copy;

    // Skip leading slashes
    while (*current_pos == '/') {
        current_pos++;
    }
    // If path was only slashes (e.g. "///"), current_pos is now at '\0'
    if (*current_pos == '\0' && path[0] == '/') { // Path was effectively "/"
        free(path_copy);
        return 0; // Root itself has no components to split
    }
    
    token = strtok(current_pos, "/");
    while (token != NULL && count < MAX_PATH_LEN) {
        components[count] = strdup(token);
        if (components[count] == NULL) { // strdup failed
            for (int i=0; i<count; ++i) free(components[i]); // Clean up already duplicated tokens
            count = -1; // Indicate error
            break;
        }
        count++;
        token = strtok(NULL, "/");
    }
    
    free(path_copy);
    return count;
}

// 挂载磁盘镜像
int fat_mount(const char *path) {
    if (mounted == 0) { // A file system is already mounted
        if (hdr != NULL && hdr != (void *)-1) { // Check if hdr is a valid mmap pointer
            if (current_mapped_size > 0) {
                munmap(hdr, current_mapped_size);
            }
        }
        reset_oft_and_fd_state(); // Clear OFT and reset FD counter
        hdr = NULL;
        current_mapped_size = 0;
    }
    mounted = -1; // Mark as not mounted or in process
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
    current_mapped_size = size;
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
    fat_first_sec = hdr->BPB_RsvdSecCnt;
    data_first_sec = hdr->BPB_RsvdSecCnt + hdr->BPB_FATSz32 * hdr->BPB_NumFATs;
    bytes_per_clus = hdr->BPB_BytsPerSec * hdr->BPB_SecPerClus;
    return 0;
}

int fat_open(const char *path) {
    if (mounted != 0) return -1;
    if (opened_files == OFT_SIZE) return -1;

    char **path_files = (char **)malloc(MAX_PATH_LEN * sizeof(char *));
    if (path_files == NULL) return -1;
    int files_count = split_path(path, path_files);
    // printf("DEBUG: open path count:%d\n", files_count);
    // for (int i = 0; i < files_count; i++) {
    //     printf("DEBUG:\t\t[%d]: %s\n", i, path_files[i]);
    // }

    if (files_count <= 0) {
        free(path_files);
        return -1;
    }

    int current_file_clus = hdr->BPB_RootClus;
    int file_size = 0;

    // 对路径中的中间组件，只接受目录类型
    for (int i = 0; i < files_count - 1; i++) {
        current_file_clus = locate_first_cluster(current_file_clus, path_files[i], &file_size, TYPE_DIR);
        if (current_file_clus < 2) {
            // 清理资源
            for (int j = 0; j < files_count; j++) {
                free(path_files[j]);
            }
            free(path_files);
            return -1;
        }
    }

    // 对最后一个组件，只接受文件类型
    current_file_clus = locate_first_cluster(current_file_clus, path_files[files_count-1], &file_size, TYPE_FILE);
    if (current_file_clus < 2) {
        // 清理资源
        for (int j = 0; j < files_count; j++) {
            free(path_files[j]);
        }
        free(path_files);
        return -1;
    }

    for (int i = 0; i < files_count; i++) {
        free(path_files[i]);
    }
    free(path_files);

    return oft_add(current_file_clus, file_size);
}

// 关闭文件
int fat_close(int fd) {
    if (mounted != 0) return -1; 
    return oft_remove(fd);
}

int fat_pread(int fd, void *buffer, int count, int offset) {
    if (mounted != 0) return -1;
    // 允许 buffer 为 NULL 当 count 为 0 时, 但如果 count > 0 则 buffer 不能为空
    if ((buffer == NULL && count > 0) || count < 0 || offset < 0) {
        return -1;
    }

    OFTNode *fileinfo = oft_find(fd);
    if (fileinfo == NULL) return -1;

    int file_size = fileinfo->file_size;
    
    // 如果偏移量超出或等于文件大小，或者请求读取0字节，则返回0
    if (offset >= file_size || count == 0) {
        return 0;
    }

    // 如果请求读取的字节数超出了从偏移量开始到文件末尾的范围，
    // 则调整读取字节数为实际可读的字节数
    if (offset + count > file_size) {
        count = file_size - offset;
    }
    // 经过调整后，如果 count 变为0或负数（理论上不应为负），则返回0
    if (count <= 0) {
        return 0;
    }

    int total_bytes_read = 0; // 用于累计实际读取的字节数
    int current_clus = fileinfo->first_clus;

    // 处理0字节文件的情况，其簇号可能为0
    if (file_size == 0) { // 对于0字节文件，簇号可以是0
        return 0; // 没有数据可读
    }
    // 对于非0字节文件，起始簇号必须有效
    if (current_clus < 2) {
        return -1; // 无效的起始簇号
    }

    // 1. 定位到读取操作开始的簇
    int offset_in_clus = offset; // 当前处理的文件内绝对偏移
    while (offset_in_clus >= bytes_per_clus) {
        current_clus = next_clus(current_clus);
        if (current_clus < 2) { // 如果在到达目标偏移前遇到EOC或坏簇
            return total_bytes_read; // 返回目前为止读取到的字节数（可能为0）
        }
        offset_in_clus -= bytes_per_clus;
    }
    // 此处, current_clus 是读取开始的簇, offset_in_clus 是在该簇内的偏移量

    // 2. 从定位到的簇开始读取数据
    int bytes_remaining = count; // 还需要读取的字节数
    uint8_t *current_buffer_ptr = (uint8_t *)buffer;

    while (bytes_remaining > 0 && current_clus >= 2) {
        uint8_t *clus_start_ptr = ptr_at_clus(current_clus, 0);
        if (clus_start_ptr == NULL) {
            break; // 获取簇数据失败，停止读取
        }

        // 计算从当前簇的哪个位置开始复制，以及能复制多少字节
        int start_offset = offset_in_clus; // 仅在第一个读取的簇中，此值可能非0
        offset_in_clus = 0; // 后续簇都从簇首开始计算

        int bytes_available_in_cluster = bytes_per_clus - start_offset;
        int bytes_to_copy = bytes_remaining;

        if (bytes_to_copy > bytes_available_in_cluster) {
            bytes_to_copy = bytes_available_in_cluster;
        }

        memcpy(current_buffer_ptr, clus_start_ptr + start_offset, bytes_to_copy);

        current_buffer_ptr += bytes_to_copy;
        bytes_remaining -= bytes_to_copy;
        total_bytes_read += bytes_to_copy;

        if (bytes_remaining > 0) {
            current_clus = next_clus(current_clus);
        }
    }

    return total_bytes_read; // 返回实际读取的总字节数
}

struct FilesInfo* fat_readdir(const char *path) {
    if (mounted != 0) return NULL;
    //找到目标目录的首簇
    int target_dir_clus = hdr->BPB_RootClus;
    if (strcmp(path, "/") != 0 && strlen(path) > 0) {
        char **path_dirs = (char **)malloc(MAX_PATH_LEN * sizeof(char *));
        if (path_dirs == NULL) return NULL;
        int dirs_count = split_path(path, path_dirs);
        if (dirs_count <= 0) {
            free(path_dirs);
            return NULL;
        }

        int temp = 0;
        for (int i = 0; i < dirs_count; i++) {
            target_dir_clus = locate_first_cluster(target_dir_clus, path_dirs[i], &temp, TYPE_DIR);
            if (target_dir_clus < 2) {
                for (int j = 0; j < dirs_count; j++) {
                    if (path_dirs[j]) free(path_dirs[j]);
                }
                free(path_dirs);
                return NULL;
            }
        }

        // 清理资源
        for (int i = 0; i < dirs_count; i++) free(path_dirs[i]);
        free(path_dirs);
    }
    if (target_dir_clus < 2) return NULL;

    struct FileInfo *file_info = (struct FileInfo *)malloc(MAX_FILESINFO_SIZE * sizeof(struct FileInfo));
    if (file_info == NULL) return NULL;

    //遍历目录
    Dir_entry *dir_entry;
    int size = 0;
    int dir_ended = 0;

    while (target_dir_clus >= 2 && !dir_ended) {
        dir_entry = (Dir_entry *)(ptr_at_clus(target_dir_clus, 0));
        if (dir_entry == NULL) {
            free(file_info);
            return NULL;
        }

        int entrys_per_clus = bytes_per_clus / sizeof(Dir_entry);
        for (int i = 0; i < entrys_per_clus; i++) {
            if (size >= MAX_FILESINFO_SIZE) {
                dir_ended = 1;
                break;
            }

            uint8_t first_byte = *(uint8_t *)dir_entry;
            if (first_byte == 0x00) {
                dir_ended = 1;
                break;
            }
            else if (first_byte == 0xE5 || (dir_entry->DIR_Attr & LONG_NAME_MASK) == LONG_NAME || (dir_entry->DIR_Attr & VOLUME_ID)) {
                dir_entry++;
                continue;;
            }

            file_info[size].DIR_FileSize = dir_entry->DIR_FileSize;
            memcpy((void *)file_info[size].DIR_Name, (void *)dir_entry->DIR_Name, 11);
            size++;

            dir_entry++;
        }
        if (dir_ended || size >= MAX_FILESINFO_SIZE) break;
        target_dir_clus = next_clus(target_dir_clus);
    }
    struct FilesInfo *filesinfo_package = (struct FilesInfo *)malloc(sizeof(struct FilesInfo));
    if (filesinfo_package == NULL) {
        free(file_info);;
        return NULL;
    }
    filesinfo_package->files = file_info;
    filesinfo_package->size = size;
    return filesinfo_package;
}
