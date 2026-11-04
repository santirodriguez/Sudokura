#!/usr/bin/env python3
"""Check PT_LOAD page alignment of every packaged shared library for the selected ABI."""
from pathlib import Path
import argparse
import re
import subprocess
import zipfile
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('readelf')
parser.add_argument('apk')
parser.add_argument('--abi', choices=['arm64-v8a', 'x86_64'], default='arm64-v8a')
args = parser.parse_args()
readelf, apk = args.readelf, args.apk
checked = 0
with tempfile.TemporaryDirectory(prefix="sudokura-elf-audit-") as temp:
    with zipfile.ZipFile(apk) as archive:
        libraries = sorted(name for name in archive.namelist()
                           if name.startswith("lib/") and name.endswith(".so"))
        if not libraries:
            raise SystemExit("Missing native libraries")
        for name in libraries:
            if not name.startswith(f"lib/{args.abi}/"):
                raise SystemExit(f"Unexpected ABI: {name}")
            target = Path(temp) / Path(name).name
            target.write_bytes(archive.read(name))
            output = subprocess.run(
                [readelf, "--program-headers", "--wide", str(target)],
                text=True, stdout=subprocess.PIPE, check=True).stdout
            load_alignments = []
            for line in output.splitlines():
                if re.match(r"^\s*LOAD\s+", line):
                    value = line.split()[-1]
                    load_alignments.append(int(value, 16))
            if not load_alignments:
                raise SystemExit(f"No ELF PT_LOAD segments: {name}")
            if any(alignment < 16384 for alignment in load_alignments):
                raise SystemExit(
                    f"16 KiB PT_LOAD alignment failed: {name}: {load_alignments}")
            print(f"{name}: PT_LOAD alignments={load_alignments}")
            checked += 1
print(f"ELF 16 KiB structural audit PASS: {checked} {args.abi} shared libraries")
print("Runtime on an actual 16 KiB Android device/emulator remains NOT VERIFIED")
