#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include "minfs_common.h"
#include "min.h"

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
#define MAXPART 3
#define MINPART 0

#define FILEMASK  0170000
#define DIRECTORY 0040000
#define REGFILE   0100000

#define PERMSMASK 07777

#define CORRECTSIZE 64
#define SAFE_NAME_SIZE 61

int verbose_flag = 0;
static int primary = -1;
static int subpart = -1;
const char *imagefile = NULL;
const char *srcpath = NULL;
const char *dstpath = NULL;


/*function to use getopts to parse cmd line args*/
void parse_options(int argc, char *argv[]) {
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
    printf("dstpath: %s\n", dstpath ? dstpath : "(not provided)");
    printf("partition: %d\n", primary);
    printf("subpartition: %d\n", subpart);
  }
}

/*minget specific source code*/

/*testing function - will delete */
/* ---------- zone iterator test helpers (for debugging) ---------- */

typedef struct {
  uint64_t total_bytes;       /* total length of all spans */
  uint64_t next_file_off;     /* where the next span *should* start */
  int      first_span;        /* flag so we don't check contiguity on first one */
} zone_test_ctx;

static int test_zone_cb(const zone_span *span, void *user) {
  zone_test_ctx *ctx = (zone_test_ctx *)user;

  /* 1. Check contiguity: each span should start where the previous ended */
  if (!ctx->first_span) {
    if (span->file_off != ctx->next_file_off) {
      fprintf(stderr,
              "iterate_file_zones BUG: non-contiguous span: "
              "got file_off=%llu, expected=%llu\n",
              (unsigned long long)span->file_off,
              (unsigned long long)ctx->next_file_off);
      return -1;  /* abort iteration */
    }
  }

  /* 2. Length must be > 0 */
  if (span->length == 0) {
    fprintf(stderr, "iterate_file_zones BUG: zero-length span\n");
    return -1;
  }

  /* 3. Holes vs data sanity */
  if (span->is_hole && span->zone != 0) {
    fprintf(stderr, "iterate_file_zones BUG: hole with nonzero zone=%u\n", span->zone);
    return -1;
  }
  if (!span->is_hole && span->zone == 0) {
    fprintf(stderr, "iterate_file_zones BUG: data span with zone==0\n");
    return -1;
  }

  /* 4. Optional debug print (guard with verbose_flag if you want) */
  printf("span: file_off=%10llu len=%6u zone=%6u %s\n",
         (unsigned long long)span->file_off,
         span->length,
         span->zone,
         span->is_hole ? "(hole)" : "(data)");

  /* 5. Update running totals */
  ctx->total_bytes   += span->length;
  ctx->next_file_off  = span->file_off + span->length;
  ctx->first_span     = 0;

  return 0;  /* keep iterating */
}


int main(int argc, char *argv[]){
  FILE *image = NULL;
  fs_info fs;
  minix_inode inode;

  parse_options(argc, argv);

  if (verbose_flag) {
    printf("verbose: Opening imagefile...");
  }
  image = fopen(imagefile, "rb");
  if (image == NULL) {
    perror("fopen");
    exit(EXIT_FAILURE);
  }
  init_fs(image, &fs, primary, subpart);

  if (verbose_flag) {
    print_superblock(&fs);
  }
  /*then do the minget specific stuff*/
  resolve_path(image, &fs, &inode, (char *)srcpath);
  /* ---------- TEST iterate_file_zones ON THIS INODE ---------- */
  zone_test_ctx ctx;
  ctx.total_bytes   = 0;
  ctx.next_file_off = 0;
  ctx.first_span    = 1;

  int rc = iterate_file_zones(image, &fs, &inode, test_zone_cb, &ctx);
  if (rc != 0) {
    fprintf(stderr, "iterate_file_zones failed with %d\n", rc);
    exit(EXIT_FAILURE);
  }

  if (ctx.total_bytes != inode.size) {
    fprintf(stderr,
            "iterate_file_zones BUG: covered %llu bytes, inode.size=%u\n",
            (unsigned long long)ctx.total_bytes,
            inode.size);
    exit(EXIT_FAILURE);
  }

  printf("iterate_file_zones OK: covered %llu bytes (inode.size=%u)\n",
          (unsigned long long)ctx.total_bytes,
          inode.size);
  return 0;
}