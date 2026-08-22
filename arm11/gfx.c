#include "gfx.h"
#include "assets.h"
#include "native_anim.h"
#ifndef ANMBOOT_OLD3DS
#include "official_glow.h"
#endif
#include "three_bank.h"

/* Use Luma's own normal bare-metal framebuffer slots. Modern Luma also packs its
   K11 extension at 0x18000000, so v0.7's "low VRAM" animation buffers collided
   with a real target FIRM section and were rejected before Luma could start. */
#define FB_A 0x18300000u
#define FB_B 0x18400000u
#define TOP_SIZE (400u*240u*3u)
#define BOTTOM_SIZE (320u*240u*3u)
#define BOTTOM_OFF TOP_SIZE
#define TOTAL_SIZE (TOP_SIZE+BOTTOM_SIZE)
#define REG_PSC0 ((vu32*)0x10400010u)
#define REG_PDC0 ((vu32*)0x10400400u)
#define REG_PDC1 ((vu32*)0x10400500u)
#define VCOUNT   (REG_PDC0[0x54/4] & 0xFFFu)
#define VBLANK_START 0x192u

static u32 cur;

static void fill_region(u32 start,u32 len,u32 v){
    REG_PSC0[0]=start>>3;REG_PSC0[1]=(start+len)>>3;REG_PSC0[2]=v;REG_PSC0[3]=(2u<<8)|1u;
    while(REG_PSC0[3]&1u){}
}
static void setup_bgr8(void){
    REG_PDC0[26]=FB_A;REG_PDC0[27]=FB_B;REG_PDC0[37]=FB_A;REG_PDC0[38]=FB_B;
    REG_PDC1[26]=FB_A+BOTTOM_OFF;REG_PDC1[27]=FB_B+BOTTOM_OFF;
    REG_PDC0[28]=0x80341u;REG_PDC0[30]=0;REG_PDC0[36]=0x2D0u;
    REG_PDC1[28]=0x80301u;REG_PDC1[30]=0;REG_PDC1[36]=0x2D0u;
    cur=0;
}
static inline u32 fb_index(int x,int y){return ((u32)x*240u+(239u-(u32)y))*3u;}
static inline int q8_round(s16 v){return v>=0?(v+128)/256:(v-128)/256;}
static inline void blend_px(u8*fb,int x,int y,u8 r,u8 g,u8 b,u32 a){
    if(!a)return;if(a>255)a=255;u32 di=fb_index(x,y),ia=255u-a;
    fb[di+0]=(u8)(((u32)b*a+(u32)fb[di+0]*ia+127u)/255u);
    fb[di+1]=(u8)(((u32)g*a+(u32)fb[di+1]*ia+127u)/255u);
    fb[di+2]=(u8)(((u32)r*a+(u32)fb[di+2]*ia+127u)/255u);
}
static void draw_native_crop(u8*fb,int sw,const AlphaTile*t,int x0,int y0,
                             int cx0,int cy0,int cx1,int cy1,
                             u8 r,u8 g,u8 b,u32 ga){
    if(!ga)return;
    if(cx0<0)cx0=0;if(cy0<0)cy0=0;
    if(cx1>(int)t->w)cx1=t->w;if(cy1>(int)t->h)cy1=t->h;
    for(int y=cy0;y<cy1;y++){
        int sy=y0+y;if((unsigned)sy>=240u)continue;
        const u8*src=t->a+(u32)y*t->w;
        for(int x=cx0;x<cx1;x++){
            int sx=x0+x;if((unsigned)sx>=(unsigned)sw)continue;
            u32 a=((u32)src[x]*ga+127u)/255u;
            if(a)blend_px(fb,sx,sy,r,g,b,a);
        }
    }
}
/* v1.13: the animated red "3" is pre-baked offline at all 39 distinct
   Scene-A scale values.  This preserves v1.12's centered bilinear result
   pixel-for-pixel while removing the per-pixel variable integer divides that
   were expensive on ARM11 MPCore.  Runtime now touches only non-zero spans. */
