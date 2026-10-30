#!/usr/bin/env python3
"""Fail-closed structural audit of an Android debug APK (not device acceptance)."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
EXPECTED_NATIVE = {"libSDL2.so", "libSDL2_ttf.so", "libSDL2_mixer.so", "libmain.so"}
EXPECTED_ASSETS = {
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
AUDIO_NAMES = ("music-main.ogg", "music-fail.ogg", "jingle-win.ogg", "jingle-fail.ogg")


def expected_identity(version_header, variant="debug"):
    """Read the same source components as Gradle; never fall back to a version."""
    if variant != "debug":
        raise ValueError("Only the debug prototype variant is currently supported")
    components = []
    for name in ("MAJOR", "MINOR", "PATCH"):
        values = re.findall(
            rf"^#define[ \t]+SUDOKURA_VERSION_{name}[ \t]+([^\n]*)$",
            version_header, flags=re.MULTILINE)
        if len(values) != 1 or not re.fullmatch(r"[0-9]+", values[0].strip()):
            raise ValueError(f"Invalid or ambiguous version component: {name}")
        components.append(int(values[0].strip()))
    major, minor, patch = components
    if minor > 99 or patch > 99:
        raise ValueError("versionCode requires two-digit minor/patch")
    code = major * 10000 + minor * 100 + patch
    if not 0 < code <= 2_147_483_647:
        raise ValueError("versionCode must fit a positive signed 32-bit integer")
    return {
        "application_id": "org.sudokura.prototype.debug",
        "version_code": code,
        "version_name": f"{major}.{minor}.{patch}-prototype",
        "build_variant": variant,
    }


def audit_manifest(badging, manifest, identity):
    # Match the package record itself, not an activity or incidental substring.
    packages = re.findall(r"^package: ([^\n]+)$", badging, flags=re.MULTILINE)
    if len(packages) != 1:
        raise ValueError("Expected exactly one aapt package record")
    for key, value in (("name", identity["application_id"]),
                       ("versionCode", str(identity["version_code"])),
                       ("versionName", identity["version_name"])):
        values = re.findall(rf"(?:^|\s){key}='([^']*)'", packages[0])
        if values != [value]:
            raise ValueError(f"APK mismatch: {key}={value!r}")
    for expected in ("sdkVersion:'28'", "targetSdkVersion:'36'",
                     "native-code: 'arm64-v8a'"):
        if expected not in {line.strip() for line in badging.splitlines()}:
            raise ValueError(f"APK mismatch: {expected}")
    if re.search(r"uses-permission", badging, flags=re.IGNORECASE):
        raise ValueError("Unexpected prototype manifest permission")
    if not re.search(r"android:icon\(", manifest):
        raise ValueError("Android launcher icon is missing from merged manifest")
    if not re.search(r"application:.*icon='[^']+'", badging):
        raise ValueError("Android launcher icon is not resolved by aapt")
    if not re.search(
        r"android:allowBackup\(0x01010280\)=\(type 0x12\)0x0\b", manifest
    ):
        raise ValueError("Manifest does not explicitly disable backup")
    for attribute in ("android:fullBackupContent(", "android:dataExtractionRules("):
        if attribute not in manifest:
            raise ValueError(f"Manifest missing prototype backup exclusion reference: {attribute}")


def audit_archive(apk, source_audio):
    with zipfile.ZipFile(apk) as archive:
        paths = set(archive.namelist())
        native_paths = {p for p in paths if p.startswith("lib/") and p.endswith(".so")}
        if any(not p.startswith("lib/arm64-v8a/") for p in native_paths):
            raise ValueError("Unexpected native ABI")
        actual_native = {Path(p).name for p in native_paths}
        if not EXPECTED_NATIVE.issubset(actual_native):
            raise ValueError(f"Missing native libraries: {sorted(EXPECTED_NATIVE - actual_native)}")
        if not EXPECTED_ASSETS.issubset(paths):
            raise ValueError(f"Missing assets/notices: {sorted(EXPECTED_ASSETS - paths)}")
        for path in EXPECTED_ASSETS:
            if archive.getinfo(path).file_size == 0:
                raise ValueError(f"Empty asset: {path}")
        icon_files = [p for p in paths if p.endswith("/sudokura_icon_source.png")]
        if len(icon_files) != 1:
            raise ValueError(f"Expected one generated launcher icon: {icon_files}")
        icon_png = archive.read(icon_files[0])
        if not (icon_png.startswith(b"\x89PNG\r\n\x1a\n") and len(icon_png) >= 24):
            raise ValueError("Launcher icon is not a PNG")
        if struct.unpack(">II", icon_png[16:24]) != (512, 512):
            raise ValueError("Launcher icon does not match 512x512 master dimensions")
        for name in AUDIO_NAMES:
            packed = archive.read(f"assets/audio/{name}")
            local = (source_audio / name).read_bytes()
            if hashlib.sha256(packed).digest() != hashlib.sha256(local).digest():
                raise ValueError(f"Altered Android OGG resource: {name}")
    return sorted(native_paths)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("aapt", help="Path to the Android aapt executable")
    parser.add_argument("apk", type=Path, help="Debug APK to inspect")
    # Production identity/signing remains separate; unsupported variants fail closed.
    parser.add_argument("--variant", choices=("debug",), default="debug")
    args = parser.parse_args(argv)
    try:
        if not args.apk.is_file():
            raise ValueError(f"Missing APK: {args.apk}")
        identity = expected_identity(
            (ROOT / "src/version.h").read_text(encoding="utf-8"), args.variant)
        badging = subprocess.run(
            [args.aapt, "dump", "badging", str(args.apk)], check=True,
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout
        manifest = subprocess.run(
            [args.aapt, "dump", "xmltree", str(args.apk), "AndroidManifest.xml"],
            check=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout
        audit_manifest(badging, manifest, identity)
        native = audit_archive(args.apk, ROOT / "assets/audio")
    except (ValueError, OSError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        raise SystemExit(f"APK audit failed: {error}") from error
    print(json.dumps({
        "apk": str(args.apk),
        "bytes": args.apk.stat().st_size,
        "native": native,
        "permissions": [],
        **identity,
        "android_backup": False,
        "asset_count": len(EXPECTED_ASSETS),
        "device_test": "NOT_RUN",
    }, indent=2))


if __name__ == "__main__":
    main()
