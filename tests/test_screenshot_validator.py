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
validator = Path(__file__).resolve().parents[1] / "scripts/validate_screenshots.py"
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
