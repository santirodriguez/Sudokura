#!/usr/bin/env python3
import argparse
import hashlib
import json
import pathlib
import re
import shutil
import sys
import tempfile
from types import SimpleNamespace

PRODUCT = "Sudokura"
VERSION_RE = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def contract(version, include_android=False):
    build_info = f"Sudokura-{version}-Build-Info.json"
    entries = [
        {
            "name": f"Sudokura-{version}-Windows-x64-Setup.exe",
            "label": "Windows — Installer (x64)",
            "role": "application",
            "platform": "windows",
            "architecture": "x86_64",
            "manifest": "artifact-manifest-windows.json",
        },
        {
            "name": f"Sudokura-{version}-Windows-x64-Portable.exe",
            "label": "Windows — Portable (x64, no installation)",
            "role": "application",
            "platform": "windows",
            "architecture": "x86_64",
            "manifest": "artifact-manifest-windows.json",
        },
        {
            "name": f"Sudokura-{version}-Linux-x64.AppImage",
            "label": "Linux — AppImage (x64)",
            "role": "application",
            "platform": "linux",
            "architecture": "x86_64",
            "manifest": "artifact-manifest-linux.json",
        },
        {
            "name": f"Sudokura-{version}-macOS-arm64.dmg",
            "label": "macOS — Apple Silicon (experimental)",
            "role": "application",
            "platform": "macos",
            "architecture": "arm64",
            "manifest": "artifact-manifest-macos-arm64.json",
        },
        {
            "name": "SHA256SUMS.txt",
            "label": "Checksums — SHA-256",
            "role": "technical",
        },
        {
            "name": build_info,
            "label": "Build details — JSON",
            "role": "technical",
        },
    ]

    if include_android:
        entries.insert(4, {
            "name": f"Sudokura-{version}-Android-arm64.apk",
            "label": "Android — APK (arm64)", "role": "application",
            "platform": "android", "architecture": "arm64",
            "manifest": "artifact-manifest-android.json",
        })
    return entries


def manifest_specs(version, include_android=False):
    specs = [
        {
            "key": "linux",
            "file": "artifact-manifest-linux.json",
            "checksums": "SHA256SUMS-linux.txt",
            "platform": "linux",
            "architecture": "x86_64",
            "expected": {f"Sudokura-{version}-Linux-x64.AppImage"},
        },
        {
            "key": "windows",
            "file": "artifact-manifest-windows.json",
            "checksums": "SHA256SUMS-windows.txt",
            "platform": "windows",
            "architecture": "x86_64",
            "expected": {
                f"Sudokura-{version}-Windows-x64-Portable.exe",
                f"Sudokura-{version}-Windows-x64-Setup.exe",
            },
        },
        {
            "key": "macos_arm64",
            "file": "artifact-manifest-macos-arm64.json",
            "checksums": "SHA256SUMS-macos-arm64.txt",
            "platform": "macos",
            "architecture": "arm64",
            "expected": {f"Sudokura-{version}-macOS-arm64.dmg"},
        },
    ]

    if include_android:
        specs.append({
            "key": "android", "file": "artifact-manifest-android.json",
            "checksums": "SHA256SUMS-android.txt", "platform": "android",
            "architecture": "arm64",
            "expected": {f"Sudokura-{version}-Android-arm64.apk"},
        })
    return specs


def parse_checksums(path):
    values = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line:
            continue
        parts = line.split(maxsplit=1)
        if len(parts) != 2:
            raise SystemExit(f"invalid checksum line in {path}: {raw!r}")
        digest, name = parts
        name = name.lstrip("*")
        if not re.fullmatch(r"[0-9a-fA-F]{64}", digest):
            raise SystemExit(f"invalid SHA-256 in {path}: {digest}")
        if name in values:
            raise SystemExit(f"duplicate checksum entry in {path}: {name}")
        values[name] = digest.lower()
    return values


