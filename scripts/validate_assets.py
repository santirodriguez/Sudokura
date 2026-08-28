#!/usr/bin/env python3
"""Validate source and generated branding resources with the Python standard library."""
from pathlib import Path
import base64
import hashlib
import re
import struct

root = Path(__file__).resolve().parents[1]
source = root / "assets" / "branding" / "source"
packed_source = root / "assets" / "branding" / "source-packed"
flags = root / "assets" / "flags"
generated = root / "assets" / "generated"


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", path
    assert data[12:16] == b"IHDR", path
    return struct.unpack(">II", data[16:24])


def git_blob_sha(path: Path) -> str:
    data = path.read_bytes()
    # Git may materialize tracked text with CRLF on Windows. Normalize only
    # line endings before comparing against the canonical upstream Git blob.
    data = data.replace(b"\r\n", b"\n")
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


direct_source_sizes = {
    "sudokura-head.png": (516, 166),
    "sudokura-icon.png": (512, 512),
    "android-chrome-512x512.png": (512, 512),
}
direct_source_sha256 = {
    "android-chrome-512x512.png": "921633c7e39bf09cf72c1b29499f5f5a889a44f90f059eba8f151cb91136d0ad",
    "sudokura-head.png": "e4a12a3465cfa01ea6821e1ceb52c003e87c5db287a571cbc5e67715475a44db",
    "sudokura-icon.png": "6a7ac18b749a60b8d76a04023d0c97a74e3208cefac985a5ad7c8cc122f72f19",
}
assert {path.name for path in source.iterdir() if path.is_file()} == set(direct_source_sha256)
for name, expected in direct_source_sizes.items():
    assert png_size(source / name) == expected, name
for name, expected in direct_source_sha256.items():
    assert hashlib.sha256((source / name).read_bytes()).hexdigest() == expected, name

packed_assets = {
    "apple-touch-icon": (
        "apple-touch-icon.png",
        (180, 180),
        19584,
        "2ab16a34551772a520cf3e462a1bd9e8db2ad2a8f78363061aa969c5621914e6",
    ),
    "android-chrome-192x192": (
        "android-chrome-192x192.png",
        (192, 192),
        21142,
        "0991568b17ea2a007ccccb4ad4ef8c424d817cda6c07bd5802e69d022fd53729",
    ),
}
for directory, (name, dimensions, byte_length, expected_sha256) in packed_assets.items():
    parts = sorted((packed_source / directory).glob("part-*.b64"))
    assert parts, directory
    encoded = "".join(path.read_text(encoding="ascii") for path in parts)
    data = base64.b64decode(encoded, validate=True)
    assert len(data) == byte_length, name
    assert hashlib.sha256(data).hexdigest() == expected_sha256, name
    assert data[:8] == b"\x89PNG\r\n\x1a\n" and data[12:16] == b"IHDR", name
    assert struct.unpack(">II", data[16:24]) == dimensions, name

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
    source / "sudokura-icon.png"
).read_bytes()
ico = (generated / "sudokura.ico").read_bytes()
assert ico[:4] == b"\0\0\1\0" and struct.unpack("<H", ico[4:6])[0] == 6
assert ico.count(b"\x89PNG\r\n\x1a\n") == 6
icns = (generated / "sudokura.icns").read_bytes()
assert icns[:4] == b"icns" and struct.unpack(">I", icns[4:8])[0] == len(icns)

icon_values = rgba_resource(generated / "window_icon.c", "sudokura_icon", (128, 128))
icon_alpha = icon_values[3::4]
assert min(icon_alpha) == 0 and max(icon_alpha) == 255
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
    "validated three direct and two packed source assets, exact MIT flag sources, 96x72 flag rasters, "
    "8 PNG icon sizes from the transparent sudokura-icon.png master, 6-size PNG-backed ICO, ICNS, "
    "transparent 128x128 window icon, 516x166 embedded head and three embedded flags"
)
