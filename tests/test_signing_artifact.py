#!/usr/bin/env python3
"""Verify fail-closed source-bound unsigned artifact receipts without APK signing."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'android/scripts'))
spec = importlib.util.spec_from_file_location(
    'verify_input', ROOT / 'android/scripts/verify_signing_input.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
identity = module.expected_identity((ROOT / 'src/version.h').read_text(), 'release')
version = identity['version_name']
source = 'a' * 40
names = [f'Sudokura-{version}-Android-arm64-unsigned.apk',
         f'Sudokura-{version}-Android-unsigned.aab']


def reject(call, label):
    try:
        call()
    except (ValueError, FileNotFoundError):
        print('PASS reject ' + label)
    else:
        raise AssertionError(label)


with tempfile.TemporaryDirectory() as work:
    root = Path(work)
    for name in names:
        (root / name).write_bytes(('test fixture ' + name).encode())

    def reset():
        (root / 'SHA256SUMS.txt').write_text(''.join(
            f"{hashlib.sha256((root / name).read_bytes()).hexdigest()}  android-production-output/{name}\n"
            for name in names))
        (root / 'build-info.txt').write_text(
            f'source_sha={source}\nversion={version}\nsigning=NOT_RUN\nphysical_device=NOT_RUN\n')
        (root / 'apk-audit.json').write_text(json.dumps({
            **identity, 'bytes': (root / names[0]).stat().st_size,
            'android_backup': False, 'permissions': [], 'unsigned_preparation': True
        }))
        (root / 'bundle-readiness.json').write_text(json.dumps({
            'container': 'PASS', 'signed': False, 'bundletool_validation': 'PASS',
            'play_upload': 'NOT_RUN'
        }))
        (root / 'elf-audit.txt').write_text('ELF 16 KiB structural audit PASS\n')
        (root / 'zipalign.txt').write_text('Verification succesful\n')

    reset()
    assert module.verify(root, source, version) == root / names[0]
    print('PASS valid exact-source unsigned input')
    reject(lambda: module.verify(root, 'b' * 40, version), 'wrong source')
    reject(lambda: module.verify(root, source, '1.2.3'), 'wrong version')
    (root / names[0]).write_bytes(b'tampered')
    reject(lambda: module.verify(root, source, version), 'tampered APK')
    (root / names[0]).write_bytes(('test fixture ' + names[0]).encode())
    reset()
    (root / 'build-info.txt').write_text(f'source_sha={source}\nversion={version}\nsigning=PASS\n')
    reject(lambda: module.verify(root, source, version), 'already signed receipt')
    reset()
    data = json.loads((root / 'apk-audit.json').read_text())
    data['application_id'] = 'org.sudokura.prototype.debug'
    (root / 'apk-audit.json').write_text(json.dumps(data))
    reject(lambda: module.verify(root, source, version), 'wrong app identity')
    reset()
    (root / 'SHA256SUMS.txt').write_text((root / 'SHA256SUMS.txt').read_text().splitlines()[0] + '\n')
    reject(lambda: module.verify(root, source, version), 'missing bundle checksum')
    reset()
    (root / 'bundle-readiness.json').write_text('{"container":"PASS","signed":true,"bundletool_validation":"PASS","play_upload":"NOT_RUN"}')
    reject(lambda: module.verify(root, source, version), 'bundle marked signed')
    reset()
    (root / 'elf-audit.txt').write_text('NOT_VERIFIED\n')
    reject(lambda: module.verify(root, source, version), 'missing 16 KiB receipt')
