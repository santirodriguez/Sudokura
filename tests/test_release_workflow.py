#!/usr/bin/env python3
"""Exercise the v1.4.1 manual release staging contract without credentials."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
workflows = root / ".github" / "workflows"
release = (workflows / "release.yml").read_text(encoding="utf-8")
android = (workflows / "android-preview.yml").read_text(encoding="utf-8")
ci = (workflows / "ci.yml").read_text(encoding="utf-8")
packager = (root / "scripts/release_bundle.py").read_text(encoding="utf-8")

# The signing and packaging workflow is NOT automatically invoked by a tag
# or push and cannot create a GitHub Release or publish anything.
assert release.startswith("name: Package Sudokura release\n")
assert "\n  workflow_dispatch:\n" in release
assert "\n  push:\n" not in release
assert "\n  draft-release:\n" not in release
assert "\n  publish-release:\n" not in release
assert "approved_source_sha:" in release
assert "required: true" in release
assert 'test "$GITHUB_ACTOR" = santirodriguez' in release
assert 'test "$GITHUB_REF" = refs/heads/release/1.4.1' in release
assert 'test "$GITHUB_SHA" = "$APPROVED_SOURCE_SHA"' in release
assert 'test "$(sh scripts/version.sh)" = 1.4.1' in release

# Production keys are absent from the reusable Android and ordinary CI jobs.
assert "  android-sign:" not in android
assert "secrets.SUDOKURA_APP_KEYSTORE_" not in android
assert "secrets.SUDOKURA_APP_KEYSTORE_" not in ci
for job in ("version", "android", "android-sign", "linux", "linux-ubuntu24",
            "linux-fedora44", "windows", "macos", "release-bundle"):
    assert "\n  " + job + ":\n" in release, job

sign_job = release.split("\n  android-sign:\n", 1)[1].split("\n  linux:\n", 1)[0]
assert "needs: [version, android]" in sign_job
assert "github.event_name == 'workflow_dispatch'" in sign_job
assert "github.actor == 'santirodriguez'" in sign_job
assert "Verify approved source and downloaded unsigned inputs" in sign_job
assert "python3 android/scripts/verify_signing_input.py" in sign_job
assert sign_job.index("Verify approved source and downloaded unsigned inputs") < (
    sign_job.index("Sign production APK with the original Sudokura app key"))
assert sign_job.count("secrets.SUDOKURA_APP_KEYSTORE_BASE64") == 1
assert sign_job.count("secrets.SUDOKURA_APP_KEYSTORE_PASSWORD") == 1

# Five platform packages plus a source-bound Build-Info and SHA256SUMS
# constitute the seven uploadable release files; the bundle is internal.
bundle = release.split("\n  release-bundle:\n", 1)[1]
assert "needs: [version, linux, linux-ubuntu24, linux-fedora44, windows, macos, android-sign]" in bundle
assert "--include-android" in bundle
asset_names = [
    "Sudokura-*-Windows-x64-Setup.exe",
    "Sudokura-*-Windows-x64-Portable.exe",
    "Sudokura-*-Linux-x64.AppImage",
    "Sudokura-*-macOS-arm64.dmg",
    "Sudokura-*-Android-arm64.apk",
    "SHA256SUMS.txt",
    "Sudokura-*-Build-Info.json",
]
uploads = bundle.split("name: release-bundle", 1)[1]
for name in asset_names:
    assert ("artifacts/" + name) in uploads, name
assert 'f"Sudokura-{version}-Android-arm64.apk"' in packager
assert 'f"Sudokura-{version}-Build-Info.json"' in packager
assert "if include_android:" in packager

# The two source-only release checks run on Linux CI.
assert "python3 tests/test_release_workflow.py" in ci
assert "python3 tests/test_release_readiness.py" in ci
print("PASS: manual-only release, source-bound signing and seven-file staging contract")
