#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "min.h"
#include "minfs_common.h"

off_t fs_start = 0;
off_t fs_end;

void print_superblock(fs_info *fs) {
  printf("Superblock:\n");
  printf("  On disk:\n");
  printf("    ninodes        = %10u\n", fs->sb.ninodes);
  printf("    pad1           = %10u\n", fs->sb.pad1);
  printf("    i_blocks       = %10d\n", fs->sb.i_blocks);
  printf("    z_blocks       = %10d\n", fs->sb.z_blocks);
  printf("    firstdata      = %10u\n", fs->sb.firstdata);
  printf("    log_zone_size  = %10d\n", fs->sb.log_zone_size);
  printf("    max_file       = %10u\n", fs->sb.max_file);
  printf("    zones          = %10u\n", fs->sb.zones);
  printf("    magic          = 0x%8.4x\n", (unsigned)fs->sb.magic);
  printf("    blocksize      = %10u\n", fs->sb.blocksize);
  printf("    subversion     = %10u\n", fs->sb.subversion);
  printf("  Computed:\n");
  printf("    firstIblock    = %10u\n", fs->firstIblock);
  printf("    zonesize       = %10u\n", fs->zonesize);
  printf("    ptrs_per_blk   = %10u\n", fs->ptrs_per_blk);
  printf("    links_per_zone = %10u\n", fs->links_per_zone);
  printf("    ino_per_block  = %10u\n", fs->ino_per_block);
}

/*some common math functionalty*/

void handle_superblock(FILE *image, fs_info *fs) {
  uint8_t buf[sizeof(superblock)];
  size_t bytes_read;
  
  if (fseek(image, (fs_start + SUPERBLOCK_OFFSET), SEEK_SET) != 0) {
    perror("fseek");
    exit(EXIT_FAILURE);
  }
  
  bytes_read = fread(&fs->sb, sizeof(uint8_t), sizeof(superblock), image);
  if (bytes_read <= sizeof(buf)) {
    if (ferror(image)) {
      perror("fread");
      exit(EXIT_FAILURE);
    }
  }
  
  if (fs->sb.magic != MINIX_MAGIC) {
    fprintf(stderr, "This doesn't look like a Minix filesystem.\n");
    exit(EXIT_FAILURE);
  }
  
  fs->zonesize = fs->sb.blocksize << fs->sb.log_zone_size;
  fs->firstIblock = 2 + fs->sb.i_blocks + fs->sb.z_blocks;
  fs->ptrs_per_blk = fs->sb.blocksize / sizeof(uint32_t);
  fs->links_per_zone = fs->zonesize / sizeof(minix_dirent);
  fs->ino_per_block = fs->sb.blocksize / sizeof(minix_inode);
}

void handle_part(FILE *image) {
  uint8_t buf[MBR_SIZE];
  size_t bytes_read;
  partition_entry entry;
  
  if (fseek(image, fs_start, SEEK_SET) != 0) {
    perror("fseek");
    exit(EXIT_FAILURE);
  }
  
  bytes_read = fread(buf, sizeof(uint8_t), MBR_SIZE, image);
  if (bytes_read <= sizeof(buf)) {
    if (ferror(image)) {
      perror("fread");
      exit(EXIT_FAILURE);
    }
  }
  
  if (buf[BOOT_SIGNATURE_1_LOC] != BOOT_SIG_1 || 
      buf[BOOT_SIGNATURE_2_LOC] != BOOT_SIG_2) {
    fprintf(stderr, "Invalid partition signature (0x%x, 0x%x).\n", 
            buf[BOOT_SIGNATURE_1_LOC], buf[BOOT_SIGNATURE_2_LOC]);
    exit(EXIT_FAILURE);
  }
  
  if (memcpy(&entry, &buf[PARTITION_TABLE_OFFSET + 
             (sizeof(entry) * (fs_start / SECTOR_SIZE ? 0 : 0))], 
             sizeof(entry)) == NULL) {
    perror("memcpy");
    exit(EXIT_FAILURE);
  }
  
  if (entry.type != PARTITION_TYPE_MINIX) {
    fprintf(stderr, "This doesn't look like a Minix filesystem.\n");
    exit(EXIT_FAILURE);
  }
  
  if (fseek(image, entry.lFirst * SECTOR_SIZE, SEEK_SET) != 0) {
    perror("fseek");
    exit(EXIT_FAILURE);
  }
  
  fs_start = entry.lFirst * SECTOR_SIZE;
  fs_end = (entry.lFirst + entry.size) * SECTOR_SIZE;
}

static void init_haspart(FILE *image, fs_info *fs, int primary, int subpart) {
  handle_part(image);
  if (subpart != -1) {
    handle_part(image);
  }
  handle_superblock(image, fs);
}

void init_fs(FILE *image, fs_info *fs, int primary, int subpart) {
  if (primary == -1) {
    handle_superblock(image, fs);
  } else {
    init_haspart(image, fs, primary, subpart);
  }
}