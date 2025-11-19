#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include "min.h"
#include <string.h>
#include <stdlib.h>
#include "minfs_common.h"

#define BASE 10
#define MAXPART 3
#define MINPART 0

#define FILEMASK  0170000
#define DIRECTORY 0040000
#define REGFILE   0100000

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

off_t get_inode_offset(int node_num, fs_info *fs){
  return (fs->firstIblock * fs->sb.blocksize) + ((node_num - 1) * sizeof(minix_inode));
}


/*TODO: go through indirect zones as well*/
/* TODO: need to get file perms and size as well*/
void list_dir(FILE *image, minix_inode *dir_node, fs_info *fs){
  int i;
  minix_dirent dirent;
  size_t bytes_read = 0;
  off_t offset;

  for (i=0; i < DIRECT_ZONES; i++){ /* go through each zone */
    while (bytes_read < fs->zonesize){ /* read through a zone */
      offset = (dir_node->zone[i] * fs->zonesize) + bytes_read;
      readinto(&dirent, offset, sizeof(dirent), image, &bytes_read);
      if (bytes_read >= dir_node->size){
        return;
      }
    }
  }
}

/*TODO: go trough a path. just doing root rn.*/
void resolve_path(FILE *image, fs_info *fs, minix_inode *inode){
  char *cur_path;
  char *delim = strdup("/");

  readinto(inode, get_inode_offset(1,fs), sizeof(minix_inode), image, NULL);
  cur_path = strtok(path, delim);
  while (cur_path != NULL){
    /* traverse */
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
  resolve_path(image, &fs, &inode);
  list_dir(image, &inode, &fs);
  return 0;
}