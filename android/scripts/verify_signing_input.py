#!/usr/bin/env python3
"""Verify that an unsigned APK input belongs to the exact source and run."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from audit_apk import expected_identity

SHA_RE = re.compile(r'[0-9a-f]{40}')


def verify(root, source, version):
    if not SHA_RE.fullmatch(source):
        raise ValueError('Invalid source SHA')
    identity = expected_identity((Path(__file__).resolve().parents[2] /
                                  'src/version.h').read_text(encoding='utf-8'), 'release')
    if identity['version_name'] != version:
        raise ValueError('Candidate version differs from checked-out source')
    apk_name = f'Sudokura-{version}-Android-arm64-unsigned.apk'
    aab_name = f'Sudokura-{version}-Android-unsigned.aab'
    files = {f'android-production-output/{name}': root / name
             for name in (apk_name, aab_name)}
    sums = (root / 'SHA256SUMS.txt').read_text(encoding='utf-8').splitlines()
    actual = {}
    for line in sums:
        match = re.fullmatch(r'([0-9a-f]{64})  (android-production-output/[^\s]+)', line)
        if match is None or match.group(2) in actual:
            raise ValueError('Malformed or duplicated checksum entry')
        actual[match.group(2)] = match.group(1)
    if set(actual) != set(files):
        raise ValueError('Unsigned APK/AAB inventory mismatch')
    for path, file in files.items():
        if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != actual[path]:
            raise ValueError('Unsigned artifact hash mismatch: ' + path)
    info = {}
    for line in (root / 'build-info.txt').read_text(encoding='utf-8').splitlines():
        if '=' not in line:
            raise ValueError('Invalid build-info record')
        key, value = line.split('=', 1)
        if key in info:
            raise ValueError('Duplicate build-info field')
        info[key] = value
    if (info.get('source_sha') != source or info.get('version') != version
            or info.get('signing') != 'NOT_RUN'):
        raise ValueError('Source, version or signing receipt mismatch')
    audit = json.loads((root / 'apk-audit.json').read_text(encoding='utf-8'))
    required = {**identity, 'bytes': files[f'android-production-output/{apk_name}'].stat().st_size,
                'android_backup': False, 'permissions': [], 'unsigned_preparation': True}
    for name, value in required.items():
        if audit.get(name) != value:
            raise ValueError('Unsigned APK audit mismatch: ' + name)
    bundle = json.loads((root / 'bundle-readiness.json').read_text(encoding='utf-8'))
    if (bundle.get('container') != 'PASS' or bundle.get('signed') is not False
            or bundle.get('bundletool_validation') != 'PASS'
            or bundle.get('play_upload') != 'NOT_RUN'):
        raise ValueError('Unsigned AAB validation was not accepted')
    if 'ELF 16 KiB structural audit PASS' not in (
            root / 'elf-audit.txt').read_text(encoding='utf-8'):
        raise ValueError('Native library alignment receipt missing')
    if 'Verification succesful' not in (
            root / 'zipalign.txt').read_text(encoding='utf-8'):
        raise ValueError('Unsigned ZIP alignment receipt missing')
    return files[f'android-production-output/{apk_name}']


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--source-commit', required=True)
    parser.add_argument('--version', required=True)
    args = parser.parse_args()
    print(verify(args.directory, args.source_commit, args.version))
