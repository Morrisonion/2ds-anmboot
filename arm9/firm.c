#include "firm.h"
#include "shared.h"
#include "minilib.h"

extern u8 arm9_normal_stub_start[],arm9_normal_stub_end[];
extern u8 arm9_animated_stub_start[],arm9_animated_stub_end[];

static int overlap(u32 a,u32 s,u32 b,u32 e){
    if(!s)return 0;u32 ae=a+s;if(ae<a)return 1;return a<e&&ae>b;
}
static int range_ok(u32 a,u32 s){
    if(!s)return 1;u32 e=a+s;if(e<a)return 0;
    if(a>=0x08000000u&&e<=0x08100000u)return 1;
    if(a>=0x18000000u&&e<=0x18600000u)return 1;
    if(a>=0x1FF00000u&&e<=0x1FFFFC00u)return 1;
    if(a>=0x20000000u&&e<=0x28000000u)return 1;
    return 0;
}
int firm_validate(const FirmHeader*f,u32 sz){
    if(sz<0x200||memcmp(f->magic,"FIRM",4))return -1;
    if(!f->arm9_entry)return -2;
    for(int i=0;i<4;i++){
        const FirmSection*s=&f->section[i];if(!s->size)continue;
        if((s->offset&0x1FF)||(s->size&0x1FF)||(s->address&0xF))return -3;
        if(s->offset+s->size<s->offset||s->offset+s->size>sz)return -4;
        if(!range_ok(s->address,s->size))return -5;
        u32 a=s->address,e=a+s->size,ba=TARGET_FIRM_ADDR,be=TARGET_FIRM_ADDR+TARGET_FIRM_MAX;
        if(a<be&&e>ba)return -6;
    }
    return 0;
}

/* The animated path needs the target FIRM not to overwrite our renderer or its
   two temporary BGR8 framebuffers before the PREPARE handoff. Current stock
   Luma satisfies these ranges; the check fails closed for incompatible builds. */
int firm_validate_concurrent(const FirmHeader*f){
    for(int i=0;i<4;i++){
        const FirmSection*s=&f->section[i];if(!s->size)continue;
        if(overlap(s->address,s->size,0x1FFB0000u,0x1FFBE000u))return -20;
        if(overlap(s->address,s->size,0x18300000u,0x1837E900u))return -21;
        if(overlap(s->address,s->size,0x18400000u,0x1847E900u))return -21;
        if(overlap(s->address,s->size,0x23000000u,0x23100000u))return -23;
        if(overlap(s->address,s->size,0x1FFFF000u,0x20000000u))return -22;
    }
    return 0;
}

static const char path_ascii[]="sdmc:/anmboot/boot.firm";

void prepare_and_launch_normal(FirmHeader*f){
    u32 n=(u32)(arm9_normal_stub_end-arm9_normal_stub_start);
    memcpy((void*)ARM9_STUB_ADDR,arm9_normal_stub_start,n);
    ((void(*)(FirmHeader*,const char*))ARM9_STUB_ADDR)(f,path_ascii);
    while(1){}
}

void prepare_and_launch_animated(FirmHeader*f){
    u32 n=(u32)(arm9_animated_stub_end-arm9_animated_stub_start);
    memcpy((void*)ARM9_STUB_ADDR,arm9_animated_stub_start,n);
    ((void(*)(FirmHeader*,const char*))ARM9_STUB_ADDR)(f,path_ascii);
    while(1){}
}
