#!/usr/bin/env python3
"""Fail-closed structural audit of an Android debug APK (not device acceptance)."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import zipfile

if len(sys.argv) != 3:
    raise SystemExit("usage: audit_apk.py <aapt-path> <debug-apk>")
aapt, apk = sys.argv[1], Path(sys.argv[2])
if not apk.is_file():
    raise SystemExit(f"Missing APK: {apk}")


def aapt_dump(mode):
    process = subprocess.run(
        [aapt, "dump", mode, str(apk)], check=True,
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return process.stdout


badging = aapt_dump("badging")
manifest = subprocess.run(
    [aapt, "dump", "xmltree", str(apk), "AndroidManifest.xml"],
    check=True, text=True, stdout=subprocess.PIPE).stdout
for expected in (
    "name='org.sudokura.prototype.debug'",
    "versionCode='10309'",
    "versionName='1.3.9-prototype'",
    "sdkVersion:'28'",
    "targetSdkVersion:'36'",
    "native-code: 'arm64-v8a'",
):
    if expected not in badging:
        raise SystemExit(f"APK mismatch: {expected}")
if re.search(r"uses-permission", badging, flags=re.IGNORECASE):
    raise SystemExit("Unexpected prototype manifest permission")
if not re.search(
    r"android:icon\(", manifest
):
    raise SystemExit("Android launcher icon is missing from merged manifest")
if not re.search(r"application:.*icon='[^']+'", badging):
    raise SystemExit("Android launcher icon is not resolved by aapt")
if not re.search(
    r"android:allowBackup\(0x01010280\)=\(type 0x12\)0x0\b", manifest
):
    raise SystemExit("Manifest does not explicitly disable backup")
for attribute in ("android:fullBackupContent(", "android:dataExtractionRules("):
    if attribute not in manifest:
        raise SystemExit(f"Manifest missing prototype backup exclusion reference: {attribute}")
expected_native = {
    "libSDL2.so",
    "libSDL2_ttf.so",
    "libSDL2_mixer.so",
    "libmain.so",
}
with zipfile.ZipFile(apk) as archive:
    paths = set(archive.namelist())
    native_paths = {p for p in paths if p.startswith("lib/") and p.endswith(".so")}
    if any(not p.startswith("lib/arm64-v8a/") for p in native_paths):
        raise SystemExit("Unexpected native ABI")
    actual_native = {Path(p).name for p in native_paths}
    if not expected_native.issubset(actual_native):
        raise SystemExit(f"Missing native libraries: {sorted(expected_native - actual_native)}")
    expected_assets = {
        "res/xml/backup_rules.xml",
        "res/xml/data_extraction_rules.xml",
        "assets/DejaVuSans.ttf",
        "assets/audio/music-main.ogg",
        "assets/audio/music-fail.ogg",
        "assets/audio/jingle-win.ogg",
        "assets/audio/jingle-fail.ogg",
        "assets/licenses/Sudokura-GPLv3.txt",
        "assets/licenses/DejaVuSans-copyright.txt",
        "assets/licenses/SDL2-2.32.10-license.txt",
        "assets/licenses/SDL2_ttf-2.24.0-license.txt",
        "assets/licenses/SDL2_mixer-2.8.1-license.txt",
        "assets/licenses/FreeType-license-choice.txt",
        "assets/licenses/FreeType-FTL.txt",
    }
    if not expected_assets.issubset(paths):
        raise SystemExit(f"Missing assets/notices: {sorted(expected_assets - paths)}")
    for path in expected_assets:
        if archive.getinfo(path).file_size == 0:
            raise SystemExit(f"Empty asset: {path}")
    icon_files = [p for p in paths if p.endswith("/sudokura_icon_source.png")]
    if len(icon_files) != 1:
        raise SystemExit(f"Expected one generated launcher icon: {icon_files}")
    icon_png = archive.read(icon_files[0])
    if not (icon_png.startswith(b"\x89PNG\r\n\x1a\n") and len(icon_png) >= 24):
        raise SystemExit("Launcher icon is not a PNG")
    import struct
    if struct.unpack(">II", icon_png[16:24]) != (512, 512):
        raise SystemExit("Launcher icon does not match 512x512 master dimensions")
    source_audio = Path(__file__).resolve().parents[2] / "assets" / "audio"
    for name in ("music-main.ogg", "music-fail.ogg",
                 "jingle-win.ogg", "jingle-fail.ogg"):
        packed = archive.read(f"assets/audio/{name}")
        local = (source_audio / name).read_bytes()
        if hashlib.sha256(packed).digest() != hashlib.sha256(local).digest():
            raise SystemExit(f"Altered Android OGG resource: {name}")
print(json.dumps({
    "apk": str(apk),
    "bytes": apk.stat().st_size,
    "native": sorted(native_paths),
    "permissions": [],
    "application_id": "org.sudokura.prototype.debug",
    "android_backup": False,
    "asset_count": len(expected_assets),
    "device_test": "NOT_RUN",
}, indent=2))
