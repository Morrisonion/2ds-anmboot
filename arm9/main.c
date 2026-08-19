#include "types.h"
#include "shared.h"
#include "sdmmc.h"
#include "fat32.h"
#include "firm.h"

static inline u32 read_keys(void){
    return ((u32)(~(*(vu16*)HID_KEYS_ADDR))) & 0x0FFFu;
}

void arm9_main(void){
    /* Decide mode immediately, before any SD work. Luma boot shortcuts are used
       by holding the key before power-on, so sampling here preserves the normal
       behavior without adding an artificial delay to every cold boot. */
    u32 keys=read_keys();
    SHARED->magic=0x50494E4Bu;
    SHARED->boot_mode=keys?BOOTMODE_NORMAL_LUMA:BOOTMODE_ANIMATED;
    SHARED->key_mask=keys;
    SHARED->load_state=LOAD_SD_INIT;
    SHARED->arm11_in_stub=0;
    SHARED->target_size=0;
    SHARED->debug0=SHARED->debug1=SHARED->debug2=SHARED->debug3=0;

    sdmmc_init();
    int r=SD_Init();
    if(r!=0){SHARED->debug0=(u32)r;SHARED->load_state=LOAD_ERR_SD;while(1){}}

    SHARED->load_state=LOAD_FS_INIT;
    r=fat32_init();
    if(r!=0){SHARED->debug0=(u32)r;SHARED->load_state=LOAD_ERR_FS;while(1){}}

    SHARED->load_state=LOAD_READING;
    u32 sz=0;
    r=fat32_load_anmboot_boot((void*)TARGET_FIRM_ADDR,TARGET_FIRM_MAX,&sz);
    if(r){SHARED->debug0=(u32)r;SHARED->load_state=LOAD_ERR_FILE;while(1){}}

    SHARED->load_state=LOAD_VALIDATING;
    r=firm_validate((const FirmHeader*)TARGET_FIRM_ADDR,sz);
    if(r){SHARED->debug0=(u32)r;SHARED->load_state=LOAD_ERR_FIRM;while(1){}}

    if(SHARED->boot_mode==BOOTMODE_ANIMATED){
        r=firm_validate_concurrent((const FirmHeader*)TARGET_FIRM_ADDR);
        if(r){SHARED->debug0=(u32)r;SHARED->load_state=LOAD_ERR_COMPAT;while(1){}}
    }

    SHARED->target_size=sz;
    SHARED->load_state=LOAD_READY;

    if(SHARED->boot_mode==BOOTMODE_NORMAL_LUMA){
        /* ARM11 first moves into a neutral wait stub, then the normal B9S-style
           chainloader launches Luma's own ARM11 helper + ARM9 entry unchanged. */
        while(SHARED->arm11_in_stub!=1){}
        prepare_and_launch_normal((FirmHeader*)TARGET_FIRM_ADDR);
    }else{
        /* Animated fast path: Luma ARM9 runs immediately via its own FIRMLAUNCH
           ABI while our ARM11 renderer services its mailbox. */
        prepare_and_launch_animated((FirmHeader*)TARGET_FIRM_ADDR);
    }
    while(1){}
}