static void draw_baked3(u8 *fb,u32 anim_frame,u8 ga){
    if(!ga)return;
    if(anim_frame>59u)anim_frame=59u;
    const u8 *base=(const u8*)THREE_BANK_BASE;
    if(*(const vu32*)base!=THREE_BANK_MAGIC)return;
    u32 id=three_frame_map[anim_frame];
    if(id>=THREE_BANK_COUNT)return;
    const u32 *off=(const u32*)(base+12u);
    const u8 *p=base+off[id],*end=base+off[id+1u];
    if(p+4u>end)return;
    signed char x0=(signed char)*p++,y0=(signed char)*p++;u32 w=*p++,h=*p++;
    (void)w;
    while(p<end){
        u32 y=*p++;
        if(y==0xFFu)break;
        if(p+2u>end)break;
        u32 x=*p++,n=*p++;
        if(y>=h || p+n>end)break;
        int sy=120+(int)y0+(int)y;
        int sx=231+(int)x0+(int)x;
        while(n--){
            u32 aa=(u32)(*p++)*ga/255u;
            if(aa && (unsigned)sx<400u && (unsigned)sy<240u)
                blend_px(fb,sx,sy,212,0,0,aa);
            sx++;
        }
    }
}

#ifndef ANMBOOT_OLD3DS
/* v1.19: stock-like glow from raw BCLIM, still stored as 200x120 sparse spans.
   Runtime expands only non-zero samples to 2x2 cells. The offline tail now uses
   the real continuing Scene-B motion instead of a global synchronized fade, and
   low-intensity half-res samples are culled more aggressively for performance. */
static void draw_official_glow(u8 *fb,u32 frame_index){
    const u8 *base=(const u8*)OFFICIAL_GLOW_BASE;
    if(*(const vu32*)base!=OFFICIAL_GLOW_MAGIC || frame_index>=OFFICIAL_GLOW_COUNT)return;
    const u32 *off=(const u32*)(base+16u);
    const u8 *p=base+off[frame_index],*end=base+off[frame_index+1u];
    while(p<end){
        u32 hx=*p++;
        if(hx==0xFFu)break;
        if(p+2>end)break;
        u32 hy=*p++,n=*p++;
        if(hx>=OFFICIAL_GLOW_HALF_W || hy>=OFFICIAL_GLOW_HALF_H)break;
        if(hy+n>OFFICIAL_GLOW_HALF_H)n=OFFICIAL_GLOW_HALF_H-hy;
        if(p+n>end)n=(u32)(end-p);
        u8 *d0=fb+fb_index((int)(hx<<1),(int)(hy<<1))+2u;
        u8 *d1=d0+720;
        while(n--){
            u8 v=*p++;
            d0[0]=v;d0[-3]=v;d1[0]=v;d1[-3]=v;
            d0-=6;d1-=6;
        }
    }
}

#endif

static void draw_logo(u32 base,const NativeAnimFrame*f,int glow_frame){
    u8*top=(u8*)base,*bot=(u8*)(base+BOTTOM_OFF);
    u32 anim_frame=(u32)(f-native_anim);

    /* Both lower-screen buffers have received the fully opaque Nintendo logo by
       the end of intro frame 16 (frame 15 fills one buffer, frame 16 the other).
       From frame 17 onward the lower image never changes, so leave it completely
       untouched.  This removes a full 320x240 clear plus an 128x64 alpha pass
       exactly during the expensive glow + red-3 overlap window. */
    int preserve_bottom=(anim_frame>=17u);
    fill_region(base,preserve_bottom?TOP_SIZE:TOTAL_SIZE,0);

#ifndef ANMBOOT_OLD3DS
    if(glow_frame>=0)draw_official_glow(top,(u32)glow_frame);
#else
    (void)glow_frame;
#endif
    u32 na=(u32)f->nin_a*f->root_a/255u,da=(u32)f->ds_a*f->root_a/255u,ta=(u32)f->three_a*f->root_a/255u;
    int nx=q8_round(f->nin_x_q8),dx=q8_round(f->ds_x_q8);

    /* Skip transparent margins in the source alpha tiles.  These crop boxes
       are exact non-zero bounds, so this is pixel-identical to the old loops. */
    draw_native_crop(top,400,&logo_nintend,72+nx,104, 9,1,121,16, 255,255,255,na);
    draw_native_crop(top,400,&logo_o_top,184+nx,104, 11,1,28,15, 255,255,255,na);
    draw_native_crop(top,400,&logo_o_bottom,184+nx,120, 11,1,28,15, 140,140,140,na);
    draw_baked3(top,anim_frame,(u8)ta);
    draw_native_crop(top,400,&logo_ds,200+dx,104, 50,1,119,31, 255,255,255,da);

    if(!preserve_bottom)
        draw_native_crop(bot,320,&logo_nintendo_bottom,97,79, 23,31,105,53, 255,255,255,f->bottom_a);
}

