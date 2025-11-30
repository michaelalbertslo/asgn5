#ifndef MIN_H
#define MIN_H

#include <stdint.h>
#include <unistd.h>

#define PARTITION_TABLE_OFFSET   0x1BE
#define PARTITION_TYPE_MINIX     0x81
#define BOOT_SIGNATURE_1         0x55
#define BOOT_SIGNATURE_2         0xAA

#define MINIX_MAGIC              0x4D5A
#define MINIX_MAGIC_REVERSED     0x5A4D

#define MINIX_INODE_SIZE         64
#define MINIX_DIRENT_SIZE        64

#define MBR_SIZE                 512
#define SECTOR_SIZE              512
#define BOOT_SIGNATURE_1_LOC     510
#define BOOT_SIGNATURE_2_LOC     511
#define BOOT_SIG_1               0x55
#define BOOT_SIG_2               0xAA
#define SUPERBLOCK_OFFSET  1024

typedef struct __attribute__((packed)) partition_entry {
  uint8_t  bootind;
  uint8_t  start_head;
  uint8_t  start_sec;
  uint8_t  start_cyl;
  uint8_t  type;
  uint8_t  end_head;
  uint8_t  end_sec;
  uint8_t  end_cyl;
  uint32_t lFirst;
  uint32_t size;
} partition_entry;


typedef struct __attribute__((packed)) superblock {
  uint32_t ninodes;
  uint16_t pad1;
  int16_t  i_blocks;
  int16_t  z_blocks;
  uint16_t firstdata;
  int16_t  log_zone_size;
  int16_t  pad2;
  uint32_t max_file;
  uint32_t zones;
  int16_t  magic;
  int16_t  pad3;
  uint16_t blocksize;
  uint8_t  subversion;
} superblock;


#define DIRECT_ZONES 7

typedef struct {
  int        is_hole; 
  uint32_t   zone;       
  off_t      image_off;  
  uint64_t   file_off;   
  uint32_t   length;     
} zone_span;

typedef struct __attribute__((packed)) minix_inode {
  uint16_t mode;
  uint16_t links;
  uint16_t uid;
  uint16_t gid;
  uint32_t size;
  int32_t  atime;
  int32_t  mtime;
  int32_t  ctime;
  uint32_t zone[DIRECT_ZONES];
  uint32_t indirect;
  uint32_t two_indirect;
  uint32_t unused;
} minix_inode;

typedef struct __attribute__((packed)) minix_dirent {
  uint32_t inode;
  unsigned char name[60];
} minix_dirent;

typedef struct {
  uint32_t firstIblock;
  uint32_t zonesize;
  uint32_t ptrs_per_blk;
  uint32_t links_per_zone;
  uint32_t ino_per_block;
  struct superblock sb;
} fs_info;

#endif
