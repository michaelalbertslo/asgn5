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

static inline int set_zone_traits(zone_visit_fn cb, void *user, uint32_t zone, int is_hole,
uint64_t file_off, uint32_t length, uint32_t zonesize) {
  zone_span span;
  span.is_hole = is_hole;
  span.zone = zone;
  span.file_off = file_off;
  span.length = length;
  if (is_hole) {
    span.image_off = 0;
  } else {
    span.image_off = (off_t)zone * (off_t)zonesize;
  }
  return cb(&span, user);
}

/*adding functionality that you have in minls as shared stuff and changing a bit */
int iterate_file_zones(FILE *image, fs_info *fs, const minix_inode *in, 
zone_visit_fn cb, void *user) {
  (void)image;
  const uint64_t fsize = in->size;
  const uint32_t zonesize = fs->zonesize;
  const uint32_t ptrs_per_blk = fs->ptrs_per_blk;
  uint64_t produced = 0;
  int i;
  uint32_t j;
  uint32_t f;
  uint32_t o;
  if (fsize == 0) {
    return 0;
  }
  /*returns a zone struct for a direct zone*/
  for (i = 0; i < DIRECT_ZONES && produced < fsize; i++) {
    uint32_t z = in->zone[i];
    uint32_t take;
    if ((fsize - produced) < zonesize) {
      take = (uint32_t)(fsize - produced);
    } else {
      take = (uint32_t)zonesize;
    }
    int rz = set_zone_traits(cb, user, z, (z == 0), produced, take, zonesize);
    if (rz) {
      return rz;
    }
    produced += take;
  }

  /*handling single indirect blocks*/
  /*only one indirect block to check, array of zone nums*/
  /*if still bytes in file and interect, cont.*/
  if (produced < fsize && in->indirect != 0) {
    uint32_t table_zone = in->indirect;
    off_t off = (off_t)table_zone * (off_t)zonesize;
    /*read first blcok and pull out the ptrs*/
    /*allocate a buffer the size of 1 block*/
    uint32_t *ptrs = (uint32_t*)malloc(fs->sb.blocksize);
    if (!ptrs) {
      perror("malloc");
      exit(EXIT_FAILURE);
    }
    readinto(ptrs, off, fs->sb.blocksize, image, NULL);
    /*a loop kinda like direct block to return zone structs*/
    for (j = 0; j < ptrs_per_blk && produced < fsize; j++) {
      uint32_t z = ptrs[j];
      uint32_t take;
      if ((fsize - produced) < zonesize) {
        take = (uint32_t)(fsize - produced);
      } else {
        take = (uint32_t)zonesize;
      }
      int rz = set_zone_traits(cb, user, z, (z == 0), produced, take, zonesize);
      if (rz) {
        free(ptrs);
        return rz;
      }
      produced += take;
    }
    free(ptrs);
  }

  /*double indirect block handling*/
  /*check if there is data and if double exists*/
  if (produced < fsize && in->two_indirect != 0) {
    uint32_t top_zone = in->two_indirect;
    off_t top_off = (off_t)top_zone * (off_t)zonesize;
    /*just like single indirect, allocate memory of 1 block*/
    uint32_t *top = (uint32_t*)malloc(fs->sb.blocksize);
    if (!top) {
      perror("malloc");
      exit(EXIT_FAILURE);
    }
    readinto(top, top_off, fs->sb.blocksize, image, NULL);
    for (f = 0; f < ptrs_per_blk && produced < fsize; i++) {
      uint32_t second_zone = top[i];
      /*check if second zone is holes*/
      if (second_zone == 0) {
        for (o = 0; o < ptrs_per_blk && produced < fsize; o++) {
          uint32_t take;
          if ((fsize - produced) < zonesize) {
            take = (uint32_t)(fsize - produced);
          } else {
            take = (uint32_t)zonesize;
          }
          int rz = set_zone_traits(cb, user, 0, 
            1, produced, take, zonesize);
          if (rz) {
            free(top);
            return rz;
          }
          produced += take;
        }
        continue;
      }
      off_t leaf_off = (off_t)second_zone * (off_t)zonesize;
      uint32_t *leaf = (uint32_t*)malloc(fs->sb.blocksize);
      if (!leaf) {
        free(top);
        perror("malloc");
        exit(EXIT_FAILURE);
      }
      readinto(leaf, leaf_off, fs->sb.blocksize, image, NULL);
      /*a loop kinda like direct block to return zone structs*/
      for (f = 0; f < ptrs_per_blk && produced < fsize; f++) {
        uint32_t z = leaf[f];
        uint32_t take;
        if ((fsize - produced) < zonesize) {
          take = (uint32_t)(fsize - produced);
        } else {
          take = (uint32_t)zonesize;
        }
        int rz = set_zone_traits(cb, user, z, (z == 0), produced, take, zonesize);
        if (rz) {
          free(leaf);
          free(top);
          return rz;
        }
        produced += take;
      }
      free(leaf);
    }
    free(top);
  }
  return 0;
}


