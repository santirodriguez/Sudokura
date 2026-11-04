#!/usr/bin/env python3
"""Dependency-free validation for the bundled DejaVu Sans TrueType font."""
from pathlib import Path
import argparse, struct
p=argparse.ArgumentParser();p.add_argument('font',type=Path);args=p.parse_args();data=args.font.read_bytes()
assert len(data)>100_000,'font is unexpectedly small';assert data[:4] in (b'\0\1\0\0',b'true'), 'not a TrueType sfnt'
count=struct.unpack_from('>H',data,4)[0];tables={}
for i in range(count):
 tag,_,offset,length=struct.unpack_from('>4sIII',data,12+i*16);tables[tag]=(offset,length)
assert b'name' in tables and b'cmap' in tables and b'glyf' in tables,'required TrueType tables missing'
offset,length=tables[b'name'];table=data[offset:offset+length];records=struct.unpack_from('>H',table,2)[0];storage=struct.unpack_from('>H',table,4)[0];names=[]
for i in range(records):
 platform,encoding,language,name_id,size,start=struct.unpack_from('>HHHHHH',table,6+i*12)
 if name_id not in (1,4):continue
 raw=table[storage+start:storage+start+size]
 try:names.append(raw.decode('utf-16-be' if platform in (0,3) else 'latin1'))
 except UnicodeDecodeError:pass
assert any('DejaVu Sans' in name for name in names),names
print(f'validated DejaVu Sans TrueType font ({len(data)} bytes; name/cmap/glyf tables present)')
