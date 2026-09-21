#!/usr/bin/env python3
import json
import pathlib
import plistlib
import re
import subprocess
import sys

app = pathlib.Path(sys.argv[1])
expected_version = sys.argv[2]
minimum = sys.argv[3]

def run(*args, check=True):
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if check and result.returncode != 0:
        raise SystemExit(f"{' '.join(args)} failed:\n{result.stdout}{result.stderr}")
    return result.stdout + result.stderr

def version_tuple(value):
    return tuple(int(part) for part in value.split("."))

plist_path = app / "Contents" / "Info.plist"
with plist_path.open("rb") as handle:
    plist = plistlib.load(handle)

if plist.get("CFBundleShortVersionString") != expected_version:
    raise SystemExit("CFBundleShortVersionString mismatch")
if plist.get("CFBundleVersion") != expected_version:
    raise SystemExit("CFBundleVersion mismatch")
if plist.get("LSMinimumSystemVersion") != minimum:
    raise SystemExit("LSMinimumSystemVersion mismatch")

mach_o = []
for path in sorted(p for p in app.rglob("*") if p.is_file()):
    kind = run("file", "-b", str(path))
    if "Mach-O" not in kind:
        continue
    archs = run("lipo", "-archs", str(path)).strip().split()
    if archs != ["arm64"]:
        raise SystemExit(f"{path}: expected arm64-only Mach-O, got {archs}")

    loads = run("otool", "-L", str(path)).splitlines()[1:]
    deps = []
    for line in loads:
        dep = line.strip().split(" (", 1)[0]
        deps.append(dep)
        if dep.startswith(("@rpath/", "@loader_path/", "@executable_path/", "/usr/lib/", "/System/Library/")):
            continue
        raise SystemExit(f"{path}: unbundled or build-host dependency {dep}")

    load_commands = run("otool", "-l", str(path)).splitlines()
    min_versions = []
    rpaths = []
    for idx, line in enumerate(load_commands):
        stripped = line.strip()
        if stripped == "cmd LC_BUILD_VERSION":
            for follow in load_commands[idx + 1: idx + 10]:
                match = re.match(r"\s*minos\s+([0-9.]+)", follow)
                if match:
                    min_versions.append(match.group(1))
                    break
        elif stripped == "cmd LC_VERSION_MIN_MACOSX":
            for follow in load_commands[idx + 1: idx + 8]:
                match = re.match(r"\s*version\s+([0-9.]+)", follow)
                if match:
                    min_versions.append(match.group(1))
                    break
        elif stripped == "cmd LC_RPATH":
            for follow in load_commands[idx + 1: idx + 8]:
                match = re.match(r"\s*path\s+([^ ]+)", follow)
                if match:
                    rpaths.append(match.group(1))
                    break

    if not min_versions:
        raise SystemExit(f"{path}: no macOS deployment target load command found")
    for value in min_versions:
        if version_tuple(value) > version_tuple(minimum):
            raise SystemExit(f"{path}: minimum {value} exceeds declared {minimum}")
    for rpath in rpaths:
        if rpath.startswith(("/opt/homebrew", "/usr/local", "/Users/", "/private/var/folders/")):
            raise SystemExit(f"{path}: build-host rpath remains: {rpath}")

    run("codesign", "--verify", "--strict", "--verbose=2", str(path))
    mach_o.append({
        "path": str(path.relative_to(app)),
        "architectures": archs,
        "minimum_versions": min_versions,
        "dependencies": deps,
        "rpaths": rpaths,
    })

if not mach_o:
    raise SystemExit("no Mach-O files found in app bundle")

run("codesign", "--verify", "--deep", "--strict", "--verbose=2", str(app))
signature = run("codesign", "-dv", "--verbose=4", str(app), check=False)
if "Signature=adhoc" not in signature:
    raise SystemExit("final bundle is not carrying the expected ad-hoc integrity signature")

report = {
    "bundle": app.name,
    "version": expected_version,
    "declared_minimum": minimum,
    "architecture": "arm64",
    "signature": "ad-hoc integrity only; not Developer ID and not notarized",
    "mach_o": mach_o,
}
print(json.dumps(report, indent=2, sort_keys=True))
