#!/usr/bin/env python3
"""Reject stale release-facing copy, missing local links and invalid screenshots.

This is a structural/source-consistency check. It does NOT approve screenshots
aesthetically, sign an APK, create a release, or verify external URLs.
"""
from pathlib import Path
import re
import struct

root = Path(__file__).resolve().parents[1]
read = lambda name: (root / name).read_text(encoding="utf-8")
version_header = read("src/version.h")


def macro(name):
    match = re.search(r"^#define\s+" + re.escape(name) + r"\s+(\d+)\s*$",
                      version_header, re.M)
    assert match, name
    return int(match.group(1))


version = ".".join(str(macro("SUDOKURA_VERSION_" + part))
                   for part in ("MAJOR", "MINOR", "PATCH"))
code = macro("SUDOKURA_VERSION_ANDROID_CODE")
assert version == "1.4.1" and code == 2, (version, code)

readme = read("README.md")
changelog = read("CHANGELOG.md")
docs_index = read("docs/README.md")
gallery = read("docs/images/README.md")
release_notes_path = f"docs/RELEASE_NOTES_{version}.md"
release_notes = read(release_notes_path)
assert re.match(r"# Changelog\s+## \[" + re.escape(version) + r"\]", changelog)
assert release_notes.strip() and not release_notes.startswith("# ")
assert f"docs/RELEASE_NOTES_{version}.md" in readme
assert f"RELEASE_NOTES_{version}.md" in docs_index
assert f"RELEASE_NOTES_{version}.md" in gallery
assert "https://github.com/santirodriguez/Sudokura/releases/latest" in readme
assert "releases/download/v1.4.0/" not in readme
assert "once v1.4.0 is published" not in readme.lower()
assert "once that release is published" not in readme.lower()
assert "without uninstall" in readme
assert "1.4.0" in gallery and "1.4.1" in gallery
assert "16 KiB" in release_notes and "TalkBack" in release_notes

for locale in ("en-US", "es", "ca"):
    folder = f"fastlane/metadata/android/{locale}"
    assert (root / folder / "title.txt").is_file()
    assert (root / folder / "full_description.txt").is_file()
    assert (root / folder / "short_description.txt").is_file()
    current = read(f"{folder}/changelogs/{code}.txt")
    assert version in current and len(current.strip()) < 500

# Source-owned six canonical images: real 1.4.0 captures reused *with their
# provenance*, NOT rebranded synthetic 1.4.1 screenshots.
screenshots = root / "fastlane/metadata/android/en-US/images/phoneScreenshots"
names = [f"{n:02d}-{role}.png" for n, role in enumerate(
    ("home-dark", "classic-dark", "daily-dark", "achieved-dark",
     "try-again-dark", "saved-games-light"), 1)]
assert sorted(p.name for p in screenshots.glob("*.png")) == sorted(names)
for name in names:
    header = (screenshots / name).read_bytes()[:26]
    assert header[:8] == b"\x89PNG\r\n\x1a\n", name
    assert struct.unpack(">II", header[16:24]) == (1080, 1920), name
    assert header[24:26] == b"\x08\x02", name  # 8-bit RGB, no alpha
assert (root / "docs/images/sudokura-v1.4.0.png").is_file()

# Resolve local Markdown and HTML links in the release-facing corpus. External
# URLs / same-page anchors are intentionally not tested by this checker.
documents = ("README.md", "CHANGELOG.md", "docs/README.md",
             "docs/images/README.md", "docs/REPORTING_ISSUES.md",
             "docs/DEVELOPMENT.md", "docs/ANDROID_RELEASE_PLAN.md",
             "docs/ANDROID_STORE_PREPARATION.md",
             "android/README.md", "android/RELEASING.md", release_notes_path)
for document in documents:
    source = read(document)
    links = re.findall(r"\]\(([^)\s]+)", source)
    links += re.findall(r"(?:href|src)=[\"']([^\"']+)[\"']", source)
    for link in links:
        if (link.startswith(("http:", "https:", "mailto:", "#", "data:", "//"))
                or not link.split("#", 1)[0]):
            continue
        relative = link.split("#", 1)[0].split("?", 1)[0]
        target = (root / document).parent / relative
        assert target.exists(), f"{document}: broken local link {link!r}"

print(f"PASS: v{version}/code{code} copy, local links, locales and genuine PNG structure")
