#ifndef FAT_H_
#define FAT_H_

#include <stdint.h>

// FAT32 文件系统中 BPB (BIOS Parameter Block) 结构体
// 具体含义如下：
// BS_jmpBoot[3]      : 跳转指令，启动代码的前3字节
// BS_oemName[8]      : OEM 名称和版本
// BPB_BytsPerSec     : 每扇区字节数（通常为512）
// BPB_SecPerClus     : 每簇扇区数
// BPB_RsvdSecCnt     : 保留扇区数（引导扇区数）
// BPB_NumFATs        : FAT 表的数量（通常为2）
// BPB_rootEntCnt     : 根目录最大条目数（FAT32中应为0）
// BPB_totSec16       : 总扇区数（小于65536时使用，否则为0）
// BPB_media          : 媒体描述符
// BPB_FATSz16        : 每个FAT表的扇区数（FAT32中应为0）
// BPB_SecPerTrk      : 每磁道扇区数（用于CHS寻址）
// BPB_NumHeads       : 磁头数（用于CHS寻址）
// BPB_HiddSec        : 隐藏扇区数（分区前的扇区数）
// BPB_TotSec32       : 总扇区数（大于65535时使用）
// BPB_FATSz32        : 每个FAT表的扇区数（FAT32专用）
// BPB_ExtFlags       : 扩展标志（FAT表同步等信息）
// BPB_FSVer          : 文件系统版本号
// BPB_RootClus       : 根目录起始簇号
// BPB_FSInfo         : FSInfo 结构扇区号
// BPB_bkBootSec      : 备份引导扇区号
// BPB_reserved[12]   : 保留，未使用
// BS_DrvNum          : 物理驱动器号
// BS_Reserved1       : 保留，未使用
// BS_BootSig         : 扩展引导标志（0x29表示存在扩展引导记录）
// BS_VolID           : 卷序列号
// BS_VolLab[11]      : 卷标（ASCII）
// BS_FileSysTye[8]   : 文件系统类型（ASCII，通常为"FAT32   "）
// __padding_1[420]   : 填充，使结构体大小为512字节
// Signature_word     : 结束标志（0xAA55）
// FAT32 文件系统中 BPB 的各个数据，具体含义请参照 FAT 规范
struct Fat32BPB {
    uint8_t BS_jmpBoot[3];
    uint8_t BS_oemName[8];
    uint16_t BPB_BytsPerSec;
    uint8_t BPB_SecPerClus;
    uint16_t BPB_RsvdSecCnt;
    uint8_t BPB_NumFATs;
    uint16_t BPB_rootEntCnt;
    uint16_t BPB_totSec16;
    uint8_t BPB_media;
    uint16_t BPB_FATSz16;
    uint16_t BPB_SecPerTrk;
    uint16_t BPB_NumHeads;
    uint32_t BPB_HiddSec;
    uint32_t BPB_TotSec32;
    uint32_t BPB_FATSz32;
    uint16_t BPB_ExtFlags;
    uint16_t BPB_FSVer;
    uint32_t BPB_RootClus;
    uint16_t BPB_FSInfo;
    uint16_t BPB_bkBootSec;
    uint8_t BPB_reserved[12];
    uint8_t BS_DrvNum;
    uint8_t BS_Reserved1;
    uint8_t BS_BootSig;
    uint32_t BS_VolID;
    uint8_t BS_VolLab[11];
    uint8_t BS_FileSysTye[8];
    uint8_t  __padding_1[420];
    uint16_t Signature_word;
} __attribute__((packed));

