#ifndef FIRM_H
#define FIRM_H
#include "types.h"
typedef struct __attribute__((packed)){
    u8 magic[4];u32 reserved0;u32 arm11_entry;u32 arm9_entry;u8 reserved1[0x30];
    struct __attribute__((packed)){u32 offset,address,size,copy_method;u8 hash[32];} section[4];
    u8 signature[0x100];
} FirmHeader;
typedef typeof(((FirmHeader*)0)->section[0]) FirmSection;
int firm_validate(const FirmHeader*f,u32 sz);
int firm_validate_concurrent(const FirmHeader*f);
void prepare_and_launch_normal(FirmHeader*f) __attribute__((noreturn));
void prepare_and_launch_animated(FirmHeader*f) __attribute__((noreturn));
#endif
