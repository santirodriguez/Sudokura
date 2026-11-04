#!/usr/bin/env python3
"""Regression tests for the public release-asset validator."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "release_bundle.py"
VERSION_SCRIPT = ROOT / "scripts" / "version.sh"


def run(*args):
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )


version_result = subprocess.run(
    [str(VERSION_SCRIPT)],
    cwd=ROOT,
    capture_output=True,
    text=True,
)
assert version_result.returncode == 0, version_result.stderr
VERSION = version_result.stdout.strip()

labels_result = run("labels", "--version", VERSION)
assert labels_result.returncode == 0, labels_result.stderr
contract = []
for line in labels_result.stdout.splitlines():
    name, label = line.split("\t", 1)
    contract.append((name, label))


def payload(prefix):
    return [
        {
            "name": name,
            "label": label,
            "state": "uploaded",
            "browser_download_url": prefix + name,
        }
        for name, label in contract
    ]


def validate(assets, *extra):
    with tempfile.TemporaryDirectory(prefix="sudokura-release-assets-") as temp:
        path = Path(temp) / "assets.json"
        path.write_text(json.dumps(assets), encoding="utf-8")
        return run(
            "validate-assets",
            "--version",
            VERSION,
            "--assets-json",
            str(path),
            *extra,
        )


draft_prefix = (
    "https://github.com/santirodriguez/Sudokura/releases/download/"
    "untagged-abcdef123456/"
)
published_prefix = (
    f"https://github.com/santirodriguez/Sudokura/releases/download/v{VERSION}/"
)

result = validate(payload(draft_prefix))
assert result.returncode == 0, result.stderr
print("PASS draft asset URLs")

result = validate(payload(published_prefix))
assert result.returncode == 0, result.stderr
print("PASS published asset URLs in draft-compatible mode")

result = validate(payload(published_prefix), "--published")
assert result.returncode == 0, result.stderr
print("PASS published asset URLs in published mode")

result = validate(payload(draft_prefix), "--published")
assert result.returncode != 0
assert "published release URL" in result.stderr
print("PASS reject draft URL after publication")

wrong_name = payload(draft_prefix)
wrong_name[0]["browser_download_url"] = draft_prefix + "wrong.exe"
result = validate(wrong_name)
assert result.returncode != 0
assert "unexpected browser_download_url" in result.stderr
print("PASS reject mismatched asset URL filename")

wrong_label = payload(draft_prefix)
wrong_label[0]["label"] = "Wrong label"
result = validate(wrong_label)
assert result.returncode != 0
assert "label mismatch" in result.stderr
print("PASS reject mismatched asset label")
