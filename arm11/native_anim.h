#ifndef NATIVE_ANIM_H
#define NATIVE_ANIM_H
#include "types.h"
typedef struct __attribute__((packed)) {
    s16 nin_x_q8, ds_x_q8;
    u16 three_scale_q8;
    u8 nin_a, ds_a, three_a, root_a, bottom_a;
    u8 red_a[3];
    s16 red_tx_q8[3], red_sc_q8[3], blk_tx_q8[3], blk_sc_q8[3];
} NativeAnimFrame;
extern const NativeAnimFrame native_anim[105];
#endif
