#!/usr/bin/env python3
"""Run the dependency-free, reproducible Go asset generator."""
from pathlib import Path
import os
import shutil
import subprocess


def find_go_executable() -> str:
    """Locate Go without requiring setup-go's Windows PATH inside MSYS2."""
    explicit = os.environ.get("SUDOKURA_GO")
    if explicit:
        candidate = Path(explicit)
        if candidate.is_file():
            return str(candidate)
        raise SystemExit(f"SUDOKURA_GO does not point to a file: {explicit}")

    for name in ("go", "go.exe"):
        found = shutil.which(name)
        if found:
            return found

    goroot = os.environ.get("GOROOT")
    if goroot:
        for name in ("go.exe", "go"):
            candidate = Path(goroot) / "bin" / name
            if candidate.is_file():
                return str(candidate)

    raise SystemExit(
        "Go toolchain not found. Install Go or set SUDOKURA_GO to the Go executable."
    )


root = Path(__file__).resolve().parents[1]
go = find_go_executable()
subprocess.run([go, "version"], cwd=root, check=True)
subprocess.run([go, "run", str(root / "scripts/generate_assets.go")], cwd=root, check=True)
