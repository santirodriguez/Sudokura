#!/usr/bin/env python3
"""Fail-closed structural audit of prototype/production APKs, not device acceptance."""
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
    "assets/licenses/flag-icons-MIT.txt",
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
    if variant not in ("debug", "release"):
        raise ValueError("Unsupported APK variant")
    components = []
    for name in ("MAJOR", "MINOR", "PATCH", "ANDROID_CODE"):
        values = re.findall(
            rf"^#define[ \t]+SUDOKURA_VERSION_{name}[ \t]+([^\n]*)$",
            version_header, flags=re.MULTILINE)
        if len(values) != 1 or not re.fullmatch(r"[0-9]+", values[0].strip()):
            raise ValueError(f"Invalid or ambiguous version component: {name}")
        components.append(int(values[0].strip()))
    major, minor, patch, code = components
    if minor > 99 or patch > 99:
        raise ValueError("versionCode requires two-digit minor/patch")
    if not 0 < code <= 2_100_000_000:
        raise ValueError("versionCode must be between 1 and 2100000000")
    return {
        "application_id": "org.sudokura.prototype.debug" if variant == "debug" else "org.cajapersonal.sudokura",
        "version_code": code,
        "version_name": f"{major}.{minor}.{patch}" + ("-prototype" if variant == "debug" else ""),
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
    if identity["build_variant"] == "release" and (
            "application-debuggable" in badging or re.search(
                r"android:debuggable\([^)]*\)=\(type 0x12\)0xffffffff\b", manifest)):
        raise ValueError("Production release must not be debuggable")
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


def packaged_resources(resources, manifest):
    """Resolve manifest IDs and the canonical icon through aapt's resource table.

    Release resource paths may be shortened; source filenames are not ABI.
    """
    entries = list(re.finditer(r"(?m)^[ \t]*resource\s+(0x[0-9a-fA-F]+)\s+([^\n]*)", resources))
    blocks = []
    for index, entry in enumerate(entries):
        end = entries[index + 1].start() if index + 1 < len(entries) else len(resources)
        paths = set(re.findall(r"res/[A-Za-z0-9_./+-]+\.(?:xml|png)", resources[entry.start():end]))
        blocks.append((entry.group(1).lower(), entry.group(2), paths))
    resolved = {}
    for name in ("fullBackupContent", "dataExtractionRules"):
        ids = re.findall(r"android:" + name + r"\([^)]*\)\s*=\s*@(0x[0-9a-fA-F]+)", manifest)
        if len(ids) != 1:
            raise ValueError("Missing or ambiguous backup resource ID: " + name)
        paths = set().union(*(paths for resource_id, _, paths in blocks if resource_id == ids[0].lower()))
        if len(paths) != 1 or not next(iter(paths)).endswith(".xml"):
            raise ValueError("Missing or ambiguous packaged XML: " + name)
        resolved[name] = paths.pop()
    icons = set().union(*(paths for _, name, paths in blocks
                         if re.search(r":drawable/sudokura_icon_source(?:\s|:|$)", name)))
    if len(icons) != 1 or not next(iter(icons)).endswith(".png"):
        raise ValueError("Canonical launcher PNG resource is missing or ambiguous")
    resolved["icon"] = icons.pop()
    return resolved


def audit_backup_xml(dump, modern=False):
    """Check the actual compiled exclusion tree, not just a manifest pointer."""
    roots, stack = [], []
    for line in dump.splitlines():
        element = re.match(r"^( *)(?:E:) ([A-Za-z0-9_-]+)(?: |$)", line)
        if element:
            indent = len(element.group(1))
            while stack and stack[-1][0] >= indent:
                stack.pop()
            node = {"tag": element.group(2), "attrs": {}, "children": []}
            (stack[-1][1]["children"] if stack else roots).append(node)
            stack.append((indent, node))
        else:
            attribute = re.match(r'^ *A: ([A-Za-z0-9_:-]+)(?:\([^)]*\))?="([^"\n]*)"', line)
            if attribute and stack:
                name = attribute.group(1)
                if name in stack[-1][1]["attrs"]:
                    raise ValueError("Duplicate compiled backup attribute")
                stack[-1][1]["attrs"][name] = attribute.group(2)
    expected_root = "data-extraction-rules" if modern else "full-backup-content"
    if len(roots) != 1 or roots[0]["tag"] != expected_root:
        raise ValueError("Unexpected compiled backup root")
    groups = roots[0]["children"] if modern else roots
    if modern and (len(groups) != 2 or {g["tag"] for g in groups} != {"cloud-backup", "device-transfer"}):
        raise ValueError("Missing cloud/device transfer exclusions")
    expected = {("root", "."), ("file", "."), ("database", "."), ("sharedpref", "."), ("external", ".")}
    for group in groups:
        children = group["children"]
        exclusions = {(n["attrs"].get("domain"), n["attrs"].get("path")) for n in children}
        if len(children) != 5 or exclusions != expected or any(n["tag"] != "exclude" or n["children"] for n in children):
            raise ValueError("Incomplete compiled backup exclusions")


def audit_archive(apk, source_audio, packaged=None):
    with zipfile.ZipFile(apk) as archive:
        paths = set(archive.namelist())
        native_paths = {p for p in paths if p.startswith("lib/") and p.endswith(".so")}
        if any(not p.startswith("lib/arm64-v8a/") for p in native_paths):
            raise ValueError("Unexpected native ABI")
        actual_native = {Path(p).name for p in native_paths}
        if not EXPECTED_NATIVE.issubset(actual_native):
            raise ValueError(f"Missing native libraries: {sorted(EXPECTED_NATIVE - actual_native)}")
        required = EXPECTED_ASSETS
        if packaged:
            required = (required - {"res/xml/backup_rules.xml", "res/xml/data_extraction_rules.xml"}) | {
                packaged["fullBackupContent"], packaged["dataExtractionRules"]}
        if not required.issubset(paths):
            raise ValueError(f"Missing assets/notices: {sorted(required - paths)}")
        for path in required:
            if archive.getinfo(path).file_size == 0:
                raise ValueError(f"Empty asset: {path}")
        if packaged:
            icon_files = [packaged["icon"]] if packaged["icon"] in paths else []
        else:
            icon_files = [p for p in paths if p.endswith("/sudokura_icon_source.png")]
        if len(icon_files) != 1:
            raise ValueError(f"Expected one generated launcher icon: {icon_files}")
        icon_png = archive.read(icon_files[0])
        if not (icon_png.startswith(b"\x89PNG\r\n\x1a\n") and len(icon_png) >= 24):
            raise ValueError("Launcher icon is not a PNG")
        if struct.unpack(">II", icon_png[16:24]) != (512, 512):
            raise ValueError("Launcher icon does not match 512x512 master dimensions")
        if (archive.read("assets/licenses/flag-icons-MIT.txt") !=
                (ROOT / "assets/flags/LICENSE-MIT.txt").read_bytes()):
            raise ValueError("Altered flag-icons MIT notice")
        for name in AUDIO_NAMES:
            packed = archive.read(f"assets/audio/{name}")
            local = (source_audio / name).read_bytes()
            if hashlib.sha256(packed).digest() != hashlib.sha256(local).digest():
                raise ValueError(f"Altered Android OGG resource: {name}")
    return sorted(native_paths)


def audit_unsigned(apk):
    """Refuse signature material; this is preparation, never signing approval."""
    with zipfile.ZipFile(apk) as archive:
        if any(re.fullmatch(r"META-INF/[^/]+\.(SF|RSA|DSA|EC)", path, re.IGNORECASE)
               for path in archive.namelist()):
            raise ValueError("Unsigned candidate contains JAR signature material")
        central_directory = archive.start_dir
    if central_directory >= 24:
        with open(apk, "rb") as stream:
            stream.seek(central_directory - 16)
            if stream.read(16) == b"APK Sig Block 42":
                raise ValueError("Unsigned candidate contains an APK signing block")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("aapt", help="Path to the Android aapt executable")
    parser.add_argument("apk", type=Path, help="APK to inspect")
    # Production identity/signing remains separate; unsupported variants fail closed.
    parser.add_argument("--variant", choices=("debug", "release"), default="debug")
    parser.add_argument("--require-unsigned", action="store_true",
                        help="Refuse signing material in a production preparation APK")
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
        resources = subprocess.run(
            [args.aapt, "dump", "--values", "resources", str(args.apk)], check=True,
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout
        packaged = packaged_resources(resources, manifest)
        for name in ("fullBackupContent", "dataExtractionRules"):
            tree = subprocess.run(
                [args.aapt, "dump", "xmltree", str(args.apk), packaged[name]], check=True,
                text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout
            audit_backup_xml(tree, modern=name == "dataExtractionRules")
        native = audit_archive(args.apk, ROOT / "assets/audio", packaged)
        if args.require_unsigned:
            if args.variant != "release":
                raise ValueError("Unsigned preparation requires the release variant")
            audit_unsigned(args.apk)
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
        "unsigned_preparation": args.require_unsigned,
    }, indent=2))


if __name__ == "__main__":
    main()
