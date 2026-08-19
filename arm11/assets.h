#ifndef ASSETS_H
#define ASSETS_H
#include "types.h"
typedef struct { u16 w,h; const u8 *a; } AlphaTile;
/* All assets are decoded directly from the supplied first-party logo.bin. */
extern const AlphaTile logo_nintend,logo_o_top,logo_o_bottom,logo_3_native,logo_ds,logo_nintendo_bottom;
#endif
