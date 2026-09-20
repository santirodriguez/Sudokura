#!/usr/bin/env python3
"""Validate the Phase 5 diagnostic UI review matrix and semantic manifest."""
from pathlib import Path
import argparse,csv,struct

SIZES={(640,480),(1024,768),(1366,768),(360,640),(1920,1080)}
STATES={"home-empty","home-save","notes","conflict","hint","save-error",
        "pause","loss","win","help","settings","about"}
LANGS={"English","Español","Català"}
THEMES={"dark","light"}

def crop_colors(path,x,y,w,h):
    data=path.read_bytes()
    offset=struct.unpack_from("<I",data,10)[0]
    bw,bh=struct.unpack_from("<ii",data,18)
    bpp=struct.unpack_from("<H",data,28)[0]
    assert bpp in (24,32),(path.name,"unsupported BMP depth",bpp)
    bytes_per_pixel=bpp//8
    stride=((abs(bw)*bytes_per_pixel+3)//4)*4
    colors=set()
    for logical_y in range(y,y+h):
        stored_y=abs(bh)-1-logical_y if bh>0 else logical_y
        row=offset+stored_y*stride
        for logical_x in range(x,x+w):
            pixel=row+logical_x*bytes_per_pixel
            colors.add(bytes(data[pixel:pixel+3]))
    return colors

p=argparse.ArgumentParser();p.add_argument("directory",type=Path);args=p.parse_args()
manifest=args.directory/"MANIFEST.tsv";assert manifest.exists(),manifest
with manifest.open(encoding="utf-8",newline="") as f:
    rows=list(csv.DictReader(f,delimiter="\t"))
assert len(rows)==78,f"expected 78 manifest rows, found {len(rows)}"
files=sorted(args.directory.glob("*.bmp"));assert len(files)==78,f"expected 78 BMPs, found {len(files)}"
by_name={p.name:p for p in files};assert len(by_name)==78
matrix=[r for r in rows if not r["file"].startswith("audit-")]
audit=[r for r in rows if r["file"].startswith("audit-")]
assert len(matrix)==60 and len(audit)==18,(len(matrix),len(audit))
assert {r["state"] for r in matrix}==STATES
assert {(int(r["width"]),int(r["height"])) for r in matrix}==SIZES
for size in SIZES:
    assert sum((int(r["width"]),int(r["height"]))==size for r in matrix)==12
assert {r["language"] for r in rows}==LANGS
assert {r["theme"] for r in rows}==THEMES
for state in ("help-bottom","settings","about"):
    subset=[r for r in audit if r["state"]==state]
    assert len(subset)==6,state
    assert {(r["language"],r["theme"]) for r in subset}=={(l,t) for l in LANGS for t in THEMES}
# Short translations can fit without scrolling. Still require the matrix to
# exercise genuine overflow in each theme, so a broken zero-only manifest fails.
for theme in THEMES:
    assert any(r["state"]=="help-bottom" and r["theme"]==theme and
               int(r["scroll_max"])>0 for r in audit),(theme,"missing Help overflow coverage")
for r in rows:
    assert r["file"] in by_name,r["file"]
    w,h=int(r["width"]),int(r["height"])
    fx,fy,fw,fh=(int(r[k]) for k in ("fx","fy","fw","fh"))
    assert fw>0 and fh>0,(r["file"],"missing focus rectangle")
    assert 0<=fx<w and 0<=fy<h and fx+fw<=w and fy+fh<=h,(r["file"],fx,fy,fw,fh)
    assert r["focus"]!="none",r["file"]
    scroll,scroll_max=int(r["scroll"]),int(r["scroll_max"])
    assert 0<=scroll<=scroll_max,(r["file"],scroll,scroll_max)
    if r["state"]=="help-bottom":
        assert scroll==scroll_max,(r["file"],scroll,scroll_max)
    data=by_name[r["file"]].read_bytes();assert data[:2]==b"BM" and len(data)>=54,r["file"]
    offset=struct.unpack_from("<I",data,10)[0];bw,bh=struct.unpack_from("<ii",data,18)
    assert (bw,abs(bh))==(w,h),(r["file"],bw,bh,w,h)
    pixels=data[offset:];assert len(pixels)>w*h,r["file"]
    sample=pixels[::max(1,len(pixels)//12000)]
    assert len(set(sample))>=8,f"blank-looking frame: {r['file']}"
    if r["state"]=="save-error":
        # The retry button used to render while an overlong warning silently
        # disappeared. Inspect a button-free slice immediately to its left.
        crop_left=max(0,fx-150)
        crop_right=fx-4
        assert crop_right>crop_left,(r["file"],"missing save-warning area")
        colors=crop_colors(by_name[r["file"]],crop_left,fy,
                           crop_right-crop_left,fh)
        assert len(colors)>=4,(r["file"],"save warning has no visible text")
print("validated 78 diagnostic BMPs, semantic focus/scroll bounds, required states, viewports, EN/ES/CA and both themes")