/**
 * @brief FAT32 Directory Entry Structure (Short Filename)
 * 
 * @details This structure represents a directory entry in the FAT32 file system
 * using the 8.3 format (short filename).
 * 
 * @struct DirEntry
 * @var DIR_Name 文件名(8字节)和扩展名(3字节)，空格填充
 * @var DIR_Attr 文件属性(只读、隐藏、系统、卷标、子目录、归档等)
 * @var DIR_NTRes Windows NT保留字段，一般为0
 * @var DIR_CrtTimeTenth 文件创建时间的毫秒部分(0-199)
 * @var DIR_CrtTime 文件创建时间(时、分、秒)
 * @var DIR_CrtDate 文件创建日期(年、月、日)
 * @var DIR_LstAccDate 文件最后访问日期
 * @var DIR_FstClusHI 文件起始簇号的高16位
 * @var DIR_WrtTime 文件最后写入时间
 * @var DIR_WrtDate 文件最后写入日期
 * @var DIR_FstClusLO 文件起始簇号的低16位
 * @var DIR_FileSize 文件大小(字节)，目录项该值为0
 */
// 目录项 (短文件名)
struct DirEntry {
    uint8_t DIR_Name[11];
    uint8_t DIR_Attr;
    uint8_t DIR_NTRes;
    uint8_t DIR_CrtTimeTenth;
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate;
    uint16_t DIR_LstAccDate;
    uint16_t DIR_FstClusHI;
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} __attribute__((packed));

// 目录项的属性，即 DIR_Attr 字段
enum DirEntryAttributes {
    READ_ONLY       = 0x01,
    HIDDEN          = 0x02,
    SYSTEM          = 0x04,
    VOLUME_ID       = 0x08,
    DIRECTORY       = 0x10,
    ARCHIVE         = 0x20,
    LONG_NAME       = 0x0F,
    LONG_NAME_MASK  = 0x3F,
};

// 用于存储文件名及文件大小
struct FileInfo {
    uint8_t DIR_Name[11];
    uint32_t DIR_FileSize;
};

// 用于存储一个目录文件中包含的所有文件的文件名及文件大小
struct FilesInfo {
    struct FileInfo *files;
    int size;
};

// 你应当在 fat.c 中实现以下函数 (fat_mount 已实现)
// 该 API 接收待挂载文件系统镜像路径 path 为输入，挂载成功返回 0，否则返回 -1 (例如，挂载不存在的文件系统、或者重复挂载)。
extern int fat_mount(const char *path);

// 该 API 接收一个普通文件的绝对路径 path 为输入 (例如，/DIR_1/111.txt)，
// 打开成功时返回一个 int 类型的文件描述符 (从 0 开始编号)，
// 打开失败返回 -1 (例如，打开一个不存在的文件、或打开一个目录文件)。
// 在这里你需要自行维护一个打开文件表 (即返回的文件描述符应该是自己维护打开文件表的索引，而不是当前进程的文件描述符)。
// 为了简化设计，这里将打开文件表的最大长度设置为 128；如果已经打开了 128 个文件，再次调用 fat_open() 时直接返回 -1。
extern int fat_open(const char *path);

// 该 API 接收一个已打开文件的文件描述符 fd 为输入，关闭成功返回 0，否则返回 -1 (例如，关闭一个未打开的文件)。
extern int fat_close(int fd);

// 该 API 将读取文件描述符 fd 对应文件从 offset 开始、长度为 count 的内容，并将其复制到缓冲区 buffer，
// 读取成功时返回真实读取的数据长度 (这里的 offset、count 和返回值均以字节为单位)。
// 在读取到文件末尾时，返回值可能会小于 count；
// 如果输入参数 count 值为 0 或者 offset 超过了文件末尾，则应返回 0。对于其它读取失败的情况，返回 -1。
extern int fat_pread(int fd, void *buffer, int count, int offset);

// 该 API 接收一个目录文件的绝对路径 path 为输入 (根目录为 / )，
// 返回该目录中包含的每个文件 (包括普通文件和目录文件) 的文件名和文件大小，
// 这里的文件名按照 FAT 的存储格式返回 (例如文件 a.txt 在 FAT 中的存储格式应为 A TXT)；
// 若读取失败，则返回 NULL (例如，读取的目录文件路径不存在、或者为非目录文件)。 
// 注意在目录文件中会存在空闲的目录项 (例如，文件被删除)，这些空闲目录项不应该包含在该 API 的返回结果中。
extern struct FilesInfo* fat_readdir(const char *path);

#endif
