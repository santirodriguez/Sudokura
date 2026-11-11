#!/usr/bin/env python3
"""Fail if a package omits or changes the original flag-icons MIT notice."""
import argparse
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "assets/flags/LICENSE-MIT.txt"


def verify(payload, member=None, source=SOURCE):
    expected = Path(source).read_bytes()
    if b"\r" in expected:
        raise ValueError("Canonical flag-icons MIT notice must retain LF line endings")
    if (b"Copyright (c) 2013 Panayiotis Lipiridis" not in expected or
            b"Permission is hereby granted" not in expected):
        raise ValueError("Canonical flag-icons MIT notice is invalid")
    if member is None:
        actual = Path(payload).read_bytes()
    else:
        with zipfile.ZipFile(payload) as archive:
            actual = archive.read(member)
    if actual != expected:
        raise ValueError("Packaged flag-icons MIT notice differs from source")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("payload", type=Path)
    parser.add_argument("--member", help="ZIP member; omit for a file")
    args = parser.parse_args(argv)
    try:
        verify(args.payload, args.member)
    except (OSError, KeyError, ValueError, zipfile.BadZipFile) as error:
        parser.exit(1, f"Flag-icons license check failed: {error}\n")
    print("flag-icons MIT notice: PASS")


if __name__ == "__main__":
    main()
