#!/usr/bin/env python3
"""Validate the Phase 5 diagnostic UI review matrix and semantic manifest."""
from pathlib import Path
import argparse,csv,struct

SIZES={(640,480),(1024,768),(1366,768),(360,640),(1920,1080)}
STATES={"home-empty","home-save","notes","conflict","hint","save-error",
        "pause","loss","win","help","settings","about"}
LANGS={"English","Español","Català"}
THEMES={"dark","light"}

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
        assert scroll_max>0 and scroll==scroll_max,(r["file"],scroll,scroll_max)
    data=by_name[r["file"]].read_bytes();assert data[:2]==b"BM" and len(data)>=54,r["file"]
    offset=struct.unpack_from("<I",data,10)[0];bw,bh=struct.unpack_from("<ii",data,18)
    assert (bw,abs(bh))==(w,h),(r["file"],bw,bh,w,h)
    pixels=data[offset:];assert len(pixels)>w*h,r["file"]
    sample=pixels[::max(1,len(pixels)//12000)]
    assert len(set(sample))>=8,f"blank-looking frame: {r['file']}"
print("validated 78 diagnostic BMPs, semantic focus/scroll bounds, required states, viewports, EN/ES/CA and both themes")
