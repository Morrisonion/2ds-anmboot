#include "fat32.h"
#include "sdmmc.h"
#include "minilib.h"
static u8 sec[512] __attribute__((aligned(4)));
static u32 fat,data,root,spc;
static u16 le16(const u8*p){return (u16)p[0]|((u16)p[1]<<8);}static u32 le32(const u8*p){return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);} 
static int rd(u32 lba){return sdmmc_sdcard_readsectors(lba,1,sec);} 
static int is_pow2(u8 x){return x && !(x&(x-1));}
static u8 lower(u8 c){return (c>='A'&&c<='Z')?(u8)(c+32):c;}
int fat32_init(void){
    if(rd(0))return -1;u32 base=0;u16 bps=le16(sec+11);u8 sc=sec[13];
    if(!(bps==512 && is_pow2(sc))){
        u8 type=sec[0x1C2];if(!(type==0x0B||type==0x0C||type==0x0E||type==0x07))return -2;
        base=le32(sec+0x1C6);if(!base||rd(base))return -3;bps=le16(sec+11);sc=sec[13];
    }
    if(bps!=512||!is_pow2(sc))return -4;
    u16 rsv=le16(sec+14);u8 nf=sec[16];u16 roots=le16(sec+17);u16 fs16=le16(sec+22);u32 fs32=le32(sec+36);
    if(roots!=0||fs16!=0||fs32==0||nf==0)return -5;
    spc=sc;fat=base+rsv;data=fat+(u32)nf*fs32;root=le32(sec+44);if(root<2)return -6;return 0;
}
static u32 cl_lba(u32 c){return data+(c-2)*spc;}
static u32 next_cl(u32 c){u32 off=c<<2;if(rd(fat+(off>>9)))return 0x0FFFFFFF;return le32(sec+(off&511))&0x0FFFFFFF;}
static u32 ent_cl(const u8*e){return ((u32)le16(e+20)<<16)|le16(e+26);} 
static int sfn_eq(const u8*e,const char*base,const char*ext){
    int i=0;
    for(;i<8&&base[i];i++)if(lower(e[i])!=(u8)base[i])return 0;
    if(base[i])return 0;
    for(;i<8;i++)if(e[i]!=' ')return 0;
    int j=0;
    for(;j<3&&ext[j];j++)if(lower(e[8+j])!=(u8)ext[j])return 0;
    if(ext[j])return 0;
    for(;j<3;j++)if(e[8+j]!=' ')return 0;
    return 1;
}
static void lfn_piece(char *name,const u8*e){
    int ord=(e[0]&0x1F);if(ord<1||ord>20)return;int pos=(ord-1)*13;static const u8 ofs[13]={1,3,5,7,9,14,16,18,20,22,24,28,30};
    for(int i=0;i<13;i++){u16 ch=le16(e+ofs[i]);if(ch==0||ch==0xFFFF){if(pos+i<255)name[pos+i]=0;break;}if(pos+i<255)name[pos+i]=(ch<128)?(char)lower((u8)ch):'?';}
}
static int name_eq(const char*a,const char*b){while(*a&&*b){if(lower((u8)*a)!=(u8)*b)return 0;a++;b++;}return *a==0&&*b==0;}
static int find_entry(u32 dircl,const char*longname,const char*sbase,const char*sext,u8 want_dir,u32*outcl,u32*outsz){
    char lfn[256];memset(lfn,0,sizeof(lfn));int have_lfn=0;u32 c=dircl;
    for(u32 guard=0;guard<0x10000&&c>=2&&c<0x0FFFFFF8;guard++){
        for(u32 s=0;s<spc;s++){
            if(rd(cl_lba(c)+s))return -1;
            for(u32 o=0;o<512;o+=32){u8*e=sec+o;if(e[0]==0)return -2;if(e[0]==0xE5){have_lfn=0;memset(lfn,0,sizeof(lfn));continue;}
                if(e[11]==0x0F){if(e[0]&0x40){memset(lfn,0,sizeof(lfn));have_lfn=1;}if(have_lfn)lfn_piece(lfn,e);continue;}
                u8 isdir=!!(e[11]&0x10);int match=0;if(have_lfn&&longname)match=name_eq(lfn,longname);if(!match&&sbase)match=sfn_eq(e,sbase,sext?sext:"");
                if(match&&isdir==want_dir){*outcl=ent_cl(e);if(outsz)*outsz=le32(e+28);return 0;}have_lfn=0;memset(lfn,0,sizeof(lfn));
            }
        }c=next_cl(c);
    }return -3;
}
int fat32_load_anmboot_boot(void *dst,u32 max,u32*out_size){
    u32 dcl=0,fcl=0,sz=0;if(find_entry(root,"anmboot","anmboot","",1,&dcl,0))return -1;
    if(find_entry(dcl,"boot.firm","boot","firm",0,&fcl,&sz))return -2;if(!sz||sz>max)return -3;
    u8*out=(u8*)dst;u32 left=sz,c=fcl,guard=0;
    while(left&&c>=2&&c<0x0FFFFFF8&&guard++<0x10000){
        u32 lba=cl_lba(c);
        /* Read whole sectors directly into FCRAM in one multi-block command.
           v0.4 issued one SD command per 512 bytes, which can take seconds for Luma. */
        u32 full=left>>9;if(full>spc)full=spc;
        if(full){
            if(sdmmc_sdcard_readsectors(lba,full,out))return -4;
            u32 n=full<<9;out+=n;left-=n;lba+=full;
        }
        if(left && full<spc && left<512){
            if(rd(lba))return -4;memcpy(out,sec,left);out+=left;left=0;
        }
        if(left)c=next_cl(c);
    }
    if(left)return -5;*out_size=sz;return 0;
}
