#!/usr/bin/env python3
from pathlib import Path
import argparse, hashlib, struct
ap=argparse.ArgumentParser()
ap.add_argument('--arm9',required=True);ap.add_argument('--arm11',required=True)
ap.add_argument('--arm9-addr',type=lambda x:int(x,0),default=0x08006000)
ap.add_argument('--arm11-addr',type=lambda x:int(x,0),required=True)
ap.add_argument('--data');ap.add_argument('--data-addr',type=lambda x:int(x,0),default=0x23000000)
ap.add_argument('--data2');ap.add_argument('--data2-addr',type=lambda x:int(x,0),default=0x23100000)
ap.add_argument('-o','--out',required=True)
a=ap.parse_args()
def pad(b): return b+b'\0'*((-len(b))%0x200)
a9=pad(Path(a.arm9).read_bytes());a11=pad(Path(a.arm11).read_bytes())
sections=[(a9,a.arm9_addr,0),(a11,a.arm11_addr,1)]
if a.data:
    sections.append((pad(Path(a.data).read_bytes()),a.data_addr,0))
if a.data2:
    sections.append((pad(Path(a.data2).read_bytes()),a.data2_addr,0))
if len(sections)>4:
    raise SystemExit('FIRM supports at most four sections')
h=bytearray(0x200);h[:4]=b'FIRM';struct.pack_into('<I',h,8,a.arm11_addr);struct.pack_into('<I',h,12,a.arm9_addr);h[0x10]=1
o=0x200;payload=[]
for i,(d,addr,method) in enumerate(sections):
    b=0x40+i*0x30
    struct.pack_into('<IIII',h,b,o,addr,len(d),method)
    h[b+0x10:b+0x30]=hashlib.sha256(d).digest()
    payload.append(d);o+=len(d)
Path(a.out).write_bytes(bytes(h)+b''.join(payload))
print(f'{a.out}: {o} bytes, {len(sections)} sections')
