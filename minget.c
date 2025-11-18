#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include "min.h"
#include <stdlib.h>

/*
Minget copies a regular file from the given source path to the given destination path. 
If the destination path is ommitted, it copies to stdout.

Will need to do:
command line parsing
read a file helper
output to file or stdout
Paths that do not include a leading ‘/’ are processed relative to the root directory

*/

#define BASE 10

int verbose_flag = 0;
static int primary = -1;
static int subpart = -1;
const char *imagefile = NULL;
const char *srcpath = NULL;
const char *dstpath = NULL;


/*function to use getopts to parse cmd line args*/
void parse_args(int argc, char *argv[]) {
  int opt;
  char *end;
  int remain;

  while ((opt = getopt(argc, argv, "p:s:v")) != -1) {
    switch (opt) {
      case 'p':
        errno = 0;
        primary = strtol(optarg, &end, BASE);
        if (end == optarg || *end != '\0' || errno == ERANGE){
          fprintf(stderr, "-p usage: p <num>\n");
          exit(EXIT_FAILURE);
        }
        break;
      case 's':
          errno = 0;
          subpart = strtol(optarg, &end, BASE);
          if (end == optarg || *end != '\0' || errno == ERANGE){
            fprintf(stderr, "-s usage: s <num>\n");
            exit(EXIT_FAILURE);
          }
          break;
      case 'v':
        verbose_flag = 1;
        break;
      default:
        fprintf(stderr, 
        "usage: minget [ -v ] [ -p part [ -s subpart ]" 
        "] imagefile srcpath [ dstpath ] \n");
        exit(EXIT_FAILURE);
    }
  }
  if (subpart != -1 && primary == -1) {
    fprintf(stderr, "usage: -s requires -p\n");
    exit(EXIT_FAILURE);
  }
  remain = argc - optind;
  if (remain < 2 || remain > 3) {
    fprintf(stderr,
    "usage: minget [ -v ] [ -p part [ -s subpart ]"
    "] imagefile srcpath [ dstpath ] \n");
    exit(EXIT_FAILURE);
  }
  imagefile = argv[optind++];
  srcpath = argv[optind++];
  if (remain == 3) {
      dstpath = argv[optind++];
  } else {
      dstpath = NULL;
  }
  if (verbose_flag){
    printf("verbose:\n");
    printf("imagefile: %s\n", imagefile);
    printf("srcpath: %s\n", srcpath);
    printf("dstpath: %s\n", dstpath ? dstpath : "(not provided)")
    printf("partition: %d\n", primary);
    printf("subpartition: %d\n", subpart);
  }
}


int main(int argc, char *argv[]){
  /*FILE *image = NULL;*/
  parse_options(argc, argv);
  if (verbose_flag) {
    printf("verbose: Opening imagefile...");
  }
  /*image = fopen(imagefile, "r");
  if (image == NULL){
    perror("fopen");
    exit(EXIT_FAILURE);
  }*/
  return 0;
}