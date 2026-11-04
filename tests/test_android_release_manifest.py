#!/usr/bin/env python3
"""Exercise signed receipt binding and the optional fifth application asset."""
import contextlib
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


android = load('android_release', ROOT / 'android/scripts/release_manifest.py')
bundle = load('release_bundle', ROOT / 'scripts/release_bundle.py')
VERSION = '1.4.0'
SOURCE = 'a' * 40
APK = 'Sudokura-1.4.0-Android-arm64.apk'


def fixture(root):
    data = b'fixture APK: receipt binding is not cryptographic signature verification'
    (root / APK).write_bytes(data)
    digest = hashlib.sha256(data).hexdigest()
    (root / 'signed-SHA256SUMS.txt').write_text(f'{digest}  android-production-output/{APK}\n')
    (root / 'apk-signature.txt').write_text(
        'Verifies\nVerified using v3 scheme (APK Signature Scheme v3): true\n'
        f'Number of signers: 1\nSigner #1 certificate SHA-256 digest: {android.APP_CERT}\n')
    (root / 'signed-zipalign.txt').write_text('Verification succesful\n')
    audit = {'application_id': android.APP_ID, 'version_name': VERSION,
             'version_code': 10400, 'build_variant': 'release',
             'android_backup': False, 'permissions': [], 'bytes': len(data),
             'unsigned_preparation': False}
    (root / 'signed-apk-audit.json').write_text(json.dumps(audit))
    return audit


def rejected(call, label):
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            call()
    except (ValueError, SystemExit):
        print('PASS reject ' + label)
    else:
        raise AssertionError(label)


with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    audit = fixture(root)
    document = android.prepare(root, VERSION, SOURCE)
    assert document['source_commit'] == SOURCE
    assert document['runtime_16k'] == 'NOT_RUN'
    assert document['store_availability'] == 'NOT_PUBLISHED'
    assert document['reproducibility_claim'].startswith('Not established')
    print('PASS signed receipt binding without device/store/reproducibility claims')
    for key, value in [('application_id', 'org.sudokura.prototype.debug'),
                       ('version_code', 10309), ('android_backup', True),
                       ('build_variant', 'debug'), ('permissions', ['android.permission.INTERNET'])]:
        bad = copy.deepcopy(audit)
        bad[key] = value
        (root / 'signed-apk-audit.json').write_text(json.dumps(bad))
        rejected(lambda: android.prepare(root, VERSION, SOURCE), 'wrong ' + key)
    fixture(root)
    signature = root / 'apk-signature.txt'
    signature.write_text(signature.read_text().replace(android.APP_CERT, 'b' * 64))
    rejected(lambda: android.prepare(root, VERSION, SOURCE), 'wrong app certificate')
    fixture(root)
    (root / APK).write_bytes(b'changed APK')
    rejected(lambda: android.prepare(root, VERSION, SOURCE), 'changed APK after checksum')
    fixture(root)
    rejected(lambda: android.prepare(root, VERSION, 'unknown'), 'unknown source commit')
    android.prepare(root, VERSION, SOURCE)

    for spec in bundle.manifest_specs(VERSION):
        artifacts = []
        checksums = []
        for name in sorted(spec['expected']):
            (root / name).write_bytes(name.encode())
            digest = bundle.sha256(root / name)
            artifacts.append({'name': name, 'bytes': len(name.encode()), 'sha256': digest})
            checksums.append(f'{digest}  {name}\n')
        (root / spec['checksums']).write_text(''.join(checksums))
        (root / spec['file']).write_text(json.dumps({
            'product': 'Sudokura', 'version': VERSION, 'source_commit': SOURCE,
            'platform': spec['platform'], 'architecture': spec['architecture'],
            'declared_minimum': 'fixture', 'reproducibility_claim': 'Not established',
            'artifact_kind': 'fixture', 'artifacts': artifacts,
        }))
    args = SimpleNamespace(version=VERSION, source_commit=SOURCE,
                           artifacts_dir=str(root), include_android=True)
    with contextlib.redirect_stdout(io.StringIO()):
        bundle.build(args)
    info = json.loads((root / f'Sudokura-{VERSION}-Build-Info.json').read_text())
    assert info['validation']['application_asset_count'] == 5
    assert info['validation']['release_asset_count'] == 7
    assert APK in bundle.parse_checksums(root / 'SHA256SUMS.txt')
    assert len(bundle.contract(VERSION)) == 6
    print('PASS Android included in shared checksums and Build-Info; desktop contract preserved')

    export_args = SimpleNamespace(**vars(args), output_dir=str(root / 'public'))
    with contextlib.redirect_stdout(io.StringIO()):
        bundle.export(export_args)
    assert {p.name for p in (root / 'public').iterdir()} == {e['name'] for e in bundle.contract(VERSION, True)}
    rejected(lambda: bundle.export(export_args), 'existing export destination')
    export_args.output_dir = str(root / 'rejected')
    export_args.source_commit = 'b' * 40
    rejected(lambda: bundle.export(export_args), 'export source mismatch')
    assert not (root / 'rejected').exists()
    export_args.source_commit = SOURCE
    saved = (root / APK).read_bytes()
    (root / APK).write_bytes(b'tampered')
    rejected(lambda: bundle.export(export_args), 'export modified APK')
    assert not (root / 'rejected').exists()
    (root / APK).write_bytes(saved)
    original_info = (root / f'Sudokura-{VERSION}-Build-Info.json').read_bytes()
    altered = json.loads(original_info)
    altered['validation']['release_asset_count'] = 6
    (root / f'Sudokura-{VERSION}-Build-Info.json').write_text(json.dumps(altered))
    rejected(lambda: bundle.export(export_args), 'export modified aggregate metadata')
    (root / f'Sudokura-{VERSION}-Build-Info.json').write_bytes(original_info)
    print('PASS exact seven-file export without publishing or overwriting')

    manifest = root / 'artifact-manifest-android.json'
    valid = json.loads(manifest.read_text())
    for key, value in [('source_commit', 'b' * 40), ('signing_certificate_sha256', 'b' * 64)]:
        changed = copy.deepcopy(valid)
        changed[key] = value
        manifest.write_text(json.dumps(changed))
        rejected(lambda: bundle.build(args), 'bundle ' + key + ' mismatch')
    manifest.write_text(json.dumps(valid))
    (root / APK).unlink()
    rejected(lambda: bundle.build(args), 'missing signed APK')
