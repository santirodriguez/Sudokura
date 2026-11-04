#!/usr/bin/env python3
"""Bind an internally signed APK to verified receipts and exact source identity."""
import argparse
import hashlib
import json
from pathlib import Path
import re

APP_ID = 'org.cajapersonal.sudokura'
APP_CERT = 'fde951e3d2ef42e41c7186ee965944aeca549d590ad080711dd3d875f82df66d'


def prepare(directory, version, source_commit):
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', version):
        raise ValueError('invalid version')
    if not re.fullmatch(r'[0-9a-f]{40}', source_commit):
        raise ValueError('invalid source commit')
    values = re.findall(r"^#define[ \t]+SUDOKURA_VERSION_ANDROID_CODE[ \t]+([0-9]+)[ \t]*$",
                        (Path(__file__).resolve().parents[2] / 'src/version.h').read_text(), re.M)
    if len(values) != 1 or not 0 < int(values[0]) <= 2100000000:
        raise ValueError('invalid Android version code')
    code = int(values[0])
    name = f'Sudokura-{version}-Android-arm64.apk'
    apk = directory / name
    digest = hashlib.sha256(apk.read_bytes()).hexdigest()
    lines = (directory / 'signed-SHA256SUMS.txt').read_text().splitlines()
    if len(lines) != 1:
        raise ValueError('expected one signed APK checksum')
    checksum = lines[0].split()
    if checksum != [digest, f'android-production-output/{name}']:
        raise ValueError('signed APK checksum mismatch')
    signature = (directory / 'apk-signature.txt').read_text()
    certificates = re.findall(r'^Signer #[0-9]+ certificate SHA-256 digest: ([0-9a-f]+)$', signature, re.M)
    if (not signature.startswith('Verifies\n') or certificates != [APP_CERT]
            or 'Number of signers: 1\n' not in signature
            or 'Verified using v3 scheme (APK Signature Scheme v3): true\n' not in signature):
        raise ValueError('signature receipt does not verify the Sudokura app signer')
    if 'Verification succesful' not in (directory / 'signed-zipalign.txt').read_text():
        raise ValueError('alignment receipt did not pass')
    audit = json.loads((directory / 'signed-apk-audit.json').read_text())
    expected = {'application_id': APP_ID, 'version_name': version,
                'version_code': code, 'build_variant': 'release',
                'android_backup': False, 'permissions': [], 'bytes': apk.stat().st_size,
                'unsigned_preparation': False}
    for key, value in expected.items():
        if key not in audit or audit[key] != value:
            raise ValueError(f'APK audit mismatch: {key}')
    document = {
        'schema': 1, 'product': 'Sudokura', 'version': version,
        'source_commit': source_commit, 'platform': 'android', 'architecture': 'arm64',
        'declared_minimum': 'Android 9 (API 28), arm64-v8a',
        'artifact_kind': 'internal-signed-candidate',
        'reproducibility_claim': 'Not established; independent rebuild required',
        'application_id': APP_ID, 'version_code': code,
        'signing_certificate_sha256': APP_CERT, 'signing_receipt_verified': True,
        'device_acceptance': 'Separate exact-artifact evidence required',
        'runtime_16k': 'NOT_RUN', 'store_availability': 'NOT_PUBLISHED',
        'artifacts': [{'name': name, 'bytes': apk.stat().st_size, 'sha256': digest}],
    }
    (directory / 'artifact-manifest-android.json').write_text(json.dumps(document, indent=2) + '\n')
    (directory / 'SHA256SUMS-android.txt').write_text(f'{digest}  {name}\n')
    (directory / 'build-provenance-android.txt').write_text(
        f'source_commit={source_commit}\napplication_id={APP_ID}\nversion={version}\n'
        f'version_code={code}\nsigning_certificate_sha256={APP_CERT}\n'
        'artifact_kind=internal-signed-candidate\npublicability=NOT_ACCEPTED\n')
    return document


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--source-commit', required=True)
    args = parser.parse_args()
    prepare(args.directory, args.version, args.source_commit)
