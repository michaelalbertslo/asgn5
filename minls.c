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

#define PERMSMASK 07777
#define CORRECTSIZE 64

#define PATH_MAX 4096


typedef struct {
  FILE *image;
  fs_info *fs;
} list_context;

static int verbose = 0;
static int primary = -1;
static int subpart = -1;
const char *imagefile = NULL;
char *path = NULL;

/* parse command line arguments */
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
          fprintf(stderr,
            "Partition %d out of range.  Must be 0..3.\n", primary);
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
          fprintf(stderr,
            "Partition %d out of range.  Must be 0..3.\n", subpart);
          exit(EXIT_FAILURE);
        }
        break;
      default:
        fprintf(stderr,
          "usage: minls [ -v ] [ -p num [ -s num ] ] imagefile"
          " [ path ]\n");
        exit(EXIT_FAILURE);
    }
  }
  
  /* subpartition requires a primary partition */
  if (subpart != -1 && primary == -1) {
    fprintf(stderr, "usage: -s requires -p\n");
    exit(EXIT_FAILURE);
  }
  
  /* verify we have at least the imagefile argument */
  remain = argc - optind;
  if (remain <= 0) {
    fprintf(stderr,
      "usage: minls [ -v ] [ -p num [ -s num ] ] imagefile [ path ]\n");
    exit(EXIT_FAILURE);
  }
  
  /* extract imagefile and optional path arguments */
  imagefile = argv[optind++];
  if (remain >= 2) {
    path = strdup(argv[optind]);
    if (path == NULL) {
      perror("strdup");
      exit(EXIT_FAILURE);
    }
  } else {
    path = strdup("/");
    if (path == NULL) {
      perror("strdup");
      exit(EXIT_FAILURE);
    }
  }
  
  if (verbose) {
    printf("verbose:\n");
    printf("imagefile: %s\n", imagefile);
    printf("path: %s\n", path);
    printf("partition: %d\n", primary);
    printf("subpartition: %d\n", subpart);
  }
}

/* print file type indicator */
void print_filetype(uint16_t mode){
  uint16_t type = mode & FILEMASK;
  switch(type){
    case DIRECTORY:
      if (printf("d") < 0) {
        perror("printf");
        exit(EXIT_FAILURE);
      }
      break;
    case REGFILE:
      if (printf("-") < 0) {
        perror("printf");
        exit(EXIT_FAILURE);
      }
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
  if (printf("%s ", perms) < 0) {
    perror("printf");
    exit(EXIT_FAILURE);
  }
}

/* normalize path by ensuring it starts with exactly one slash */
char* normalize_path() {
  static char normalized[PATH_MAX];
  const char *start = path;
  
  /* skip any leading slashes */
  while (*start == '/') {
    start++;
  }
  
  /* handle root directory case */
  if (*start == '\0') {
    normalized[0] = '/';
    normalized[1] = '\0';
  } else {
    /* prepend single slash to non-root paths */
    normalized[0] = '/';
    if (strcpy(normalized + 1, start) == NULL) {
      fprintf(stderr, "strcpy failed\n");
      exit(EXIT_FAILURE);
    }
  }
  
  return normalized;
}

/* print file information */
void print_file(minix_dirent *entry, fs_info *fs, FILE *image){
  minix_inode file_node;
  char name[SAFE_NAME_SIZE];
  
  memcpy(name, entry->name, SAFE_NAME_SIZE-1);
  name[SAFE_NAME_SIZE-1] = '\0';

  /* read inode data for this directory entry */
  readinto(&file_node, get_inode_offset(entry->inode ,fs),
    sizeof(minix_inode), image, NULL);
  print_filetype(file_node.mode);
  print_perms(file_node.mode & PERMSMASK);
  if (printf("%9u %s\n", file_node.size, name) < 0) {
    perror("printf");
    exit(EXIT_FAILURE);
  }
}


/* callback function invoked for each data zone in a directory */
int list_zone_callback(const zone_span *span, void *user){
  list_context *ctx = (list_context *)user;
  uint32_t num_entries;
  uint32_t i;
  off_t offset;
  minix_dirent entry;

  if (span->is_hole){
    return 0;
  }

  /* calculate how many directory entries fit in this zone */
  num_entries = span->length / sizeof(minix_dirent);

  for(i=0 ; i< num_entries; i++){
    offset = span->image_off + (i * sizeof(minix_dirent));
    readinto(&entry, offset, sizeof(entry), ctx->image, NULL);

    if (entry.inode == 0){
      continue;
    }

    print_file(&entry, ctx->fs, ctx->image);
  }
  return 0;
}

/* list contents of directory or display file information */
void list_dir(FILE *image, minix_inode *dir_node, fs_info *fs) {
  list_context ctx;
  ctx.image = image;
  ctx.fs = fs;
  
  /* if target is a regular file, print its info and return */
  if ((dir_node->mode & FILEMASK) != DIRECTORY){
    print_filetype(dir_node->mode);
    print_perms(dir_node->mode & PERMSMASK);
    /* Skip leading slash when printing filename */
    if (printf("%9u %s\n", dir_node->size, normalize_path()+1) < 0) {
      perror("printf");
      exit(EXIT_FAILURE);
    }
    return;
  }
  
  /* print directory header and iterate through entries */
  if (printf("%s:\n", normalize_path()) < 0) {
    perror("printf");
    exit(EXIT_FAILURE);
  }
  iterate_file_zones(image, fs, dir_node, list_zone_callback, &ctx);
}


int main(int argc, char *argv[]){
  FILE *image = NULL;
  fs_info fs;
  minix_inode inode;
  
  parse_options(argc, argv);
  
  if (verbose) {
    if (printf("verbose: Opening imagefile...\n") < 0) {
      perror("printf");
      exit(EXIT_FAILURE);
    }
  }
  
  /* open the filesystem image file for reading */
  image = fopen(imagefile, "r");
  if (image == NULL) {
    perror("fopen");
    exit(EXIT_FAILURE);
  }
  
  /* init filesystem metadata from superblock and partition table */
  init_fs(image, &fs, primary, subpart);
  
  if (verbose) {
    print_superblock(&fs);
  }
  
  resolve_path(image, &fs, &inode, path);
  
  list_dir(image, &inode, &fs);
  
  free(path);

  if (fclose(image) != 0) {
    perror("fclose");
    exit(EXIT_FAILURE);
  }
  
  return 0;
}