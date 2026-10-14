#!/usr/bin/env python3
"""Validate the Phase 5 diagnostic UI review matrix and semantic manifest."""
from pathlib import Path
import argparse,csv,hashlib,struct

SIZES={(640,480),(1024,768),(1366,768),(360,640),(1920,1080)}
HELP_AUDIT_SIZES={(360,640),(640,480),(1024,768)}
STATES={"home-empty","home-save","notes","conflict","hint","save-error",
        "result-save-error","pause","loss","win","help","settings","about",
        "games-empty","games-active","games-unavailable",
        "games-finished","games-detail","games-recover",
        "home-daily-loading","home-daily-older","home-daily-finished"}
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
assert len(rows)==170,f"expected 170 manifest rows, found {len(rows)}"
files=sorted(args.directory.glob("*.bmp"))
assert len(files)==170,f"expected 170 BMPs, found {len(files)}"
by_name={p.name:p for p in files}
assert len(by_name)==170
assert len({r["file"] for r in rows})==len(rows),"duplicate manifest filename"
assert {r["file"] for r in rows}==set(by_name),"manifest/BMP mismatch"
matrix=[r for r in rows if not r["file"].startswith("audit-")]
audit=[r for r in rows if r["file"].startswith("audit-")]
assert len(matrix)==110 and len(audit)==60,(len(matrix),len(audit))
matrix_keys=[(int(r["width"]),int(r["height"]),r["state"]) for r in matrix]
assert len(set(matrix_keys))==len(matrix_keys),"duplicate matrix scene"
assert set(matrix_keys)=={(w,h,state) for w,h in SIZES for state in STATES}, "matrix coverage mismatch"
for size in SIZES:
    games=[r for r in matrix if
           (int(r["width"]),int(r["height"]))==size and
           r["state"].startswith("games-")]
    assert {r["state"] for r in games}=={
        "games-empty","games-active","games-unavailable",
        "games-finished","games-detail","games-recover"
    },(size,"missing Games state")
    fingerprints={hashlib.sha256(by_name[r["file"]].read_bytes()).digest()
                  for r in games}
    assert len(fingerprints)==6,(size,"Games scenes rendered identically")
    daily=[r for r in matrix if
           (int(r["width"]),int(r["height"]))==size and
           r["state"] in {"home-daily-loading","home-daily-older",
                          "home-daily-finished"}]
    assert len(daily)==3,(size,"missing Daily state")
    assert len({hashlib.sha256(by_name[r["file"]].read_bytes()).digest()
                for r in daily})==3,(size,"Daily states rendered identically")
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
    if r["state"] in {"games-empty","games-active","games-unavailable",
        "games-finished","games-detail","games-recover"}:
        expected_focus={
            "games-empty":"games-back",
            "games-active":"games-active",
            "games-unavailable":"games-unavailable",
            "games-finished":"games-finished",
            "games-detail":"games-replay",
            "games-recover":"games-recover",
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
    if r["state"] in {"games-active","games-unavailable","games-finished"}:
        # Exclude the focus outline; ensure selected-row text actually rendered.
        # This catches long translations silently disappearing on narrow screens.
        assert fw>28 and fh>20,(r["file"],"invalid Games row bounds")
        colors=crop_colors(by_name[r["file"]],fx+14,fy+10,fw-28,fh-20)
        assert len(colors)>=4,(r["file"],"Games row label missing")
        # Derive the responsive library geometry rather than accepting a
        # screenshot just because the selected list row has a visible outline.
        margin=12 if w<640 else 24
        panel_width=min(w-2*margin,1480 if w>=1600 else 1240)
        panel_x=(w-panel_width)//2
        header_y=51 if h<520 else 69
        content_y=header_y+83
        compact=panel_width<760
        button_space=96 if compact else 44
        buttons_y=h-margin-button_space
        content_height=max(105,buttons_y-content_y-12)
        split=panel_width>=860 and content_height>=320
        if split:
            list_width=(panel_width-14)*44//100
            detail_x=panel_x+list_width+14
            detail_y=content_y
            detail_width=panel_width-list_width-14
            detail_height=content_height
        else:
            detail_height=max(86,min(206,content_height*40//100))
            list_height=max(48,content_height-detail_height-10)
            detail_x=panel_x
            detail_y=content_y+list_height+10
            detail_width=panel_width
        assert detail_y+detail_height<=h,(r["file"],"invalid Games detail bounds")
        heading=crop_colors(by_name[r["file"]],detail_x+12,detail_y+8,
                            detail_width-24,27)
        assert len(heading)>=4,(r["file"],"Games preview heading missing")
        if split and r["state"]=="games-active":
            # The two-panel layout must include a legible saved board,
            # not merely a list and a blank side panel.
            side=min(detail_width-48,detail_height-167)
            side=min(side,600 if w>=2500 else 520 if w>=1500 else 380)
            assert side>=81,(r["file"],"invalid saved-board preview")
            board_x=detail_x+(detail_width-side)//2
            board_y=detail_y+44
            image=crop_colors(by_name[r["file"]],
                              board_x+side//3,board_y+side//3,
                              side//3,side//3)
            assert len(image)>=4,(r["file"],"Saved preview missing")
    if r["state"] in {"home-daily-loading","home-daily-older",
                      "home-daily-finished"}:
        assert r["focus"]=="daily",(r["file"],"Daily control focus mismatch")
        assert fw>=55 and fh>=35,(r["file"],"Daily button too small")
        foreground=crop_colors(by_name[r["file"]],fx+8,fy+8,fw-16,fh-16)
        assert len(foreground)>=4,(r["file"],"Daily text not visible")
    if r["state"]=="games-recover":
        assert fw>=55 and fh>=35,(r["file"],"Recovery button too small")
        focused=crop_colors(by_name[r["file"]],fx+8,fy+8,fw-16,fh-16)
        assert len(focused)>=4,(r["file"],"Recovery action not visible")
    if r["state"]=="games-detail":
        assert fw>=55 and fh>=35,(r["file"],"Replay button too small")
        focused=crop_colors(by_name[r["file"]],fx+8,fy+8,fw-16,fh-16)
        assert len(focused)>=4,(r["file"],"Replay text not visible")
        # The destructive control is not the default focus, but its caption
        # must remain readable in both desktop and compact two-row layout.
        panel_width=min(w-2*(12 if w<640 else 24),
                        1480 if w>=1600 else 1240)
        delete_x=fx+fw+(6 if panel_width<640 else 10)
        assert delete_x+fw<=w,(r["file"],"Delete action clipped")
        delete_colors=crop_colors(by_name[r["file"]],
                                  delete_x+8,fy+8,fw-16,fh-16)
        assert len(delete_colors)>=4,(r["file"],"Delete text not visible")
        margin=12 if w<640 else 24
        panel_width=min(w-2*margin,1480 if w>=1600 else 1240)
        panel_x=(w-panel_width)//2
        content_y=(51 if h<520 else 69)+83
        action_space=96 if panel_width<760 else 44
        buttons_y=h-margin-action_space
        surface_height=buttons_y-content_y-12
        max_side=560 if w>=2200 else 480 if w>=1500 else 420
        side=max(80,min(panel_width-40,surface_height-128,max_side))
        board_x=panel_x+(panel_width-side)//2
        board_y=content_y+12
        board=crop_colors(by_name[r["file"]],
                          board_x+side//3,board_y+side//3,
                          side//3,side//3)
        assert len(board)>=4,(r["file"],"Finished board missing")
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
    if r["state"] in {"save-error","result-save-error"}:
        assert r["focus"]=="retry-save",(r["file"],"Save retry focus mismatch")
        # In-play uses the compact status strip (22px at 640x480 and
        # 24px at 360x640); result screens use the taller bottom banner.
        min_height = 20 if r["state"]=="save-error" else 32
        assert fw>=55 and fh>=min_height,(r["file"],"Save retry button too small")
        # Check the warning itself, not just the retry button. The in-play
        # strip is narrow; the result screen centers short copy in a much
        # wider message rectangle, leaving its right edge legitimately blank.
        if r["state"]=="result-save-error":
            margin=8 if w<640 else 14
            banner_width=min(w-2*margin,760)
            crop_left=(w-banner_width)//2+14
            crop_right=fx-12
            crop_y,crop_height=fy+3,fh-6
        else:
            crop_left,crop_right=max(0,fx-150),fx-4
            crop_y,crop_height=fy,fh
        assert crop_right>crop_left and crop_height>0,(r["file"],"missing save-warning area")
        colors=crop_colors(by_name[r["file"]],crop_left,crop_y,
                           crop_right-crop_left,crop_height)
        assert len(colors)>=4,(r["file"],"save warning has no visible text")
print("validated 170 diagnostic BMPs, including 30 distinct Games scenes (five recovery states) and 15 Daily loading/older/finished Home states, covered-audio-control and native-dialog repaint policy, H41/H42 compact Help top/bottom, semantic states, viewports, EN/ES/CA and both themes")
