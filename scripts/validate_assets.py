#!/usr/bin/env python3
"""Validate the two canonical branding masters and generated resources."""
from pathlib import Path
import hashlib
import re
import struct

root = Path(__file__).resolve().parents[1]
source = root / "assets" / "branding" / "source"
flags = root / "assets" / "flags"
generated = root / "assets" / "generated"


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", path
    assert data[12:16] == b"IHDR", path
    return struct.unpack(">II", data[16:24])


def git_blob_sha(path: Path) -> str:
    data = path.read_bytes().replace(b"\r\n", b"\n")
    assert b"\r" not in data, path
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()


def rgba_resource(path: Path, symbol: str, expected: tuple[int, int]) -> list[int]:
    text = path.read_text(encoding="utf-8")
    match = re.search(
        rf"{re.escape(symbol)}_width=(\d+),{re.escape(symbol)}_height=(\d+)", text
    )
    assert match, path
    dimensions = (int(match.group(1)), int(match.group(2)))
    assert dimensions == expected, (path, dimensions, expected)
    values = [int(value) for value in re.findall(r"(?<![A-Za-z_])([0-9]+),", text.split("[]={", 1)[1])]
    assert len(values) == expected[0] * expected[1] * 4, path
    assert all(0 <= value <= 255 for value in values), path
    return values


canonical_sources = {
    "sudokura-head.png": (516, 166),
    "sudokura-512.png": (512, 512),
}
source_files = {path.name for path in source.iterdir() if path.is_file()}
assert source_files == set(canonical_sources), (
    "branding source directory must contain exactly the two canonical masters",
    source_files,
)
for name, expected in canonical_sources.items():
    assert png_size(source / name) == expected, name

upstream_blobs = {
    flags / "source" / "us.svg": "9cfd0c927f975b7afff364179c2b850e525236c3",
    flags / "source" / "ar.svg": "c753da103f3ceae0a975b6eda70c2776f7467c5f",
    flags / "source" / "es-ct.svg": "4d85911402e448090b373c2d359b71de7c257f1b",
    flags / "LICENSE-MIT.txt": "ee959dc1d045dc91fa3daed6ac73fef53700cf5f",
}
for path, expected in upstream_blobs.items():
    assert git_blob_sha(path) == expected, path
for name in ("us.png", "ar.png", "es-ct.png"):
    assert png_size(flags / "raster" / name) == (96, 72), name

for size in (16, 32, 48, 64, 128, 256, 512, 1024):
    assert png_size(generated / f"sudokura-{size}.png") == (size, size)
assert (generated / "sudokura-512.png").read_bytes() == (
    source / "sudokura-512.png"
).read_bytes()

ico = (generated / "sudokura.ico").read_bytes()
assert ico[:4] == b"\0\0\1\0" and struct.unpack("<H", ico[4:6])[0] == 6
assert ico.count(b"\x89PNG\r\n\x1a\n") == 6
icns = (generated / "sudokura.icns").read_bytes()
assert icns[:4] == b"icns" and struct.unpack(">I", icns[4:8])[0] == len(icns)

icon_values = rgba_resource(generated / "window_icon.c", "sudokura_icon", (128, 128))
icon_alpha = icon_values[3::4]
assert min(icon_alpha) < 255 and max(icon_alpha) == 255
head_values = rgba_resource(generated / "wordmark.c", "sudokura_wordmark", (516, 166))
head_alpha = head_values[3::4]
assert min(head_alpha) == 0 and max(head_alpha) == 255
for path, symbol in (
    (generated / "flag_us.c", "sudokura_flag_us"),
    (generated / "flag_ar.c", "sudokura_flag_ar"),
    (generated / "flag_ca.c", "sudokura_flag_ca"),
):
    values = rgba_resource(path, symbol, (96, 72))
    assert min(values[3::4]) == 255

print(
    "validated exactly two branding masters, eight icon sizes derived only from "
    "sudokura-512.png, PNG-backed ICO/ICNS, transparent embedded icon/head, "
    "and the three licensed offline language flags"
)
