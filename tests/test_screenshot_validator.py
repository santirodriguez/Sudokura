#!/usr/bin/env python3
"""Exercise the screenshot validator against a real rendered matrix and mutations."""
import argparse
import csv
from pathlib import Path
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("directory", type=Path)
args = parser.parse_args()
source = args.directory.resolve()
repo_root = Path(__file__).resolve().parents[1]
validator = repo_root / "scripts/validate_screenshots.py"
for path in (repo_root / "src" / "sudokura_sdl").glob("*.inc"):
    if path.name == "04_screens.inc":
        continue
    text = path.read_text(encoding="utf-8")
    assert "SDL_ShowSimpleMessageBox(" not in text, path
    assert "SDL_ShowMessageBox(" not in text, path
print("PASS native dialog wrapper routing")

with (source / "MANIFEST.tsv").open(encoding="utf-8", newline="") as stream:
    reader = csv.DictReader(stream, delimiter="\t")
    fields = reader.fieldnames
    original = list(reader)

with tempfile.TemporaryDirectory(prefix="sudokura-validator-") as temporary:
    root = Path(temporary)
    for frame in source.glob("*.bmp"):
        (root / frame.name).symlink_to(frame)

    def check(rows, expected, name):
        with (root / "MANIFEST.tsv").open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=fields, delimiter="\t")
            writer.writeheader()
            writer.writerows(rows)
        result = subprocess.run([sys.executable, str(validator), str(root)],
                                capture_output=True, text=True)
        assert (result.returncode == 0) == expected, (name, result.stdout, result.stderr)
        print(f"PASS {name}")

    check(original, True, "real matrix")

    # Every frame must validate against its own focus rectangle, regardless
    # of manifest ordering (including More and Saved Games paging).
    check(list(reversed(original)), True, "manifest row order independence")

    rows = [dict(row) for row in original]
    more = next(row for row in rows if row["state"] == "more")
    more["fx"],more["fy"],more["fw"],more["fh"]="0","0","80","36"
    check(rows, False, "reject More focus outside the rendered panel")

    rows = [dict(row) for row in original]
    pager = next(row for row in rows if row["state"] == "games-paging")
    pager["fx"],pager["fy"],pager["fw"],pager["fh"]="0","0","80","36"
    check(rows, False, "reject paging focus outside the rendered controls")

    rows = [dict(row) for row in original if row["state"] != "more"]
    check(rows, False, "reject missing compact More audit")

    rows = [dict(row) for row in original if row["state"] != "games-paging"]
    check(rows, False, "reject missing visible library paging audit")

    rows = [dict(row) for row in original]
    more = next(row for row in rows if row["state"]=="more")
    more["focus"] = "games-next-page"
    check(rows, False, "reject incorrect More keyboard focus")

    rows = [dict(row) for row in original]
    pager = next(row for row in rows if row["state"]=="games-paging")
    pager["focus"] = "games-back"
    check(rows, False, "reject missing page button focus")


    rows = [dict(row) for row in original]
    compact = next(row for row in rows
                   if row["state"] == "save-error"
                   and (int(row["width"]), int(row["height"])) == (640, 480))
    compact["fh"] = "19"
    check(rows, False, "reject undersized compact in-play save retry")

    rows = [dict(row) for row in original]
    pending = next(row for row in rows if row["state"] == "result-save-error")
    pending["fh"] = "31"
    check(rows, False, "reject undersized result save retry")

    rows = [dict(row) for row in original]
    pending = next(row for row in rows if row["state"] == "result-save-error")
    pending["focus"] = "home"
    check(rows, False, "reject pending result save focus mismatch")

    rows = [dict(row) for row in original]
    pending = next(row for row in rows if row["state"] == "result-save-error")
    pending["state"] = "win"
    check(rows, False, "reject missing pending result save scene")

    rows = [dict(row) for row in original]
    recovering = next(row for row in rows if row["state"] == "games-recover")
    recovering["focus"] = "games-unavailable"
    check(rows, False, "reject Recovery action focus mismatch")

    rows = [dict(row) for row in original]
    recovering = next(row for row in rows if row["state"] == "games-recover")
    recovering["state"] = "games-unavailable"
    check(rows, False, "reject missing Recovery scene")

    rows = [dict(row) for row in original]
    finished = next(row for row in rows if row["state"] == "games-detail")
    finished["focus"] = "games-back"
    check(rows, False, "reject finished Replay focus mismatch")

    rows = [dict(row) for row in original]
    detail = next(row for row in rows if row["state"] == "games-detail")
    detail["state"] = "games-finished"
    check(rows, False, "reject missing finished detail scene")

    rows = [dict(row) for row in original]
    daily = next(row for row in rows if row["state"] == "home-daily-loading")
    daily["focus"] = "new-game"
    check(rows, False, "reject Daily loading focus mismatch")

    rows = [dict(row) for row in original]
    daily = next(row for row in rows if row["state"] == "home-daily-older")
    daily["state"] = "home-daily-finished"
    check(rows, False, "reject missing older Daily scene")

    rows = [dict(row) for row in original]
    row = next(row for row in rows
               if row["state"] == "games-unavailable" and
               (int(row["width"]), int(row["height"])) == (360, 640))
    row["focus"] = "games-active"
    check(rows, False, "reject unavailable Games focus mismatch")

    rows = [dict(row) for row in original]
    row = next(row for row in rows
               if row["state"] == "games-active" and
               (int(row["width"]), int(row["height"])) == (640, 480))
    row["state"] = "games-empty"
    check(rows, False, "reject missing Games scene and duplicate state")

    games = [row for row in original if
             (int(row["width"]), int(row["height"])) == (360, 640)]
    active = next(row for row in games if row["state"] == "games-active")
    empty = next(row for row in games if row["state"] == "games-empty")
    active_link = root / active["file"]
    active_link.unlink()
    active_link.symlink_to(source / empty["file"])
    check(original, False, "reject identical Games screen render")
    active_link.unlink()
    active_link.symlink_to(source / active["file"])

    # A focused Games row may have a valid outline but no visible text.
    rows = [dict(row) for row in original]
    row = next(row for row in rows
               if row["state"] == "games-unavailable" and
               (int(row["width"]), int(row["height"])) == (360, 640))
    frame = root / row["file"]
    frame.unlink()
    data = bytearray((source / row["file"]).read_bytes())
    offset = int.from_bytes(data[10:14], "little")
    height = int.from_bytes(data[22:26], "little", signed=True)
    bpp = int.from_bytes(data[28:30], "little")
    assert bpp in (24, 32)
    pixel_size = bpp // 8
    width = int(row["width"])
    stride = ((width * pixel_size + 3) // 4) * 4
    fx, fy, fw, fh = (int(row[key]) for key in ("fx", "fy", "fw", "fh"))
    left, right, top, bottom = fx + 14, fx + fw - 14, fy + 10, fy + fh - 10
    def pixel_offset(x, y):
        stored_y = abs(height) - 1 - y if height > 0 else y
        return offset + stored_y * stride + x * pixel_size
    sample = pixel_offset(left, top)
    fill = bytes(data[sample:sample + pixel_size])
    for py in range(top, bottom):
        for px in range(left, right):
            position = pixel_offset(px, py)
            data[position:position + pixel_size] = fill
    frame.write_bytes(data)
    check(rows, False, "reject missing Games selected-row text")
    frame.unlink()
    frame.symlink_to(source / row["file"])

    rows = [dict(row) for row in original]
    for row in rows:
        if (row["state"] == "help-bottom" and
                (int(row["width"]), int(row["height"])) == (1024, 768) and
                row["language"] == "English"):
            row["scroll"] = row["scroll_max"] = "0"
    check(rows, True, "desktop fitting translation may need no scroll")
    rows = [dict(row) for row in original]
    row = next(row for row in rows if row["state"] == "help-bottom" and int(row["scroll_max"]) > 0)
    row["scroll"] = str(int(row["scroll_max"]) - 1)
    check(rows, False, "reject bottom not reached")
    rows = [dict(row) for row in original]
    for row in rows:
        if (row["state"] == "help-bottom" and
                (int(row["width"]), int(row["height"])) == (360, 640)):
            row["scroll"] = row["scroll_max"] = "0"
    check(rows, False, "reject missing compact Help overflow")

    rows = [dict(row) for row in original]
    rows = [row for row in rows
            if not (row["state"] == "audio-popup" and row["language"] == "Català")]
    check(rows, False, "reject missing semantic-state audit")
    rows = [dict(row) for row in original]
    rows[0]["fx"] = "-1"
    check(rows, False, "reject out-of-bounds focus")

    rows = [dict(row) for row in original]
    row = next(row for row in rows if row["state"] == "hint")
    row["audio_visible"] = "1"
    check(rows, False, "reject visible audio control over Hint")
    rows = [dict(row) for row in original]
    row = next(row for row in rows if row["state"] == "pause")
    row["audio_popup"] = "1"
    check(rows, False, "reject stale audio popup over Pause")
    rows = [dict(row) for row in original]
    row = next(row for row in rows if row["state"] == "settings")
    row["audio_pressed"] = "1"
    check(rows, False, "reject stale audio press over Settings")
    rows = [dict(row) for row in original]
    row = next(row for row in rows if row["state"] == "dialog-prep")
    row["audio_popup"] = "1"
    check(rows, False, "reject stale audio popup behind native dialog")

    rows = [dict(row) for row in original]
    row = next(row for row in rows if row["state"] == "save-error")
    frame = root / row["file"]
    frame.unlink()
    data = bytearray((source / row["file"]).read_bytes())
    offset = int.from_bytes(data[10:14], "little")
    width = int.from_bytes(data[18:22], "little", signed=True)
    height = int.from_bytes(data[22:26], "little", signed=True)
    bpp = int.from_bytes(data[28:30], "little")
    assert bpp in (24, 32)
    pixel_size = bpp // 8
    stride = ((abs(width) * pixel_size + 3) // 4) * 4
    fx, fy, fh = (int(row[key]) for key in ("fx", "fy", "fh"))
    left, right = max(0, fx - 150), fx - 4
    sample_y = fy + fh // 2
    stored_sample_y = abs(height) - 1 - sample_y if height > 0 else sample_y
    sample = offset + stored_sample_y * stride + left * pixel_size
    fill = bytes(data[sample:sample + pixel_size])
    for logical_y in range(fy, fy + fh):
        stored_y = abs(height) - 1 - logical_y if height > 0 else logical_y
        for logical_x in range(left, right):
            pixel = offset + stored_y * stride + logical_x * pixel_size
            data[pixel:pixel + pixel_size] = fill
    frame.write_bytes(data)
    check(rows, False, "reject invisible save warning")
    frame.unlink()
    frame.symlink_to(source / row["file"])

    # The result banner's message starts near the left of its wider card.
    # Remove its actual message region; an empty right-side slice alone is
    # not evidence that the warning disappeared.
    rows = [dict(row) for row in original]
    row = next(row for row in rows if row["state"] == "result-save-error")
    frame = root / row["file"]
    frame.unlink()
    data = bytearray((source / row["file"]).read_bytes())
    offset = int.from_bytes(data[10:14], "little")
    width = int(row["width"])
    height = int.from_bytes(data[22:26], "little", signed=True)
    bpp = int.from_bytes(data[28:30], "little")
    assert bpp in (24, 32)
    pixel_size = bpp // 8
    stride = ((width * pixel_size + 3) // 4) * 4
    fx, fy, fh = (int(row[key]) for key in ("fx", "fy", "fh"))
    margin = 8 if width < 640 else 14
    banner_width = min(width - 2 * margin, 760)
    left, right = (width - banner_width) // 2 + 14, fx - 12
    top, bottom = fy + 3, fy + fh - 3
    stored_sample_y = abs(height) - 1 - top if height > 0 else top
    sample = offset + stored_sample_y * stride + left * pixel_size
    fill = bytes(data[sample:sample + pixel_size])
    for logical_y in range(top, bottom):
        stored_y = abs(height) - 1 - logical_y if height > 0 else logical_y
        for logical_x in range(left, right):
            pixel = offset + stored_y * stride + logical_x * pixel_size
            data[pixel:pixel + pixel_size] = fill
    frame.write_bytes(data)
    check(rows, False, "reject invisible result save warning")
