#ifndef TYPES_H
#define TYPES_H
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed short s16;
typedef signed int s32;
typedef unsigned long long u64;
typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef unsigned int size_t;
#define NULL ((void*)0)
#define BIT(n) (1u << (n))
#endif
