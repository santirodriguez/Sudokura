#!/usr/bin/env python3
"""Validate exact dimensions and non-blank SDL BMP review frames."""
from pathlib import Path
import argparse, struct

EXPECTED={(640,480),(800,600),(1024,720),(1366,768),(1920,1080),(2560,1440),(3440,1440),(360,640),(390,844),(412,915)}
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);args=p.parse_args()
files=sorted(args.directory.glob('*.bmp'));assert len(files)==40,f"expected 40 BMPs, found {len(files)}"
counts={size:0 for size in EXPECTED}
for path in files:
 data=path.read_bytes();assert data[:2]==b'BM' and len(data)>=54,path
 offset=struct.unpack_from('<I',data,10)[0];width,height=struct.unpack_from('<ii',data,18)
 size=(width,abs(height));assert size in EXPECTED,(path,size);counts[size]+=1
 pixels=data[offset:];assert len(pixels)>width*abs(height),path
 # A rendered frame must contain materially different channel values.
 assert len(set(pixels[::max(1,len(pixels)//10000)]))>=6,f"blank-looking frame: {path}"
assert all(value==4 for value in counts.values()),counts
print('validated 40 non-blank BMPs with exact dimensions:',', '.join(f'{w}x{h}' for w,h in sorted(EXPECTED)))
