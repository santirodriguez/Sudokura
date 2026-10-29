#!/usr/bin/env python3
"""Check PT_LOAD page alignment of every packaged arm64 shared library."""
from pathlib import Path
import re
import subprocess
import sys
import zipfile
import tempfile

if len(sys.argv) != 3:
    raise SystemExit("usage: audit_elf.py <llvm-readelf> <apk>")
readelf, apk = sys.argv[1:]
checked = 0
with tempfile.TemporaryDirectory(prefix="sudokura-elf-audit-") as temp:
    with zipfile.ZipFile(apk) as archive:
        libraries = sorted(name for name in archive.namelist()
                           if name.startswith("lib/") and name.endswith(".so"))
        if not libraries:
            raise SystemExit("Missing native libraries")
        for name in libraries:
            if not name.startswith("lib/arm64-v8a/"):
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
print(f"ELF 16 KiB structural audit PASS: {checked} arm64 shared libraries")
print("Runtime on an actual 16 KiB Android device/emulator remains NOT VERIFIED")