def validate_identity(document, spec, version, source_commit):
    if document.get("product") != PRODUCT:
        raise SystemExit(f"{spec['file']}: product mismatch")
    if document.get("version") != version:
        raise SystemExit(f"{spec['file']}: version mismatch")
    if document.get("source_commit") != source_commit:
        raise SystemExit(f"{spec['file']}: source commit mismatch")
    if document.get("platform") != spec["platform"]:
        raise SystemExit(f"{spec['file']}: platform mismatch")
    if document.get("architecture") != spec["architecture"]:
        raise SystemExit(f"{spec['file']}: architecture mismatch")
    if not document.get("declared_minimum"):
        raise SystemExit(f"{spec['file']}: missing declared_minimum")
    if not document.get("reproducibility_claim"):
        raise SystemExit(f"{spec['file']}: missing reproducibility_claim")


def build(args):
    if not VERSION_RE.fullmatch(args.version):
        raise SystemExit(f"invalid version: {args.version}")
    if not COMMIT_RE.fullmatch(args.source_commit):
        raise SystemExit(f"invalid source commit: {args.source_commit}")

    root = pathlib.Path(args.artifacts_dir)
    if not root.is_dir():
        raise SystemExit(f"missing artifacts directory: {root}")

    release_contract = contract(args.version, args.include_android)
    application_contract = {
        entry["name"]: entry for entry in release_contract if entry["role"] == "application"
    }
    manifests = {}
    public_assets = []
    seen_names = set()
    artifact_kinds = set()

    for spec in manifest_specs(args.version, args.include_android):
        manifest_path = root / spec["file"]
        checksum_path = root / spec["checksums"]
        if not manifest_path.is_file():
            raise SystemExit(f"missing manifest: {manifest_path}")
        if not checksum_path.is_file():
            raise SystemExit(f"missing per-platform checksums: {checksum_path}")

        document = json.loads(manifest_path.read_text(encoding="utf-8"))
        validate_identity(document, spec, args.version, args.source_commit)
        if spec["platform"] == "android":
            codes = re.findall(r"^#define[ \t]+SUDOKURA_VERSION_ANDROID_CODE[ \t]+([0-9]+)[ \t]*$",
                               (pathlib.Path(__file__).resolve().parents[1] / "src/version.h").read_text(), re.M)
            if len(codes) != 1 or not 0 < int(codes[0]) <= 2100000000:
                raise SystemExit("invalid Android version code")
            if (document.get("application_id") != "org.cajapersonal.sudokura"
                    or document.get("version_code") != int(codes[0])
                    or document.get("signing_receipt_verified") is not True
                    or document.get("signing_certificate_sha256") !=
                    "fde951e3d2ef42e41c7186ee965944aeca549d590ad080711dd3d875f82df66d"):
                raise SystemExit("Android identity/signing receipt mismatch")
        artifact_kinds.add(document.get("artifact_kind"))

        manifest_artifacts = document.get("artifacts")
        if not isinstance(manifest_artifacts, list):
            raise SystemExit(f"{spec['file']}: artifacts must be a list")
        names = {item.get("name") for item in manifest_artifacts}
        if names != spec["expected"]:
            raise SystemExit(
                f"{spec['file']}: expected artifacts {sorted(spec['expected'])}, got {sorted(names)}"
            )

        checksums = parse_checksums(checksum_path)
        if set(checksums) != spec["expected"]:
            raise SystemExit(
                f"{spec['checksums']}: expected entries {sorted(spec['expected'])}, got {sorted(checksums)}"
            )

        for item in manifest_artifacts:
            name = item["name"]
            if name in seen_names:
                raise SystemExit(f"duplicate public artifact across manifests: {name}")
            seen_names.add(name)

            expected_contract = application_contract.get(name)
            if expected_contract is None:
                raise SystemExit(f"manifest contains non-contract artifact: {name}")

            artifact_path = root / name
            if not artifact_path.is_file():
                raise SystemExit(f"missing public artifact: {artifact_path}")
            actual_size = artifact_path.stat().st_size
            actual_hash = sha256(artifact_path)
            if item.get("bytes") != actual_size:
                raise SystemExit(f"{name}: manifest size mismatch")
            if item.get("sha256") != actual_hash:
                raise SystemExit(f"{name}: manifest SHA-256 mismatch")
            if checksums[name] != actual_hash:
                raise SystemExit(f"{name}: per-platform checksum mismatch")

            public_assets.append(
                {
                    **item,
                    "label": expected_contract["label"],
                    "platform": spec["platform"],
                    "architecture": spec["architecture"],
                    "declared_minimum": document["declared_minimum"],
                    "artifact_kind": document.get("artifact_kind"),
                    "reproducibility_claim": document["reproducibility_claim"],
                    "source_manifest": spec["file"],
                }
            )

        manifests[spec["key"]] = document

    expected_application_names = set(application_contract)
    if seen_names != expected_application_names:
        raise SystemExit(
            f"public application artifact set mismatch: expected {sorted(expected_application_names)}, "
            f"got {sorted(seen_names)}"
        )

    public_assets.sort(key=lambda item: item["name"])
    build_info_name = f"Sudokura-{args.version}-Build-Info.json"
    build_info_path = root / build_info_name
    checksum_path = root / "SHA256SUMS.txt"

    document = {
        "schema": 1,
        "product": PRODUCT,
        "version": args.version,
        "source_commit": args.source_commit,
        "git_tag": f"v{args.version}",
        "release_title": f"Sudokura {args.version}",
        "release_asset_contract": release_contract,
        "artifact_kinds": sorted(kind for kind in artifact_kinds if kind),
        "public_application_assets": public_assets,
        "platform_manifests": manifests,
        "validation": {
            "application_asset_count": len(public_assets),
            "release_asset_count": len(release_contract),
            "manifests_consistent": True,
            "source_identity_consistent": True,
            "asset_names_unique": True,
            "public_windows_zip": False,
            "build_info_self_hash_omitted": True,
        },
    }
    build_info_path.write_text(
        json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )

    checksum_targets = [root / entry["name"] for entry in release_contract if entry["role"] == "application"]
    checksum_targets.append(build_info_path)
    checksum_lines = [
        f"{sha256(path)}  {path.name}" for path in sorted(checksum_targets, key=lambda path: path.name)
    ]
    checksum_path.write_text("\n".join(checksum_lines) + "\n", encoding="utf-8")

    print(f"validated_release_version={args.version}")
    print(f"validated_source_commit={args.source_commit}")
    print(f"public_application_assets={len(public_assets)}")
    print(f"release_assets={len(release_contract)}")
    print(f"build_info={build_info_path.name}")
    print(f"checksums={checksum_path.name}")


