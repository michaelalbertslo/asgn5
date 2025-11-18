#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include "min.h"
#include <stdlib.h>

#define BASE 10

static int verbose = 0;
static int primary = -1;
static int subpart = -1;
const char *imagefile = NULL;
const char *path = NULL;

off_t fs_start = 0; /*offset for where the FS starts*/

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
        break;
      case 's':
        subpart = strtol(optarg, &end, BASE);
        if (end == optarg || errno == ERANGE){
          fprintf(stderr, "-s usage: s <num>\n");
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


int init_fs(FILE *image){
  uint8_t buf[MBR_SIZE];
  if (verbose){
    printf("verbose: Loading MBR into buffer...");
  }
  if (fseek(image, 0, SEEK_SET) != 0){
    perror("fseek");
    exit(EXIT_FAILURE);
  }
  if (fread(buf, sizeof(uint8_t), MBR_SIZE, image) != 0){
    perror("fread");
    exit(EXIT_FAILURE);
  }
  if (buf[BOOT_SIGNATURE_1_LOC] != BOOT_SIGNATURE_1 || buf[BOOT_SIGNATURE_2_LOC] != BOOT_SIGNATURE_2){
    fprintf(stderr, "error: boot signature(s) is invalid\n");
    exit(EXIT_FAILURE);
  }
  if (verbose){
    printf("verbose: Boot sig 1: %d, boot sig 2: %d", buf[BOOT_SIGNATURE_1], buf[BOOT_SIGNATURE_2]);
  }
  
}


int main(int argc, char *argv[]){
  FILE *image = NULL;
  parse_options(argc, argv);
  if (verbose){
    printf("verbose: Opening imagefile...");
  }
  image = fopen(imagefile, "r");
  if (image == NULL){
    perror("fopen");
    exit(EXIT_FAILURE);
  }
  return 0;
}