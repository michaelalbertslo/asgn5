#ifndef MINFS_COMMON_H
#define MINFS_COMMON_H

#include <stdio.h>
#include "min.h"

extern off_t fs_start;
extern off_t fs_end;

void handle_part(FILE *image);
void handle_superblock(FILE *image, fs_info *fs);
void init_fs(FILE *image, fs_info *fs, int primary, int subpart);
void print_superblock(fs_info *fs);
off_t get_inode_offset(int node_num, fs_info *fs);
void readinto(void *thing, off_t offset, size_t bytes, FILE *image, size_t *tot);

#endif