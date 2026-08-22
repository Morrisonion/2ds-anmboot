#ifndef GFX_H
#define GFX_H
#include "types.h"
void gfx_init(void);
void gfx_draw_intro_frame(int frame);
void gfx_draw_glow_frame(int frame_b);
void gfx_draw_tail_frame(int frame_t);
void gfx_seal_static_logo(void);
#ifdef ANMBOOT_OLD3DS
void gfx_idle_vblank(void);
#endif
void gfx_black(void);
void gfx_error(u32 code);
#endif
