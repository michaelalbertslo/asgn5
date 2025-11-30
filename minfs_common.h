#ifndef MINFS_COMMON_H
#define MINFS_COMMON_H

#include <stdio.h>
#include "min.h"

extern off_t fs_start;
extern off_t fs_end;

typedef int (*zone_visit_fn)(const zone_span *span, void *user);

void handle_part(FILE *image);
void handle_superblock(FILE *image, fs_info *fs);
void init_fs(FILE *image, fs_info *fs, int primary, int subpart);
void print_superblock(fs_info *fs);
off_t get_inode_offset(int node_num, fs_info *fs);
void readinto(void *thing, off_t offset, size_t bytes, FILE *image, size_t *tot);
void resolve_path(FILE *image, fs_info *fs, minix_inode *inode, char *path);
int read_inode(FILE *image, fs_info *fs, uint32_t ino, minix_inode *out);
int iterate_file_zones(FILE *image, fs_info *fs, const minix_inode *in,
    zone_visit_fn cb, void *user
);

#endif
