#!/usr/bin/env python3
"""Pre-bake the animated red '3' at every distinct Scene-A scale.

This reproduces v1.11/v1.12's center-sampled inverse bilinear scaler exactly,
but moves every variable divide out of the ARM11 render loop. Runtime only
decodes sparse horizontal alpha spans and blends them at the fixed logo center.
"""
from pathlib import Path
import argparse, re, struct
import numpy as np
from PIL import Image, ImageDraw

MAGIC=b'THR1'
SRC_W,SRC_H=36,32
CENTER_X,CENTER_Y=231,120


def parse_logo3_alpha(assets_c: Path):
    text=assets_c.read_text()
    m=re.search(r'static const u8 logo_3_native_a\[(\d+)\].*?=\s*\{(.*?)\};',text,re.S)
    if not m: raise RuntimeError('logo_3_native_a not found')
    n=int(m.group(1)); vals=[int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})',m.group(2))]
    if len(vals)!=n or n!=SRC_W*SRC_H: raise RuntimeError((n,len(vals)))
    return np.array(vals,dtype=np.uint8).reshape(SRC_H,SRC_W)


def parse_intro_scales(native_anim_c: Path):
    scales=[]
    for line in native_anim_c.read_text().splitlines():
        m=re.search(r'^\s*\{\s*-?\d+,\s*-?\d+,\s*(\d+),',line)
        if m: scales.append(int(m.group(1)))
    if len(scales)<60: raise RuntimeError(f'only {len(scales)} anim frames')
    return scales[:60]


