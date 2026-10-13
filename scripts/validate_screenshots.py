#!/usr/bin/env python3
"""Validate the Phase 5 diagnostic UI review matrix and semantic manifest."""
from pathlib import Path
import argparse,csv,hashlib,struct

SIZES={(640,480),(1024,768),(1366,768),(360,640),(1920,1080)}
HELP_AUDIT_SIZES={(360,640),(640,480),(1024,768)}
STATES={"home-empty","home-save","notes","conflict","hint","save-error",
        "pause","loss","win","help","settings","about",
        "games-empty","games-active","games-unavailable"}
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
assert len(rows)==135,f"expected 135 manifest rows, found {len(rows)}"
files=sorted(args.directory.glob("*.bmp"))
assert len(files)==135,f"expected 135 BMPs, found {len(files)}"
by_name={p.name:p for p in files}
assert len(by_name)==135
assert len({r["file"] for r in rows})==len(rows),"duplicate manifest filename"
assert {r["file"] for r in rows}==set(by_name),"manifest/BMP mismatch"
matrix=[r for r in rows if not r["file"].startswith("audit-")]
audit=[r for r in rows if r["file"].startswith("audit-")]
assert len(matrix)==75 and len(audit)==60,(len(matrix),len(audit))
matrix_keys=[(int(r["width"]),int(r["height"]),r["state"]) for r in matrix]
assert len(set(matrix_keys))==len(matrix_keys),"duplicate matrix scene"
assert set(matrix_keys)=={(w,h,state) for w,h in SIZES for state in STATES}, "matrix coverage mismatch"
for size in SIZES:
    games=[r for r in matrix if
           (int(r["width"]),int(r["height"]))==size and
           r["state"].startswith("games-")]
    assert {r["state"] for r in games}=={
        "games-empty","games-active","games-unavailable"
    },(size,"missing Games state")
    fingerprints={hashlib.sha256(by_name[r["file"]].read_bytes()).digest()
                  for r in games}
    assert len(fingerprints)==3,(size,"Games scenes rendered identically")
assert {r["language"] for r in rows}==LANGS
assert {r["theme"] for r in rows}==THEMES
for state in ("help-top","help-bottom"):
    subset=[r for r in audit if r["state"]==state]
    assert len(subset)==18,state
    for size in HELP_AUDIT_SIZES:
        sized=[r for r in subset
               if (int(r["width"]),int(r["height"]))==size]
        assert len(sized)==6,(state,size,len(sized))
        assert {(r["language"],r["theme"]) for r in sized}=={
            (language,theme) for language in LANGS for theme in THEMES
        }
for state in ("settings","audio-popup","dialog-prep","about"):
    subset=[r for r in audit if r["state"]==state]
    assert len(subset)==6,state
    assert {(int(r["width"]),int(r["height"])) for r in subset}=={(1024,768)}
    assert {(r["language"],r["theme"]) for r in subset}=={
        (language,theme) for language in LANGS for theme in THEMES
    }
for r in audit:
    size=(int(r["width"]),int(r["height"]))
    if r["state"]=="help-top":
        assert int(r["scroll"])==0,(r["file"],"Help top must start at zero")
    if r["state"]=="help-bottom":
        assert int(r["scroll"])==int(r["scroll_max"]),(r["file"],"Help bottom not reached")
        if size in {(360,640),(640,480)}:
            assert int(r["scroll_max"])>0,(r["file"],"compact Help must exercise overflow")
for r in rows:
    assert r["file"] in by_name,r["file"]
    audio_visible=int(r["audio_visible"])
    audio_popup=int(r["audio_popup"])
    audio_pressed=int(r["audio_pressed"])
    assert audio_visible in (0,1) and audio_popup in (0,1) and audio_pressed in (0,1),r["file"]
    if r["state"] in {"games-empty","games-active","games-unavailable"}:
        expected_focus={
            "games-empty":"games-back",
            "games-active":"games-active",
            "games-unavailable":"games-unavailable",
        }[r["state"]]
        assert r["focus"]==expected_focus,(r["file"],"Games focus mismatch")
        assert audio_popup==0 and audio_pressed==0,(r["file"],"Games audio overlay")
    if r["state"] in {"hint","pause","settings"}:
        assert audio_visible==0,(r["file"],"covered surface exposes audio control")
        assert audio_popup==0,(r["file"],"covered surface retained audio popup")
        assert audio_pressed==0,(r["file"],"covered surface retained audio press")
    if r["state"]=="audio-popup":
        assert audio_visible==1,(r["file"],"audio popup control unexpectedly hidden")
        assert audio_popup==1,(r["file"],"audio popup did not remain open")
        assert audio_pressed==0,(r["file"],"audio popup retained pressed state")
    if r["state"]=="dialog-prep":
        assert audio_popup==0,(r["file"],"native dialog preparation retained audio popup")
        assert audio_pressed==0,(r["file"],"native dialog preparation retained audio press")
    w,h=int(r["width"]),int(r["height"])
    fx,fy,fw,fh=(int(r[k]) for k in ("fx","fy","fw","fh"))
    assert fw>0 and fh>0,(r["file"],"missing focus rectangle")
    assert 0<=fx<w and 0<=fy<h and fx+fw<=w and fy+fh<=h,(r["file"],fx,fy,fw,fh)
    assert r["focus"]!="none",r["file"]
    if r["state"] in {"games-active","games-unavailable"}:
        # Exclude the focus outline; ensure selected-row text actually rendered.
        # This catches long translations silently disappearing on narrow screens.
        assert fw>28 and fh>20,(r["file"],"invalid Games row bounds")
        colors=crop_colors(by_name[r["file"]],fx+14,fy+10,fw-28,fh-20)
        assert len(colors)>=4,(r["file"],"Games row label missing")
        # The selected game's detail title must be readable, not just its
        # fallback summary. Derive the same panel geometry as GamesBrowser.
        margin=12 if w<640 else 24
        panel_width=min(w-margin*2,850)
        panel_x=(w-panel_width)//2
        list_y=(55 if h<520 else 76)+49
        detail_height=76 if h<550 else 104
        list_height=max(48,(h-margin)-40-12-detail_height-12-list_y)
        detail_y=list_y+list_height+8
        assert detail_y+25<h,(r["file"],"invalid Games detail bounds")
        heading=crop_colors(by_name[r["file"]],panel_x+14,detail_y+5,
                            panel_width-28,20)
        assert len(heading)>=4,(r["file"],"Games detail heading missing")
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
print("validated 135 diagnostic BMPs, including 15 distinct Games empty/active/unavailable scenes, covered-audio-control and native-dialog repaint policy, H41/H42 compact Help top/bottom, semantic states, viewports, EN/ES/CA and both themes")
