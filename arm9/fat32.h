#ifndef FAT32_H
#define FAT32_H
#include "types.h"
int fat32_init(void);
int fat32_load_anmboot_boot(void *dst,u32 max,u32 *out_size);
#endif