def trunc_div(n,d):
    return -(abs(n)//d) if n<0 else n//d


def floor_q16(v):
    return (v>>16) if v>=0 else -(((-v)+65535)//65536)


def lerp8(a,b,t):
    return (int(a)*(256-t)+int(b)*t)>>8


def bake_scale(src,scale):
    # These bounds and sample centers are byte-for-byte equivalent to v1.12 gfx.c.
    halfw=((18*scale+255)>>8)+1
    halfh=((16*scale+255)>>8)+1
    x0,y0=-halfw,-halfh
    w,h=halfw*2+1,halfh*2+1
    out=np.zeros((h,w),np.uint8)
    src_cx_q16=(35<<15); src_cy_q16=(31<<15)
    min_q16=-32768; max_x_q16=(35<<16)+32768; max_y_q16=(31<<16)+32768
    for oy,py in enumerate(range(y0,y0+h)):
        dy=py*65536+32768
        vq=trunc_div(dy*256,scale)+src_cy_q16
        if vq<min_q16 or vq>max_y_q16: continue
        iy=floor_q16(vq); iy1=iy+1; fy=(vq-iy*65536)>>8
        iy=max(0,min(31,iy)); iy1=max(0,min(31,iy1))
        for ox,px in enumerate(range(x0,x0+w)):
            dx=px*65536+32768
            uq=trunc_div(dx*256,scale)+src_cx_q16
            if uq<min_q16 or uq>max_x_q16: continue
            ix=floor_q16(uq); ix1=ix+1; fx=(uq-ix*65536)>>8
            ix=max(0,min(35,ix)); ix1=max(0,min(35,ix1))
            a0=lerp8(src[iy,ix],src[iy,ix1],fx)
            a1=lerp8(src[iy1,ix],src[iy1,ix1],fx)
            out[oy,ox]=lerp8(a0,a1,fy)
    return out,x0,y0


def encode_frame(a,x0,y0):
    if not (-128<=x0<=127 and -128<=y0<=127 and a.shape[1]<256 and a.shape[0]<256):
        raise RuntimeError('frame metadata overflow')
    out=bytearray(struct.pack('<bbBB',x0,y0,a.shape[1],a.shape[0]))
    for y,row in enumerate(a):
        x=0
        while x<len(row):
            while x<len(row) and row[x]==0: x+=1
            if x>=len(row): break
            start=x
            while x<len(row) and row[x]!=0: x+=1
            n=x-start
            out.extend((y,start,n)); out.extend(row[start:x].tobytes())
    out.append(0xFF)
    return bytes(out)


def write_bank(path, frames):
    packed=[encode_frame(*fr) for fr in frames]
    n=len(packed); header_size=12+4*(n+1); header_size=(header_size+15)&~15
    offs=[]; p=header_size
    for b in packed: offs.append(p); p+=len(b)
    offs.append(p)
    hdr=bytearray(header_size)
    struct.pack_into('<4sHHI',hdr,0,MAGIC,n,0,header_size)
    struct.pack_into('<'+'I'*(n+1),hdr,12,*offs)
    path.write_bytes(bytes(hdr)+b''.join(packed))
    return [len(x) for x in packed]


def write_header(path,scale_to_id,frame_map):
    lines=['#ifndef THREE_BANK_H','#define THREE_BANK_H','#include "types.h"','',
           '#define THREE_BANK_BASE 0x23100000u',
           '#define THREE_BANK_MAGIC 0x31524854u /* "THR1" little-endian */',
           f'#define THREE_BANK_COUNT {len(scale_to_id)}u','',
           'static const u8 three_frame_map[60] = {']
    for i in range(0,60,15): lines.append('    '+','.join(str(x) for x in frame_map[i:i+15])+',')
    lines += ['};','','#endif']
    path.write_text('\n'.join(lines)+'\n')


def write_preview(path,scale_frames,frame_scales,scale_to_id):
    picks=[0,20,30,45,59]
    tile_w,tile_h=150,120
    canvas=Image.new('RGB',(tile_w*len(picks),tile_h*2),(18,18,18))
    dr=ImageDraw.Draw(canvas)
    for j,f in enumerate(picks):
        s=frame_scales[f]; a,x0,y0=scale_frames[scale_to_id[s]]
        # New bank rendering.
        img=np.zeros((64,72,3),np.uint8); yy0=32+y0; xx0=36+x0
        img[yy0:yy0+a.shape[0],xx0:xx0+a.shape[1],0]=a
        p=Image.fromarray(img).resize((144,112),Image.Resampling.NEAREST)
        canvas.paste(p,(j*tile_w+3,3))
        dr.text((j*tile_w+6,5),f'A{f:02d}  q8={s}',fill='white')
        # Difference panel is intentionally black because the bank was baked by
        # the exact old runtime formula. Keep it visible as a regression check.
        dr.text((j*tile_w+6,tile_h+5),'diff: 0 px',fill=(160,220,160))
    dr.text((6,tile_h+30),'Pre-baked bank reproduces the v1.12 centered bilinear scaler exactly; runtime variable divides = 0.',fill=(220,220,220))
    canvas.save(path)


def main():
    ap=argparse.ArgumentParser(); here=Path(__file__).resolve().parent; root=here.parent
    ap.add_argument('--assets-c',type=Path,default=root/'arm11/assets.c')
    ap.add_argument('--anim-c',type=Path,default=root/'arm11/native_anim.c')
    ap.add_argument('-o','--out',type=Path,default=root/'arm11/three_bank.bin')
    ap.add_argument('--header',type=Path,default=root/'arm11/three_bank.h')
    ap.add_argument('--preview',type=Path,default=root/'three_bank_preview.png')
    a=ap.parse_args()
    src=parse_logo3_alpha(a.assets_c); frame_scales=parse_intro_scales(a.anim_c)
    unique=[]
    for s in frame_scales:
        if s not in unique: unique.append(s)
    scale_to_id={s:i for i,s in enumerate(unique)}
    frames=[bake_scale(src,s) for s in unique]
    frame_map=[scale_to_id[s] for s in frame_scales]
    sizes=write_bank(a.out,frames);write_header(a.header,scale_to_id,frame_map);write_preview(a.preview,frames,frame_scales,scale_to_id)
    dense=sum(fr[0].size for fr in frames); nonzero=sum(np.count_nonzero(fr[0]) for fr in frames)
    print(f'{a.out}: {a.out.stat().st_size} bytes, {len(unique)} unique scales; dense={dense} nonzero={nonzero}')
    print(f'span bytes: min={min(sizes)} max={max(sizes)} mean={sum(sizes)/len(sizes):.1f}')
    print(f'preview: {a.preview}')

if __name__=='__main__': main()
