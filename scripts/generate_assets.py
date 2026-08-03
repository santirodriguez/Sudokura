#!/usr/bin/env python3
"""Run the dependency-free, reproducible Go icon generator."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
subprocess.run(["go", "run", str(root / "scripts/generate_assets.go")], cwd=root, check=True)
