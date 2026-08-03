#!/usr/bin/env python3
"""Validate generated resources using only the Python standard library."""
from pathlib import Path
import re, struct

root=Path(__file__).resolve().parents[1]/"assets/generated"
for n in (16,32,48,64,128,256,512,1024):
 data=(root/f"sudokura-{n}.png").read_bytes(); assert data[:8]==b"\x89PNG\r\n\x1a\n"
 assert struct.unpack(">II",data[16:24])==(n,n)
ico=(root/"sudokura.ico").read_bytes();assert ico[:4]==b"\0\0\1\0" and struct.unpack("<H",ico[4:6])[0]==6
icns=(root/"sudokura.icns").read_bytes();assert icns[:4]==b"icns" and struct.unpack(">I",icns[4:8])[0]==len(icns)
source=(root/"window_icon.c").read_text();values=re.findall(r"(?<![A-Za-z_])([0-9]+),",source.split("={",1)[1]);assert len(values)==128*128*4
word=(root/"wordmark.c").read_text(); match=re.search(r"wordmark_width=(\d+),sudokura_wordmark_height=(\d+)",word); assert match and int(match.group(1))==384 and int(match.group(2))>0
values=re.findall(r"(?<![A-Za-z_])([0-9]+),",word.split("[]={",1)[1]);assert len(values)==384*int(match.group(2))*4
print(f"validated 8 PNG sizes, 6-size ICO, ICNS, 128x128 icon and 384x{match.group(2)} wordmark RGBA resources")
