#ifndef SHARED_H
#define SHARED_H
#include "types.h"
#define SHARED_ADDR          0x1FFFFB00u
#define MPCORE_ENTRY_ADDR    0x1FFFFFFCu
#define ARM11_STUB_ADDR      0x1FFFFC00u
#define ARM9_STUB_ADDR       0x00000200u
#define TARGET_FIRM_ADDR     0x24000000u
#define TARGET_FIRM_MAX      0x00400000u
#define LUMA_OPERATION_ADDR  0x1FF80004u
#define LUMA_ARM11_PARAMS    0x1FFFF000u
#define HID_KEYS_ADDR        0x10146000u

enum {
    LOAD_IDLE = 0,
    LOAD_SD_INIT = 1,
    LOAD_FS_INIT = 2,
    LOAD_READING = 3,
    LOAD_VALIDATING = 4,
    LOAD_READY = 5,
    LOAD_LUMA_RUNNING = 6,
    LOAD_ERR_SD = 0xE1,
    LOAD_ERR_FS = 0xE2,
    LOAD_ERR_FILE = 0xE3,
    LOAD_ERR_FIRM = 0xE4,
    LOAD_ERR_COMPAT = 0xE5
};

enum {
    BOOTMODE_UNSET = 0,
    /* Any held key: hand the exact target Luma its ordinary B9S-style boot.
       This preserves SELECT config, START chainloader and all other Luma hotkeys. */
    BOOTMODE_NORMAL_LUMA = 1,
    /* No held key: use Luma's stock FIRMLAUNCH ABI so its final deinitScreens()
       is skipped and our logo can occupy the otherwise-black pre-NATIVE interval. */
    BOOTMODE_ANIMATED = 2
};

typedef struct {
    vu32 magic;
    vu32 load_state;
    vu32 arm11_in_stub;
    vu32 target_size;
    vu32 debug0;
    vu32 debug1;
    vu32 debug2;
    vu32 debug3;
    vu32 boot_mode;
    vu32 key_mask;
} SharedState;

#define SHARED ((SharedState*)SHARED_ADDR)
#endif