def export(args):
    """Revalidate a combined CI bundle and stage only its public assets."""
    if not VERSION_RE.fullmatch(args.version) or not COMMIT_RE.fullmatch(args.source_commit):
        raise SystemExit("invalid version or source commit")
    source = pathlib.Path(args.artifacts_dir)
    destination = pathlib.Path(args.output_dir)
    if destination.exists():
        raise SystemExit("export destination must not already exist")
    entries = contract(args.version, args.include_android)
    info_name = f"Sudokura-{args.version}-Build-Info.json"
    for name in [entry["name"] for entry in entries]:
        path = source / name
        if path.is_symlink() or not path.is_file():
            raise SystemExit(f"missing or symbolic public asset: {name}")
    original_info = (source / info_name).read_bytes()
    original_checksums = (source / "SHA256SUMS.txt").read_bytes()
    document = json.loads(original_info)
    # Rebuild from the embedded per-platform manifests, rather than trusting
    # the aggregate validation booleans or accepting a different artifact set.
    with tempfile.TemporaryDirectory(prefix="sudokura-release-export-") as temporary:
        checked = pathlib.Path(temporary)
        for entry in entries:
            if entry["role"] == "application":
                shutil.copyfile(source / entry["name"], checked / entry["name"])
        for spec in manifest_specs(args.version, args.include_android):
            manifest = document.get("platform_manifests", {}).get(spec["key"])
            if not isinstance(manifest, dict):
                raise SystemExit(f"missing embedded manifest: {spec['key']}")
            (checked / spec["file"]).write_text(json.dumps(manifest), encoding="utf-8")
            lines = [f"{item['sha256']}  {item['name']}" for item in manifest["artifacts"]]
            (checked / spec["checksums"]).write_text("\n".join(lines) + "\n", encoding="utf-8")
        build(SimpleNamespace(version=args.version, source_commit=args.source_commit,
                              artifacts_dir=str(checked), include_android=args.include_android))
        if (checked / info_name).read_bytes() != original_info:
            raise SystemExit("aggregate Build-Info differs from verified manifests")
        if (checked / "SHA256SUMS.txt").read_bytes() != original_checksums:
            raise SystemExit("aggregate checksums differ from verified assets")
        # No output is created before every validation passes. mkdir also refuses
        # an existing destination created concurrently; existing files stay intact.
        destination.mkdir(parents=True)
        for entry in entries:
            shutil.copyfile(checked / entry["name"], destination / entry["name"])
    print(f"exported_public_assets={len(entries)}")
    print("publication=NOT_RUN")