/* The v0.6 diagonal half-logo flicker was a scanout tear: framebuffer select was
   changed in the middle of LCD scanning. Wait for a new VBlank before every swap. */
static void present_vblank(u32 next){
    while(VCOUNT>=VBLANK_START){}
    while(VCOUNT<VBLANK_START){}
    cur=next;REG_PDC0[30]=cur;REG_PDC1[30]=cur;
}

static void render(const NativeAnimFrame*f,int glow){u32 next=cur^1u;draw_logo(next?FB_B:FB_A,f,glow);present_vblank(next);}

static void wait_one_vblank(void){while(VCOUNT>=VBLANK_START){} while(VCOUNT<VBLANK_START){} }

void gfx_seal_static_logo(void){
    /* Never rewrite the framebuffer currently being scanned. v1.20 rendered
       both FB_A and FB_B back-to-back; the second one could be the live scanout,
       producing the half-dark S / left-right brightness split and a lower-panel
       flash while PSC cleared and software redrew it.

       Instead: render the final clean logo only into the hidden buffer, swap on
       VBlank, then render the now-hidden old buffer. Both buffers end identical,
       while the visible one is never modified mid-scan. */
    u32 hidden=cur^1u;
    draw_logo(hidden?FB_B:FB_A,&native_anim[60],-1);
    present_vblank(hidden);
    hidden=cur^1u;
    draw_logo(hidden?FB_B:FB_A,&native_anim[60],-1);
    wait_one_vblank();
}

void gfx_init(void){fill_region(FB_A,TOTAL_SIZE,0);fill_region(FB_B,TOTAL_SIZE,0);setup_bgr8();}
void gfx_draw_intro_frame(int frame){
    if(frame<0)frame=0;if(frame>59)frame=59;
#ifdef ANMBOOT_OLD3DS
    render(&native_anim[frame],-1);
#else
    render(&native_anim[frame],frame);
#endif
}
#ifndef ANMBOOT_OLD3DS
void gfx_draw_glow_frame(int frame_b){if(frame_b<0)frame_b=0;frame_b%=30;render(&native_anim[60],60+frame_b);}
void gfx_draw_tail_frame(int frame_t){if(frame_t<0)frame_t=0;if(frame_t>44)frame_t=44;render(&native_anim[60],90+frame_t);}
#else
void gfx_draw_glow_frame(int frame_b){(void)frame_b;}
void gfx_draw_tail_frame(int frame_t){(void)frame_t;}
#endif
#ifdef ANMBOOT_OLD3DS
void gfx_idle_vblank(void){wait_one_vblank();}
#endif
void gfx_black(void){u32 next=cur^1u;fill_region(next?FB_B:FB_A,TOTAL_SIZE,0);present_vblank(next);}
void gfx_error(u32 code){
    u8 r=255,g=0,b=0;if(code==0xE2){r=0;g=255;b=0;}else if(code==0xE3){r=0;g=80;b=255;}else if(code==0xE4){r=255;g=220;b=0;}else if(code==0xE5){r=255;g=0;b=255;}
    for(int k=0;k<2;k++){u8*fb=(u8*)(k?FB_B:FB_A);fill_region((u32)fb,TOTAL_SIZE,0);for(int x=0;x<400;x+=2)for(int y=0;y<240;y+=2)for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++)blend_px(fb,x+dx,y+dy,r,g,b,255);}
    REG_PDC0[30]=0;REG_PDC1[30]=0;
}
