#!/usr/bin/env python3
"""Reproducibly derive lossless application icons from immutable Sudokura05.png."""
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]; source=root/'assets/branding/source/Sudokura05.png'; out=root/'assets/generated'; out.mkdir(parents=True,exist_ok=True)
im=Image.open(source).convert('RGBA'); px=im.load()
for y in range(im.height):
 for x in range(im.width):
  if px[x,y][3] <= 8: px[x,y]=(0,0,0,0)
bbox=im.getbbox();
if not bbox: raise SystemExit('source artwork has no visible pixels')
im=im.crop(bbox); side=max(im.size); canvas=Image.new('RGBA',(side,side)); canvas.alpha_composite(im,((side-im.width)//2,(side-im.height)//2)); padded=Image.new('RGBA',(side*10//8,side*10//8));padded.alpha_composite(canvas,((padded.width-side)//2,(padded.height-side)//2))
sizes=(16,32,48,64,128,256,512,1024); icons=[]
for n in sizes:
 icon=padded.resize((n,n),Image.Resampling.LANCZOS); icon.save(out/f'sudokura-{n}.png',optimize=True,compress_level=9);icons.append(icon)
icons[-1].save(out/'sudokura.ico',format='ICO',sizes=[(n,n) for n in sizes if n<=256])
# Pillow writes a valid single-image ICNS; macOS packaging uses it directly.
icons[-1].save(out/'sudokura.icns',format='ICNS')
rgba=icons[4].tobytes();
with (out/'window_icon.c').open('w') as f:
 f.write('#include "window_icon.h"\nconst unsigned int sudokura_icon_width=128,sudokura_icon_height=128;\nconst unsigned char sudokura_icon_rgba[]={')
 for i,b in enumerate(rgba): f.write(('\n' if i%20==0 else '')+str(b)+',')
 f.write('\n};\n')
(out/'window_icon.h').write_text('#ifndef WINDOW_ICON_H\n#define WINDOW_ICON_H\nextern const unsigned int sudokura_icon_width,sudokura_icon_height;\nextern const unsigned char sudokura_icon_rgba[];\n#endif\n')
print('generated:', ', '.join(p.name for p in sorted(out.iterdir())))