/*TODO: actually go trough a path. just doing root rn.*/
void resolve_path(FILE *image, fs_info *fs, minix_inode *inode, char *path){
  char *cur_path;
  char *save;
  char *delim = strdup("/");

  /* root */
  readinto(inode, get_inode_offset(1,fs), sizeof(minix_inode), image, NULL); 
  cur_path = strtok_r(path, delim, &save);
  while (cur_path != NULL){
    cur_path = strtok_r(NULL, delim, &save);
    /* search for i node in root. need to look through all data blocks direct zones, indirect zones, double indirect*/
    /* check if its a regular file or directory */
    /* if reg file and still more in path, fail*/
    /* if reg file and end of path, just list file*/
  }
}

void readinto(void *thing, off_t offset, size_t bytes, FILE *image, size_t *tot){
  size_t bytes_read;
  if (fseek(image, (fs_start + offset), SEEK_SET) != 0){
    perror("fseek");
    exit(EXIT_FAILURE);
  }
  bytes_read = fread(thing, sizeof(uint8_t), bytes, image);
  if (bytes_read <= bytes) {
    if (ferror(image)) {
      perror("fread");
      exit(EXIT_FAILURE);
    }
  }
  
  if (tot != NULL){
    *tot=bytes_read;
  }
}

off_t get_inode_offset(int node_num, fs_info *fs){
  return (fs->firstIblock * fs->sb.blocksize) + ((node_num - 1) * sizeof(minix_inode));
}

void handle_superblock(FILE *image, fs_info *fs) {
  size_t bytes_read;
  
  if (fseek(image, (fs_start + SUPERBLOCK_OFFSET), SEEK_SET) != 0) {
    perror("fseek");
    exit(EXIT_FAILURE);
  }
  
  bytes_read = fread(&fs->sb, sizeof(uint8_t), sizeof(superblock), image);
  if (bytes_read <= sizeof(superblock)) {
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

/* will change this to add implementation for main part and subpart options*/
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

/*function to read inode contents, necessary for both ls and get*/
int read_inode(FILE *image, fs_info *fs, uint32_t ino, minix_inode *out) {
  if (ino == 0 || ino > fs->sb.ninodes) {
    fprintf(stderr, "Invalid inode number: %u (valid range: 1..%u)\n",
            ino, fs->sb.ninodes);
    return -1;
  }
  if (sizeof(minix_inode) != MINIX_INODE_SIZE &&
    fs->ino_per_block * sizeof(minix_inode) != fs->sb.blocksize) {
    fprintf(stderr, "Unexpected inode size: %zu bytes\n", sizeof(minix_inode));
    return -1;
  }
  off_t off = get_inode_offset((int)ino, fs);
  size_t nread = 0;
  readinto(out, off, sizeof(minix_inode), image, &nread);
  if (nread != sizeof(minix_inode)) {
    fprintf(stderr, "Short read while reading inode %u (got %zu bytes)\n", ino, nread);
    return -1;
  }
  /*successfully read inode*/
  return 0;
}

void init_fs(FILE *image, fs_info *fs, int primary, int subpart) {
  if (primary == -1) {
    handle_superblock(image, fs);
  } else {
    init_haspart(image, fs, primary, subpart);
  }
}