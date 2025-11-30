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

typedef struct {
  FILE *image;
  FILE *out;
  fs_info *fs;
  /*pointer to buffer address*/
  uint8_t *buf;
  size_t buf_size;
} copy_context;

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

// typedef struct {
//   uint64_t total_bytes;       /* total length of all spans */
//   uint64_t next_file_off;     /* where the next span *should* start */
//   int      first_span;        /* flag so we don't check contiguity on first one */
// } zone_test_ctx;

// static int test_zone_cb(const zone_span *span, void *user) {
//   zone_test_ctx *ctx = (zone_test_ctx *)user;

//   /* 1. Check contiguity: each span should start where the previous ended */
//   if (!ctx->first_span) {
//     if (span->file_off != ctx->next_file_off) {
//       fprintf(stderr,
//               "iterate_file_zones BUG: non-contiguous span: "
//               "got file_off=%llu, expected=%llu\n",
//               (unsigned long long)span->file_off,
//               (unsigned long long)ctx->next_file_off);
//       return -1;  /* abort iteration */
//     }
//   }

//   /* 2. Length must be > 0 */
//   if (span->length == 0) {
//     fprintf(stderr, "iterate_file_zones BUG: zero-length span\n");
//     return -1;
//   }

//   /* 3. Holes vs data sanity */
//   if (span->is_hole && span->zone != 0) {
//     fprintf(stderr, "iterate_file_zones BUG: hole with nonzero zone=%u\n", span->zone);
//     return -1;
//   }
//   if (!span->is_hole && span->zone == 0) {
//     fprintf(stderr, "iterate_file_zones BUG: data span with zone==0\n");
//     return -1;
//   }

//   /* 4. Optional debug print (guard with verbose_flag if you want) */
//   printf("span: file_off=%10llu len=%6u zone=%6u %s\n",
//          (unsigned long long)span->file_off,
//          span->length,
//          span->zone,
//          span->is_hole ? "(hole)" : "(data)");

//   /* 5. Update running totals */
//   ctx->total_bytes   += span->length;
//   ctx->next_file_off  = span->file_off + span->length;
//   ctx->first_span     = 0;

//   return 0;  /* keep iterating */
// }

/*copies data from file zone to output*/
static int copy_callback(const zone_span *span, void *user) {
  copy_context *ctx = (copy_context *)user;
  uint32_t left = span->length;

  if (span -> is_hole) {
    /*treat the entire span as all zeroes*/
    memset(ctx->buf, 0, ctx->buf_size);
    /*fill buffer with zeroes*/
    /*then write the zeros in chunks until span->length bytes written*/
    while (left > 0) {
      size_t chunk;
      if (left < ctx->buf_size) {
        chunk = left;
      } else {
        chunk = ctx->buf_size;
      }
      /*then write to the buffer and error check*/
      if (fwrite(ctx->buf, 1, chunk, ctx->out) != chunk) {
        perror("fwrite");
        return -1;
      }
      left -= (uint32_t)chunk;
    }
    return 0;
  }
  /*not a hole, read from file and write to buffer as normal*/
  off_t pos = span->image_off;
  while (left > 0) {
    /*use readinto to get file contents and write to out*/
    size_t chunk;
    if (left < ctx->buf_size) {
      chunk = left;
    } else {
      chunk = ctx->buf_size;
    }
    readinto(ctx->buf, pos, chunk, ctx->image, NULL); 
    if (fwrite(ctx->buf, 1, chunk, ctx->out) != chunk) {
      perror("fwrite");
      return -1;
    }
    pos += (off_t)chunk;
    left -= (uint32_t)chunk;
  }
  return 0; /*success*/
}


int main(int argc, char *argv[]){
  FILE *image = NULL;
  fs_info fs;
  minix_inode inode;
  FILE *out = NULL;
  uint8_t *buf = NULL;
  size_t buf_size;
  int result;


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
  if (srcpath == NULL) {
    exit(EXIT_FAILURE);
  }
  resolve_path(image, &fs, &inode, (char *)srcpath);
  if ((inode.mode & FILEMASK) != REGFILE) {
    fprintf(stderr, "Not a regular file.\n");
    exit(EXIT_FAILURE);
  }
  /*checking for open destination, file or stdout*/
  if (dstpath == NULL) {
    out = stdout;
  } else {
    out = fopen(dstpath, "wb");
    if (out == NULL) {
      perror("fopen dstpath");
      exit(EXIT_FAILURE);
    }
  }
  /*set up read/write buffer and context struct now*/
  buf_size = fs.sb.blocksize;
  /*buff is one block size from superblock*/
  buf = malloc(buf_size);
  if (buf == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  copy_context ctx;
  ctx.image = image;
  ctx.out = out;
  ctx.fs = &fs;
  ctx.buf = buf;
  ctx.buf_size = buf_size;

   if (verbose_flag) {
    fprintf(stderr, "Copying %u bytes from \"%s\"...\n", inode.size, srcpath);
  }
  result = iterate_file_zones(image, &fs, &inode, copy_callback, &ctx);
  free(buf);
  if (out != stdout) {
    if (fclose(out) != 0) {
      perror("fclose dst");
      exit(EXIT_FAILURE);
    }
  }
  if (result != 0) {
    fprintf(stderr, "Error copying file contents. \n");
    exit(EXIT_FAILURE);
  }
  
  return 0;
}