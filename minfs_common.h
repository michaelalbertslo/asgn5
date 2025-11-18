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

#endif