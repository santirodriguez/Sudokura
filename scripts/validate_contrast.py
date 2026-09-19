#!/usr/bin/env python3
"""Verify Sudokura's documented WCAG-inspired design contrast targets."""
from pathlib import Path
import re

source=Path("src/sudokura_sdl/01_runtime.inc").read_text(encoding="utf-8")
def theme(name):
    m=re.search(rf"static Theme theme_{name}\(void\)\{{\s*Theme t=\{{(.*?)\n\s*\}};",source,re.S)
    assert m,f"theme_{name} not found"
    vals={}
    for key,r,g,b in re.findall(r"\.([a-z_]+)=\{(\d+),(\d+),(\d+),\d+\}",m.group(1)):
        vals[key]=(int(r),int(g),int(b))
    return vals
def channel(v):
    c=v/255.0
    return c/12.92 if c<=0.04045 else ((c+0.055)/1.055)**2.4
def lum(rgb):
    r,g,b=map(channel,rgb);return .2126*r+.7152*g+.0722*b
def ratio(a,b):
    x,y=lum(a),lum(b);return (max(x,y)+.05)/(min(x,y)+.05)
normal=[("btnfg","btn"),("dim","bg"),("title","bg"),("bg","title"),
        ("palette_fg","palette_bg"),("text_given","board"),
        ("text_edit","board"),("text_wrong","board")]
functional=[("thin","board"),("thick","board"),("sel_outline","bg"),
            ("sel_outline","board")]
for name in ("dark","light"):
    t=theme(name)
    for fg,bg in normal:
        value=ratio(t[fg],t[bg]);assert value>=4.5,(name,fg,bg,value)
    for fg,bg in functional:
        value=ratio(t[fg],t[bg]);assert value>=3.0,(name,fg,bg,value)
print("dark/light contrast targets passed: text >=4.5:1, functional structure >=3:1")
