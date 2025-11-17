#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "min.h"
#include <stdlib.h>

#define BASE 10

static int verbose = 0;
static int primary = -1;
static int subpart = -1;
const char *imagefile = NULL;
const char *path = NULL;


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

int main(int argc, char *argv[]){
  FILE *image = NULL;
  parse_options(argc, argv);
  image = fopen(imagefile, "r");
  if (image == NULL){
    perror("fopen");
    exit(EXIT_FAILURE);
  }
  return 0;
}