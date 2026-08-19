/* Minimal 3DS SDMMC driver. Derived from BAX/Normmatt code.
   BAX: GPLv3. Normmatt SDMMC code: MPL-2.0 or GPL-2.0+. */
#include "sdmmc.h"
static mmcdevice handleSD;
static inline u16 rd16(u16 r){return *(vu16*)(SDMMC_BASE+r);} 
static inline void wr16(u16 r,u16 v){*(vu16*)(SDMMC_BASE+r)=v;}
static inline u32 rd32(u16 r){return *(vu32*)(SDMMC_BASE+r);} 
static inline void mask16(u16 r,u16 clr,u16 set){u16 v=rd16(r);v&=(u16)~clr;v|=set;wr16(r,v);} 
static inline void setclk(u32 d){mask16(REG_SDCLKCTL,0x100,0);mask16(REG_SDCLKCTL,0x2FF,d&0x2FF);mask16(REG_SDCLKCTL,0,0x100);} 
static inline void waitcycles(u32 n){__asm__ volatile("1: subs %0,%0,#4\n bpl 1b":"+r"(n)::"cc");}
static int get_error(mmcdevice *c){return (int)((c->error<<29)>>31);} 
static void set_target(mmcdevice *c){mask16(REG_SDPORTSEL,3,(u16)c->devicenumber);setclk(c->clk);if(c->SDOPT==0)mask16(REG_SDOPT,0,0x8000);else mask16(REG_SDOPT,0x8000,0);} 
static void sendcmd(mmcdevice *c,u32 cmd,u32 args){
    const u16 getresp=(u16)((cmd<<15)>>31); u16 flags=getresp;
    const int read=!!(cmd&0x20000), write=!!(cmd&0x40000);
    if(read||write) flags|=TMIO_STAT0_DATAEND;
    c->error=0; u32 guard=0x04000000u; while((rd16(REG_SDSTATUS1)&TMIO_STAT1_CMD_BUSY) && --guard){} if(!guard){c->error|=4;return;}
    wr16(REG_SDIRMASK0,0);wr16(REG_SDIRMASK1,0);wr16(REG_SDSTATUS0,0);wr16(REG_SDSTATUS1,0);mask16(REG_DATACTL32,0x1800,0);
    wr16(REG_SDCMDARG0,(u16)args);wr16(REG_SDCMDARG1,(u16)(args>>16));wr16(REG_SDCMD,(u16)cmd);
    u32 size=c->size; u32 *rp32=(u32*)c->rData; u8 *rp8=c->rData;
    guard=0x08000000u; while(1){
        if(--guard==0){c->error|=4;break;}
        u16 s1=rd16(REG_SDSTATUS1), ctl=rd16(REG_DATACTL32);
        if((ctl&0x100) && read){
            mask16(REG_SDSTATUS1,TMIO_STAT1_RXRDY,0);
            if(size>=0x200){
                if((((u32)rp32)&3)==0){for(int i=0;i<0x200;i+=4)*rp32++=rd32(REG_SDFIFO32);}else{
                    for(int i=0;i<0x200;i+=4){u32 d=rd32(REG_SDFIFO32);*rp8++=(u8)d;*rp8++=(u8)(d>>8);*rp8++=(u8)(d>>16);*rp8++=(u8)(d>>24);} }
                size-=0x200;
            }
            mask16(REG_DATACTL32,0x800,0);
        }
        if(s1&TMIO_MASK_GW){c->error|=4;break;}
        if(!(s1&TMIO_STAT1_CMD_BUSY)){
            u16 s0=rd16(REG_SDSTATUS0);
            if(s0&TMIO_STAT0_CMDRESPEND)c->error|=1;
            if(s0&TMIO_STAT0_DATAEND)c->error|=2;
            if((s0&flags)==flags)break;
        }
    }
    c->stat0=rd16(REG_SDSTATUS0);c->stat1=rd16(REG_SDSTATUS1);wr16(REG_SDSTATUS0,0);wr16(REG_SDSTATUS1,0);
    if(getresp){for(int i=0;i<4;i++)c->ret[i]=(u32)rd16(REG_SDRESP0+i*4)|((u32)rd16(REG_SDRESP0+i*4+2)<<16);} 
}
int sdmmc_sdcard_readsectors(u32 sector_no,u32 numsectors,u8*out){
    if(!handleSD.isSDHC)sector_no<<=9;set_target(&handleSD);wr16(REG_SDSTOP,0x100);wr16(REG_SDBLKCOUNT32,(u16)numsectors);wr16(REG_SDBLKLEN32,0x200);wr16(REG_SDBLKCOUNT,(u16)numsectors);handleSD.rData=out;handleSD.size=numsectors<<9;sendcmd(&handleSD,0x33C12,sector_no);return get_error(&handleSD);
}
void sdmmc_init(void){
    handleSD.isSDHC=0;handleSD.SDOPT=0;handleSD.res=0;handleSD.initarg=0;handleSD.clk=0x80;handleSD.devicenumber=0;
    *(vu16*)0x10006100&=0xF7FF;*(vu16*)0x10006100&=0xEFFF;*(vu16*)0x10006100|=0x402;
    *(vu16*)0x100060D8=(*(vu16*)0x100060D8&0xFFDD)|2;*(vu16*)0x100060D8&=0xFFDF;
    *(vu16*)0x10006104=512;*(vu16*)0x10006108=1;*(vu16*)0x100060E0&=0xFFFE;*(vu16*)0x100060E0|=1;
    *(vu16*)0x10006020|=(u16)TMIO_MASK_ALL;*(vu16*)0x10006022|=(u16)(TMIO_MASK_ALL>>16);*(vu16*)0x100060FC|=0xDB;*(vu16*)0x100060FE|=0xDB;
    *(vu16*)0x10006002&=0xFFFC;*(vu16*)0x10006024=0x20;*(vu16*)0x10006028=0x40EE;*(vu16*)0x10006002&=0xFFFC;*(vu16*)0x10006026=512;*(vu16*)0x10006008=0;
}
int SD_Init(void){
    set_target(&handleSD);waitcycles(0xF000);sendcmd(&handleSD,0,0);sendcmd(&handleSD,0x10408,0x1AA);u32 hc=(handleSD.error&1)<<30;u32 t=0;
    u32 initguard=4096; do{do{sendcmd(&handleSD,0x10437,handleSD.initarg<<16);if(handleSD.error&4)return -9;sendcmd(&handleSD,0x10769,0x00FF8000|hc);if(handleSD.error&4)return -10;t=1;if(--initguard==0)return -11;}while(!(handleSD.error&1));}while(!(handleSD.ret[0]&0x80000000));
    if(!((handleSD.ret[0]>>30)&1)||!hc)t=0;handleSD.isSDHC=t;sendcmd(&handleSD,0x10602,0);if(handleSD.error&4)return -1;
    sendcmd(&handleSD,0x10403,0);if(handleSD.error&4)return -2;handleSD.initarg=handleSD.ret[0]>>16;
    sendcmd(&handleSD,0x10609,handleSD.initarg<<16);if(handleSD.error&4)return -3;handleSD.clk=1;setclk(1);
    sendcmd(&handleSD,0x10507,handleSD.initarg<<16);if(handleSD.error&4)return -4;sendcmd(&handleSD,0x10437,handleSD.initarg<<16);if(handleSD.error&4)return -5;
    handleSD.SDOPT=1;sendcmd(&handleSD,0x10446,2);if(handleSD.error&4)return -6;sendcmd(&handleSD,0x1040D,handleSD.initarg<<16);if(handleSD.error&4)return -7;
    sendcmd(&handleSD,0x10410,0x200);if(handleSD.error&4)return -8;handleSD.clk|=0x200;return 0;
}
