#include "types.h"
#include "shared.h"
#include "minilib.h"
#include "gfx.h"

extern u8 arm11_firmstub_start[],arm11_firmstub_end[];
extern u8 arm11_normal_stub_start[],arm11_normal_stub_end[];

enum {
    INIT_SCREENS=0,SETUP_FRAMEBUFFERS=1,CLEAR_SCREENS=2,SWAP_FRAMEBUFFERS=3,
    UPDATE_BRIGHTNESS=4,DEINIT_SCREENS=5,ZEROFILL_N3DS_ABL_REGISTERS=6,
    PREPARE_ARM11_FOR_FIRMLAUNCH=7,ARM11_READY=8
};
#define OP (*(vu32*)LUMA_OPERATION_ADDR)

static void check_error(void){
    u32 st=SHARED->load_state;
    if(st>=0xE0u){gfx_error(st);while(1)__asm__ volatile("wfi");}
}

/* For held boot keys, disappear completely and let the exact target Luma use
   its own ARM11 helper. This is what restores SELECT config, START chainloader,
   payload buttons, safe-mode combos, etc. */
static __attribute__((noreturn)) void enter_normal_luma_wait(void){
    *(vu32*)MPCORE_ENTRY_ADDR=0;
    u32 n=(u32)(arm11_normal_stub_end-arm11_normal_stub_start);
    memcpy((void*)ARM11_STUB_ADDR,arm11_normal_stub_start,n);
    SHARED->arm11_in_stub=1;
    ((void(*)(void))ARM11_STUB_ADDR)();
    while(1)__asm__ volatile("wfi");
}

/* While the animated FIRMLAUNCH path owns scanout, service only operations that
   are safe without giving PDC/framebuffer ownership away. PREPARE is deliberately
   left pending until the official Scene-B cycle and Scene-C fade have finished. */
static int service_luma(void){
    if(SHARED->load_state<LOAD_LUMA_RUNNING)return 0;
    u32 op=OP;
    if(op==ARM11_READY)return 0;
    if(op==PREPARE_ARM11_FOR_FIRMLAUNCH)return 1;
    if(op==DEINIT_SCREENS)return -2; /* FIRMLAUNCH should skip this. */

    if(op<=ZEROFILL_N3DS_ABL_REGISTERS){
        if(op==UPDATE_BRIGHTNESS){
            u32 b=*(vu32*)LUMA_ARM11_PARAMS;
            *(vu32*)0x10202240u=b;
            *(vu32*)0x10202A40u=b;
        }else if(op==ZEROFILL_N3DS_ABL_REGISTERS){
            *(vu32*)0x10202250u=0;*(vu32*)0x10202254u=0;
            *(vu32*)0x10202A50u=0;*(vu32*)0x10202A54u=0;
            for(u32 i=0;i<64;i++){
                *(vu32*)(0x10202300u+4*i)=0;
                *(vu32*)(0x10202B00u+4*i)=0;
            }
        }
        /* Setup/clear/swap stay visually deferred while our animation owns PDC. */
        OP=ARM11_READY;
        return 0;
    }
    return -1;
}

static __attribute__((noreturn)) void finish_into_native(void){
    *(vu32*)MPCORE_ENTRY_ADDR=0;
    u32 n=(u32)(arm11_firmstub_end-arm11_firmstub_start);
    memcpy((void*)ARM11_STUB_ADDR,arm11_firmstub_start,n);
    ((void(*)(u32,vu32*))ARM11_STUB_ADDR)(ARM11_READY,(vu32*)LUMA_OPERATION_ADDR);
    while(1)__asm__ volatile("wfi");
}

void arm11_main(void){
    /* ARM9 publishes boot_mode as its first action. Avoid reading stale AXI RAM
       if ARM11 happened to win the startup race by a few instructions. */
    while(SHARED->magic!=0x50494E4Bu || SHARED->boot_mode==BOOTMODE_UNSET){
        __asm__ volatile("nop");
    }

    if(SHARED->boot_mode==BOOTMODE_NORMAL_LUMA)enter_normal_luma_wait();

    gfx_init();
    int prep_seen=0;

    /* Scene A: always finish the official 60-frame entrance. If Luma becomes
       ready early, leave PREPARE pending rather than truncating the animation. */
    for(int f=0;f<60;f++){
        check_error();
        int r=service_luma();
        if(r==1)prep_seen=1;
        else if(r<0){gfx_error(LOAD_ERR_COMPAT);while(1)__asm__ volatile("wfi");}
        gfx_draw_intro_frame(f);
    }

#ifdef ANMBOOT_OLD3DS
    /* Old 3DS light mode: Scene A already contains the complete lightweight red
       "3" landing animation.  There is no glow bank at all.  If ARM9/Luma is
       still loading after frame 59, keep the final logo on-screen and merely
       service the mailbox once per VBlank; do not redraw static pixels. */
    while(!prep_seen){
        check_error();
        int r=service_luma();
        if(r==1)prep_seen=1;
        else if(r<0){gfx_error(LOAD_ERR_COMPAT);while(1)__asm__ volatile("wfi");}
        if(!prep_seen)gfx_idle_vblank();
    }
    gfx_seal_static_logo();
    finish_into_native();
#else
    /* Scene B is the wait indicator. Keep one complete 30-frame loop visible
       to match the approved official-timeline preview. After that, keep looping only while
       stock Luma ARM9 has not reached PREPARE. */
    int loops=0;
    while(1){
        for(int b=0;b<30;b++){
            check_error();
            int r=service_luma();
            if(r==1)prep_seen=1;
            else if(r<0){gfx_error(LOAD_ERR_COMPAT);while(1)__asm__ volatile("wfi");}
            gfx_draw_glow_frame(b);
        }
        loops++;
        if(prep_seen && loops>=1)break;
    }

    /* Do not globally fade all remaining waves at once. Once PREPARE has been
       observed, stop generating new waves and let the already-visible ones keep
       travelling along their Scene-B paths until they naturally disappear.
       This also reduces overlap and runtime load during the handoff lead-out. */
    for(int t=0;t<45;t++){
        check_error();
        int r=service_luma();
        if(r<0 && r!=1){gfx_error(LOAD_ERR_COMPAT);while(1)__asm__ volatile("wfi");}
        gfx_draw_tail_frame(t);
    }
    gfx_seal_static_logo();
    finish_into_native();
#endif
}
