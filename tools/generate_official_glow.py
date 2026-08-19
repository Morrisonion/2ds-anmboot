#!/usr/bin/env python3
from pathlib import Path
import argparse, struct
import numpy as np
from PIL import Image, ImageDraw

W,H=400,240
HW,HH=200,120
MAGIC=b'GLG2'
TAIL_START=30
TAIL_END=74
HALF_THRESHOLD=5


TILE=[0,1,4,5,2,3,6,7,8,9,12,13,10,11,14,15]
def decode_bclim(path):
    """Decode the original CTR BCLIM directly.

    This is intentionally not routed through the old PNG conversion.  The
    v1.13 approved glow was produced from the raw BCLIMs; LT_01 and LTMask PNGs
    were later found to have wrong channel/content decoding and reintroduced
    the square black cut-outs in v1.14+.
    """
    d=Path(path).read_bytes()
    off=len(d)-0x28
    w,h,fmt=struct.unpack_from('<HHI',d,off+0x1c)
    n=struct.unpack_from('<I',d,off+0x24)[0]
    raw=d[:n]
    out=np.zeros((h,w,4),np.uint8)
    p=0
    if fmt==3: # LA8: byte0 alpha, byte1 luminance
        for y in range(0,h,8):
            for x in range(0,w,8):
                for i in range(64):
                    x2=i%8; y2=i//8
                    pos=TILE[x2%4+y2%4*4]+16*(x2//4)+32*(y2//4)
                    A=raw[p+pos*2]; L=raw[p+pos*2+1]
                    out[y+y2,x+x2]=[L,L,L,A]
                p+=128
    elif fmt==13: # A4
        for y in range(0,h,8):
            for x in range(0,w,8):
                for i in range(64):
                    x2=i%8; y2=i//8
                    pos=TILE[x2%4+y2%4*4]+16*(x2//4)+32*(y2//4)
                    shift=(pos&1)*4
                    A=((raw[p+pos//2]>>shift)&15)*17
                    out[y+y2,x+x2]=[255,255,255,A]
                p+=32
    else:
        raise ValueError(f'unsupported BCLIM format {fmt}')
    return out.astype(np.float32)/255.0

def read_sections(data):
    hdr=struct.unpack_from('<H',data,6)[0]; count=struct.unpack_from('<H',data,16)[0]
    pos=hdr; out={}
    for _ in range(count):
        sig=data[pos:pos+4].decode('ascii'); size=struct.unpack_from('<I',data,pos+4)[0]
        out.setdefault(sig,[]).append((pos,size)); pos+=size
    return out

def parse_bclan(path):
    data=Path(path).read_bytes(); sec=read_sections(data); pai=sec['pai1'][0][0]
    frame_size,loop,_,ntex,nent,table=struct.unpack_from('<HBBHHI',data,pai+8)
    ent_offsets=struct.unpack_from('<'+'I'*nent,data,pai+table)
    entries=[]
    for eo in ent_offsets:
        ep=pai+eo; name=data[ep:ep+20].split(b'\0')[0].decode('ascii'); ntag=data[ep+20]; kind=data[ep+21]
        tag_offsets=struct.unpack_from('<'+'I'*ntag,data,ep+24); tags=[]
        for to in tag_offsets:
            tp=ep+to; sig=data[tp:tp+4].decode('ascii'); nte=data[tp+4]
            te_offsets=struct.unpack_from('<'+'I'*nte,data,tp+8); tes=[]
            for oo in te_offsets:
                kp=tp+oo; index,target,curve,_=struct.unpack_from('<BBBB',data,kp)
                nk=struct.unpack_from('<H',data,kp+4)[0]; ko=struct.unpack_from('<I',data,kp+8)[0]; q=kp+ko; keys=[]
                for _ in range(nk):
                    if curve==2:
                        fr,val,slope=struct.unpack_from('<fff',data,q); q+=12
                    else:
                        fr,val=struct.unpack_from('<ff',data,q); slope=0.; q+=8
                    keys.append((fr,val,slope))
                tes.append((index,target,curve,keys))
            tags.append((sig,tes))
        entries.append((name,kind,tags))
    return {'frames':frame_size,'loop':loop,'entries':entries}

def hermite(keys,frame):
    if frame<=keys[0][0]: return keys[0][1]
    if frame>=keys[-1][0]: return keys[-1][1]
    exact=[k for k in keys if k[0]==frame]
    if exact:return exact[-1][1]
    i=max(i for i,k in enumerate(keys) if k[0]<frame)
    while i+1<len(keys) and keys[i+1][0]<frame:i+=1
    j=i+1
    while j<len(keys) and keys[j][0]==keys[i][0]:j+=1
    f0,v0,m0=keys[i]; f1,v1,m1=keys[j]; d=f1-f0; t=(frame-f0)/d; t2=t*t; t3=t2*t
    return ((2*t3-3*t2+1)*v0+(t3-2*t2+t)*m0*d+(-2*t3+3*t2)*v1+(t3-t2)*m1*d)

def find_track(scene,name,kind,tag_sig,target,index=0):
    for ename,ekind,tags in scene['entries']:
        if ename!=name or ekind!=kind: continue
        for sig,tes in tags:
            if sig!=tag_sig: continue
            for idx,tgt,_curve,keys in tes:
                if idx==index and tgt==target: return keys
    raise KeyError((name,kind,tag_sig,target,index))

def parse_glow_uvs(path):
    data=Path(path).read_bytes(); sec=read_sections(data); out={}
    for pos,_ in sec.get('pic1',[]):
        name=data[pos+12:pos+36].split(b'\0')[0].decode('ascii')
        if not(name.startswith('P_Red_') or name.startswith('P_Blk_')): continue
        alpha=data[pos+10]; q=pos+92; _mat,nuv=struct.unpack_from('<HH',data,q); q+=4; uvs=[]
        for _ in range(nuv): uvs.append(struct.unpack_from('<8f',data,q)); q+=32
        out[name]=(alpha,uvs)
    return out

def mirror_repeat(t):
    m=np.mod(t,2.0)
    return np.where(m<=1.0,m,2.0-m)

def sample_linear(tex,u,v,wrap='clamp'):
    if wrap=='mirror': u,v=mirror_repeat(u),mirror_repeat(v)
    th,tw,_=tex.shape; xf=u*tw-.5; yf=v*th-.5
    x0=np.floor(xf).astype(np.int32); y0=np.floor(yf).astype(np.int32)
    fx=(xf-x0).astype(np.float32); fy=(yf-y0).astype(np.float32)
    x1=x0+1; y1=y0+1
    x0=np.clip(x0,0,tw-1); x1=np.clip(x1,0,tw-1); y0=np.clip(y0,0,th-1); y1=np.clip(y1,0,th-1)
    a,b=tex[y0,x0],tex[y0,x1]; c,d=tex[y1,x0],tex[y1,x1]
    top=a*(1-fx[...,None])+b*fx[...,None]; bot=c*(1-fx[...,None])+d*fx[...,None]
    return top*(1-fy[...,None])+bot*fy[...,None]

def tex_srt(u,v,translate,scale):
    return ((u-.5)*scale+.5+translate,(v-.5)*scale+.5+translate)

def smoothstep(x):
    x=np.clip(x,0.,1.)
    return x*x*(3.-2.*x)

def life_gain(T,layer):
    end=76+30*layer; start=end-12
    if T<=start:return 1.
    if T>=end:return 0.
    return 1.-smoothstep((T-start)/(end-start))

def bake_scene_range(scene,textures,pane_uvs,frame_start,frame_end,global_base=0,apply_life=False):
    lt0,lt1,mask=textures; uv0=pane_uvs['P_Red_00'][1][0]; uv1=pane_uvs['P_Red_00'][1][1]
    x=(np.arange(W,dtype=np.float32)+.5)/W; y=(np.arange(H,dtype=np.float32)+.5)/H
    u0=np.broadcast_to(uv0[0]+(uv0[2]-uv0[0])*x[None,:],(H,W)); v0=np.broadcast_to(uv0[1]+(uv0[5]-uv0[1])*y[:,None],(H,W))
    u1=np.broadcast_to(uv1[0]+(uv1[2]-uv1[0])*x[None,:],(H,W)); v1=np.broadcast_to(uv1[1]+(uv1[5]-uv1[1])*y[:,None],(H,W))
    mask_a=sample_linear(mask,u1,v1,'mirror')[...,3]
    frames=[]
    for frame in range(frame_start,frame_end+1):
        T=global_base+frame
        dst=np.zeros((H,W),dtype=np.float32)
        for layer in range(3):
            rn=f'P_Red_0{layer}'; bn=f'P_Blk_0{layer}'
            pa=np.clip(hermite(find_track(scene,rn,0,'CLVC',16,0),frame)/255.,0,1)
            if apply_life: pa*=life_gain(T,layer)
            rt=hermite(find_track(scene,rn,1,'CLTS',0,0),frame); rs=hermite(find_track(scene,rn,1,'CLTS',3,0),frame)
            rt1=hermite(find_track(scene,rn,1,'CLTS',0,1),frame); rs1=hermite(find_track(scene,rn,1,'CLTS',3,1),frame)
            ru0,rv0=tex_srt(u0,v0,rt,rs); ru1,rv1=tex_srt(u0,v0,rt1,rs1)
            t0=sample_linear(lt0,ru0,rv0,'clamp'); t1=sample_linear(lt0,ru1,rv1,'clamp')
            sr=np.clip(t0[...,0]*t1[...,0]-mask_a,0,1)
            sa=np.clip(t0[...,3]+t1[...,3],0,1)*(64./255.)*pa
            dst=sr*sa+dst*(1-sa)
            bt=hermite(find_track(scene,bn,1,'CLTS',0,0),frame); bs=hermite(find_track(scene,bn,1,'CLTS',3,0),frame)
            bt1=hermite(find_track(scene,bn,1,'CLTS',0,1),frame); bs1=hermite(find_track(scene,bn,1,'CLTS',3,1),frame)
            bu0,bv0=tex_srt(u0,v0,bt,bs); bu1,bv1=tex_srt(u0,v0,bt1,bs1)
            q0=sample_linear(lt1,bu0,bv0,'clamp'); q1=sample_linear(lt1,bu1,bv1,'clamp')
            dst*=1-np.clip(q0[...,3]+q1[...,3],0,1)
        frames.append(np.rint(np.clip(dst,0,1)*255).astype(np.uint8))
    return frames

def downsample_half(frame):
    q=frame.astype(np.uint16).reshape(HH,2,HW,2).sum(axis=(1,3))
    q=((q+2)//4).astype(np.uint8)
    q[q<HALF_THRESHOLD]=0
    return q

def encode_vertical_runs(frame):
    out=bytearray()
    for hx in range(HW):
        col=frame[:,hx]
        hy=0
        while hy<HH:
            while hy<HH and col[hy]==0: hy+=1
            if hy>=HH: break
            start=hy
            while hy<HH and col[hy]!=0: hy+=1
            n=hy-start
            out.extend(bytes((hx,start,n)))
            out.extend(col[start:hy].tobytes())
    out.extend(b'\xFF')
    return bytes(out)

def write_blob(path,a,b,f):
    frames=a+b+f; packed=[encode_vertical_runs(x) for x in frames]; n=len(frames)
    header_size=16+4*(n+1); header_size=(header_size+15)&~15
    offs=[]; pos=header_size
    for p in packed: offs.append(pos); pos+=len(p)
    offs.append(pos)
    hdr=bytearray(header_size)
    struct.pack_into('<4sHHHHHH',hdr,0,MAGIC,HW,HH,len(a),len(b),len(f),0)
    struct.pack_into('<'+'I'*(n+1),hdr,16,*offs)
    Path(path).write_bytes(bytes(hdr)+b''.join(packed))
    return [len(x) for x in packed]

def write_preview(path,b,t):
    picks=[10,16,22,29]
    tail_picks=[0,7,14,21,29,37,44]
    canvas=Image.new('RGB',(W*2,H*6),(0,0,0))
    for i,idx in enumerate(picks):
        rgb=np.zeros((H,W,3),np.uint8); rgb[...,0]=np.repeat(np.repeat(b[idx],2,0),2,1)
        im=Image.fromarray(rgb); ImageDraw.Draw(im).text((5,5),f'B{idx:02d}',fill='white')
        canvas.paste(im,((i%2)*W,(i//2)*H))
    row_off=2*H
    for i,idx in enumerate(tail_picks):
        rgb=np.zeros((H,W,3),np.uint8); rgb[...,0]=np.repeat(np.repeat(t[idx],2,0),2,1)
        im=Image.fromarray(rgb); ImageDraw.Draw(im).text((5,5),f'T{idx:02d}',fill='white')
        canvas.paste(im,((i%2)*W,row_off+(i//2)*H))
    canvas.save(path)

def main():
    ap=argparse.ArgumentParser(); here=Path(__file__).resolve().parent
    ap.add_argument('--assets',type=Path,default=here/'official_assets')
    ap.add_argument('-o','--out',type=Path,default=here.parent/'arm11'/'official_glow.bin')
    ap.add_argument('--preview',type=Path,default=here.parent/'official_glow_preview.png')
    a=ap.parse_args()
    panes=parse_glow_uvs(a.assets/'NintendoLogo_U_00.bclyt')
    sa=parse_bclan(a.assets/'NintendoLogo_U_00_SceneOutA.bclan')
    sb=parse_bclan(a.assets/'NintendoLogo_U_00_SceneOutB.bclan')
    textures=tuple(decode_bclim(a.assets/n) for n in ('LT_00.bclim','LT_01.bclim','LTMask_00.bclim'))
    full_a=bake_scene_range(sa,textures,panes,0,59,0,False)
    full_b=bake_scene_range(sb,textures,panes,0,29,60,True)
    full_t=bake_scene_range(sb,textures,panes,TAIL_START,TAIL_END,60,True)
    half_a=[downsample_half(x) for x in full_a]
    half_b=[downsample_half(x) for x in full_b]
    half_f=[downsample_half(x) for x in full_t]
    sizes=write_blob(a.out,half_a,half_b,half_f)
    write_preview(a.preview,half_b,half_f)
    print(f'{a.out}: {a.out.stat().st_size} bytes; A={len(half_a)} B={len(half_b)} T={len(half_f)} halfres vertical sparse + raw BCLIM + natural tail + threshold5')
    print(f'span frame bytes: min={min(sizes)} max={max(sizes)} mean={sum(sizes)/len(sizes):.1f}')
    print(f'preview: {a.preview}')

if __name__=='__main__':
    main()