def labels(args):
    if not VERSION_RE.fullmatch(args.version):
        raise SystemExit(f"invalid version: {args.version}")
    for entry in contract(args.version, args.include_android):
        print(f"{entry['name']}\t{entry['label']}")


def validate_assets(args):
    if not VERSION_RE.fullmatch(args.version):
        raise SystemExit(f"invalid version: {args.version}")
    payload = json.loads(pathlib.Path(args.assets_json).read_text(encoding="utf-8"))
    if isinstance(payload, dict) and "assets" in payload:
        payload = payload["assets"]
    if not isinstance(payload, list):
        raise SystemExit("release assets JSON must be a list")

    expected = {entry["name"]: entry for entry in contract(args.version, args.include_android)}
    actual = {}
    for asset in payload:
        name = asset.get("name")
        if name in actual:
            raise SystemExit(f"duplicate GitHub release asset: {name}")
        actual[name] = asset

    if set(actual) != set(expected):
        raise SystemExit(
            f"GitHub release asset set mismatch: expected {sorted(expected)}, got {sorted(actual)}"
        )

    tag_fragment = f"/releases/download/v{args.version}/"
    for name, spec in expected.items():
        asset = actual[name]
        if asset.get("label") != spec["label"]:
            raise SystemExit(
                f"{name}: label mismatch: expected {spec['label']!r}, got {asset.get('label')!r}"
            )
        if asset.get("state") != "uploaded":
            raise SystemExit(f"{name}: release asset not uploaded")
        url = asset.get("browser_download_url") or ""
        if "/releases/download/" not in url or not url.endswith("/" + name):
            raise SystemExit(f"{name}: unexpected browser_download_url: {url!r}")
        if args.published and tag_fragment not in url:
            raise SystemExit(
                f"{name}: published release URL does not use v{args.version}: {url!r}"
            )

    print(f"validated_github_release_assets={len(expected)}")
    print(f"validated_release_tag=v{args.version}")
    print(f"validated_release_urls={'published' if args.published else 'draft-compatible'}")


def parser():
    root = argparse.ArgumentParser()
    sub = root.add_subparsers(dest="command", required=True)

    build_parser = sub.add_parser("build")
    build_parser.add_argument("--artifacts-dir", required=True)
    build_parser.add_argument("--version", required=True)
    build_parser.add_argument("--include-android", action="store_true", help="Include the verified signed Android APK; no publication implied.")
    build_parser.add_argument("--source-commit", required=True)
    build_parser.set_defaults(func=build)

    export_parser = sub.add_parser("export")
    export_parser.add_argument("--artifacts-dir", required=True)
    export_parser.add_argument("--output-dir", required=True)
    export_parser.add_argument("--version", required=True)
    export_parser.add_argument("--source-commit", required=True)
    export_parser.add_argument("--include-android", action="store_true")
    export_parser.set_defaults(func=export)

    label_parser = sub.add_parser("labels")
    label_parser.add_argument("--version", required=True)
    label_parser.add_argument("--include-android", action="store_true", help="Include the verified signed Android APK; no publication implied.")
    label_parser.set_defaults(func=labels)

    validate_parser = sub.add_parser("validate-assets")
    validate_parser.add_argument("--version", required=True)
    validate_parser.add_argument("--include-android", action="store_true", help="Include the verified signed Android APK; no publication implied.")
    validate_parser.add_argument("--assets-json", required=True)
    validate_parser.add_argument(
        "--published",
        action="store_true",
        help="Require versioned /releases/download/v<version>/ URLs after publication.",
    )
    validate_parser.set_defaults(func=validate_assets)

    return root


def main():
    args = parser().parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
