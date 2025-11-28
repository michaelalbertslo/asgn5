#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include "minfs_common.h"
#include "min.h"

#define BASE 10
#define MAXPART 3
#define MINPART 0

#define FILEMASK  0170000
#define DIRECTORY 0040000
#define REGFILE   0100000

#define PERMSMASK 07777

#define CORRECTSIZE 64
#define SAFE_NAME_SIZE 61

static int verbose = 0;
static int primary = -1;
static int subpart = -1;
const char *imagefile = NULL;
char *path = NULL;

void parse_options(int argc, char *argv[]) {
  int opt;
  char *end;
  int remain;
  
  while ((opt = getopt(argc, argv, "vp:s:")) != -1) {
    switch (opt) {
      case 'v':
        verbose = 1;
        break;
      case 'p':
        primary = strtol(optarg, &end, BASE);
        if (end == optarg || errno == ERANGE) {
          fprintf(stderr, "-p usage: p <num>\n");
          exit(EXIT_FAILURE);
        }
        if (primary > MAXPART || primary < MINPART) {
          fprintf(stderr, "Partition %d out of range.  Must be 0..3.\n", primary);
          exit(EXIT_FAILURE);
        }
        break;
      case 's':
        subpart = strtol(optarg, &end, BASE);
        if (end == optarg || errno == ERANGE) {
          fprintf(stderr, "-s usage: s <num>\n");
          exit(EXIT_FAILURE);
        }
        if (subpart > MAXPART || subpart < MINPART) {
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
  if (remain <= 0) {
    fprintf(stderr, "usage: minls [ -v ] [ -p num [ -s num ] ] imagefile [ path ]\n");
    exit(EXIT_FAILURE);
  }
  
  imagefile = argv[optind++];
  if (remain >= 2) {
    path = strdup(argv[optind]);
  } else {
    path = strdup("/");
  }
  
  if (verbose) {
    printf("verbose:\n");
    printf("imagefile: %s\n", imagefile);
    printf("path: %s\n", path);
    printf("partition: %d\n", primary);
    printf("subpartition: %d\n", subpart);
  }
}

void print_filetype(uint16_t mode){
  uint16_t type = mode & FILEMASK;
  switch(type){
    case DIRECTORY:
      printf("d");
      break;
    case REGFILE:
      printf("-");
      break;
    default:
      fprintf(stderr, "err: not a directory or regfile\n");
      exit(EXIT_FAILURE);
  }
}

void print_perms(mode_t mode){
  char perms[10] = "---------";

  if(mode & S_IRUSR){
    perms[0] = 'r';
  }
  if(mode & S_IWUSR){
    perms[1] = 'w';
  }
  if(mode & S_IXUSR){
    perms[2] = (mode & S_ISUID) ? 's' : 'x';
  }

  if(mode & S_IRGRP){
    perms[3] = 'r';
  }
  if(mode & S_IWGRP){
    perms[4] = 'w';
  }
  if(mode & S_IXGRP){
    perms[5] = (mode & S_ISGID) ? 's' : 'x';
  }

  if(mode & S_IROTH){
    perms[6] = 'r';
  }
  if(mode & S_IWOTH){
    perms[7] = 'w';
  }
  if(mode & S_IXOTH){
    perms[8] = (mode & S_ISVTX) ? 't' : 'x';
  }

  perms[9] = '\0';
  printf("%s ", perms);
}

void print_file(minix_dirent *entry, fs_info *fs, FILE *image){
  minix_inode file_node;
  char name[SAFE_NAME_SIZE];
  memcpy(name, entry->name, SAFE_NAME_SIZE-1);
  name[SAFE_NAME_SIZE-1] = '\0';

  readinto(&file_node, get_inode_offset(entry->inode ,fs), sizeof(minix_inode), image, NULL);
  print_filetype(file_node.mode);
  print_perms(file_node.mode & PERMSMASK);
  printf("%9u %s\n", file_node.size, name);
}

void read_zone_entries(FILE *image, uint32_t zone_num, fs_info *fs, size_t *total_bytes_read, uint32_t dir_size) {
  size_t zone_bytes_read = 0;
  size_t bytes_just_read = 0;
  off_t offset;
  minix_dirent dirent;
  
  while (zone_bytes_read < fs->zonesize && *total_bytes_read < dir_size) {
    offset = (zone_num * fs->zonesize) + zone_bytes_read;
    bytes_just_read = 0;
    readinto(&dirent, offset, sizeof(dirent), image, &bytes_just_read);
    
    zone_bytes_read += bytes_just_read;
    *total_bytes_read += bytes_just_read;
    
    if (dirent.inode == 0) {
      continue;
    }
    
    print_file(&dirent, fs, image);
  }
}


/* indirect zone is a number (zone number) where in that zones first block, has zone numbers of actual data*/
void read_indirect_zone(FILE *image, uint32_t indirect_zone, fs_info *fs, size_t *total_bytes_read, uint32_t dir_size) {
  off_t offset = (indirect_zone * fs->zonesize);
  int i;
  uint32_t zones[fs->ptrs_per_blk];
  readinto(zones, offset, sizeof(zones), image, NULL);

  for (i = 0; i < fs->ptrs_per_blk; i++){
    if (zones[i] == 0){
      return;
    }
    read_zone_entries(image, zones[i], fs, total_bytes_read, dir_size);
  }  
}

void read_double_indirect_zone(FILE *image, uint32_t double_indirect_zone, fs_info *fs, size_t *total_bytes_read, uint32_t dir_size){
  off_t offset = (double_indirect_zone * fs->zonesize);
  int i;
  uint32_t zones[fs->ptrs_per_blk];
  readinto(zones, offset, sizeof(zones), image, NULL);
  for (i = 0; i < fs->ptrs_per_blk; i++){
    if (zones[i] == 0){
      return;
    }
    read_indirect_zone(image, zones[i], fs, total_bytes_read, dir_size);
  }
}

/* TODO: count zone 0 as bytes read even if you do nothing*/
void list_dir(FILE *image, minix_inode *dir_node, fs_info *fs) {
  int i;
  
  size_t total_bytes_read = 0;
  
  if(dir_node->size % CORRECTSIZE != 0){
    fprintf(stderr, "Directory size is not a multiple of 64. Something went wrong");
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < DIRECT_ZONES && dir_node->zone[i] != 0 && total_bytes_read < dir_node->size; i++) {
    if (dir_node->zone[i] == 0){
      return;
    }
    read_zone_entries(image, dir_node->zone[i], fs, &total_bytes_read, dir_node->size);
  }

  if (dir_node->indirect != 0 && total_bytes_read < dir_node->size) {
    read_indirect_zone(image, dir_node->indirect, fs, &total_bytes_read, dir_node->size);
  }

  if (dir_node->two_indirect != 0 && total_bytes_read < dir_node->size) {
    read_double_indirect_zone(image, dir_node->two_indirect, fs, &total_bytes_read, dir_node->size);
  }
}


int main(int argc, char *argv[]) {
  FILE *image = NULL;
  fs_info fs;
  minix_inode inode;
  
  parse_options(argc, argv);
  
  if (verbose) {
    printf("verbose: Opening imagefile...\n");
  }
  
  image = fopen(imagefile, "r");
  if (image == NULL) {
    perror("fopen");
    exit(EXIT_FAILURE);
  }
  
  init_fs(image, &fs, primary, subpart);
  
  if (verbose) {
    print_superblock(&fs);
  }
  printf("%s:\n", path);
  resolve_path(image, &fs, &inode, path);
  list_dir(image, &inode, &fs);
  return 0;
}