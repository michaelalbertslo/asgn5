#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include "min.h"
#include <string.h>
#include <stdlib.h>

#define BASE 10
#define MAXPART 3
#define MINPART 0

static int verbose = 0;
static int primary = -1;
static int subpart = -1;
const char *imagefile = NULL;
const char *path = NULL;

off_t fs_start = 0; /*offset for where the FS starts*/
off_t fs_end;

static int block_size;

void parse_options(int argc, char *argv[]){
  int opt;
  char *end;
  int remain;
  while ((opt = getopt(argc, argv, "vp:s:")) != -1) {
    switch (opt){
      case 'v':
        verbose = 1;
        break;
      case 'p':
        primary = strtol(optarg, &end, BASE);
        if (end == optarg || errno == ERANGE){
          fprintf(stderr, "-p usage: p <num>\n");
          exit(EXIT_FAILURE);
        }
        if (primary > MAXPART || primary < MINPART){
          fprintf(stderr, "Partition %d out of range.  Must be 0..3.\n", primary);
          exit(EXIT_FAILURE);
        }
        break;
      case 's':
        subpart = strtol(optarg, &end, BASE);
        if (end == optarg || errno == ERANGE){
          fprintf(stderr, "-s usage: s <num>\n");
          exit(EXIT_FAILURE);
        }
        if (subpart > MAXPART || subpart < MINPART){
          fprintf(stderr, "Partition %d out of range.  Must be 0..3.\n", subpart);
          exit(EXIT_FAILURE);
        }
        break;
      default:
        fprintf(stderr, "usage: minls [ -v ] [ -p num [ -s num ] ] imagefile [ path ]\n");
        exit(EXIT_FAILURE);
    }
  }
  if (subpart != -1 && primary == -1) {
    fprintf(stderr, "usage: -s requires -p\n");
    exit(EXIT_FAILURE);
  }
  remain = argc - optind;
  if (remain <= 0){
    fprintf(stderr, "usage: minls [ -v ] [ -p num [ -s num ] ] imagefile [ path ]\n");
    exit(EXIT_FAILURE);
  }
  imagefile = argv[optind++];
  if (remain >= 2){
    path = argv[optind];
  } else {
    path = "/";
  }

  if (verbose){
    printf("verbose:\n");
    printf("imagefile: %s\n", imagefile);
    printf("path: %s\n", path);
    printf("partition: %d\n", primary);
    printf("subpartition: %d\n", subpart);
  }
}

void print_superblock(fs_info *fs){
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

void handle_superblock(FILE *image, fs_info *fs){
  uint8_t buf[sizeof(superblock)];
  size_t bytes_read;
  if (fseek(image, (fs_start + SUPERBLOCK_OFFSET), SEEK_SET) != 0){
    perror("fseek");
    exit(EXIT_FAILURE);
  }
  bytes_read = fread(&fs->sb, sizeof(uint8_t), sizeof(superblock), image);
  if (bytes_read <= sizeof(buf)){
    if (ferror(image)){
      perror("fread");
      exit(EXIT_FAILURE);
    }
  }
  if (fs->sb.magic != MINIX_MAGIC){
    fprintf(stderr, "This doesn't look like a Minix filesystem.\n");
    exit(EXIT_FAILURE);
  }
  fs->zonesize = fs->sb.blocksize << fs->sb.log_zone_size;
  fs->firstIblock = 2 + fs->sb.i_blocks + fs->sb.z_blocks;
  fs->ptrs_per_blk = fs->sb.blocksize / sizeof(uint32_t);
  fs->links_per_zone = fs->zonesize / sizeof(minix_dirent);
  fs->ino_per_block = fs->sb.blocksize / sizeof(minix_inode);
  if (verbose){
    print_superblock(fs);
  }  
}


void handle_part(FILE *image){
  uint8_t buf[MBR_SIZE];
  size_t bytes_read;
  partition_entry entry;
  if (verbose){
    printf("verbose: Loading MBR into buffer...\n");
  }
  if (fseek(image, fs_start, SEEK_SET) != 0){
    perror("fseek");
    exit(EXIT_FAILURE);
  }
  bytes_read = fread(buf, sizeof(uint8_t), MBR_SIZE, image);
  if (bytes_read <= sizeof(buf)){
    if (ferror(image)){
      perror("fread");
      exit(EXIT_FAILURE);
    }
  }
  if (buf[BOOT_SIGNATURE_1_LOC] != BOOT_SIG_1 || buf[BOOT_SIGNATURE_2_LOC] != BOOT_SIG_2){
    fprintf(stderr, "Invalid partition signature (0x%x, 0x%x).\n", buf[BOOT_SIGNATURE_1_LOC], buf[BOOT_SIGNATURE_2_LOC]);
    exit(EXIT_FAILURE);
  }
  if (verbose){
    printf("verbose: Boot sig 1: 0x%x, boot sig 2: 0x%x\n", buf[BOOT_SIGNATURE_1_LOC], buf[BOOT_SIGNATURE_2_LOC ]);
  }
  if (memcpy(&entry, &buf[PARTITION_TABLE_OFFSET + (sizeof(entry) * primary)], sizeof(entry)) == NULL){
    perror("memcpy");
    exit(EXIT_FAILURE);
  }
  if(entry.type != PARTITION_TYPE_MINIX){
    fprintf(stderr, "This doesn't look like a Minix filesystem.\n");
    exit(EXIT_FAILURE);
  }
  if (verbose){
    printf("verbose: seeking to start of partition %d...\n", primary);
  }
  if (fseek(image, entry.lFirst * SECTOR_SIZE, SEEK_SET) != 0){
    perror("fseek");
    exit(EXIT_FAILURE);
  }
  fs_start = entry.lFirst * SECTOR_SIZE;
  fs_end = (entry.lFirst + entry.size) * SECTOR_SIZE;
}

int init_haspart(FILE *image, fs_info *fs){
  handle_part(image);
  if (subpart != -1){
    if (verbose){
      printf("verbose: handing subpart...\n");
    }
    handle_part(image);
  }
  handle_superblock(image, fs);
}


int init_fs(FILE *image, fs_info *fs){
  if (primary == -1){
    handle_superblock(image, fs);
  } else {
    init_haspart(image, fs);
  }
}


int main(int argc, char *argv[]){
  FILE *image = NULL;
  fs_info fs;
  parse_options(argc, argv);
  if (verbose){
    printf("verbose: Opening imagefile...\n");
  }
  image = fopen(imagefile, "r");
  if (image == NULL){
    perror("fopen");
    exit(EXIT_FAILURE);
  }
  init_fs(image, &fs);
  return 0;
}