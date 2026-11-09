#!/usr/bin/env python3
"""Keep original signing credentials out of ordinary CI and build jobs."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
ci = (root / '.github/workflows/ci.yml').read_text()
preview = (root / '.github/workflows/android-preview.yml').read_text()
release = (root / '.github/workflows/release.yml').read_text()
assert 'SUDOKURA_APP_KEYSTORE_' not in ci
assert 'INCLUDE_ANDROID: "false"' in ci
assert 'SUDOKURA_APP_KEYSTORE_' not in release
assert 'approved_source_sha:' in release
assert 'environment: sudokura-app-signing' in preview
assert 'needs: android-debug' in preview
assert 'name: sudokura-android-unsigned-production' in preview
assert 'name: sudokura-android-signed-github' in preview
assert 'Verify approved source and downloaded unsigned inputs' in preview
assert 'verify_signing_input.py' in preview
assert 'SUDOKURA_APPROVED_SOURCE_SHA:' in preview
build, signed = preview.split('\n  android-sign:\n', 1)
assert 'SUDOKURA_APP_KEYSTORE_' not in build
secret_bindings = [line.strip().split(':', 1)[0] for line in signed.splitlines()
                   if line.strip().startswith('SUDOKURA_APP_KEYSTORE_') and 'secrets.' in line]
assert sorted(secret_bindings) == ['SUDOKURA_APP_KEYSTORE_BASE64', 'SUDOKURA_APP_KEYSTORE_PASSWORD']
assert 'SUDOKURA_APP_KEYSTORE_BASE64: ${{ secrets.SUDOKURA_SIGNING_ENV_KEYSTORE_BASE64 }}' in signed
assert 'SUDOKURA_APP_KEYSTORE_PASSWORD: ${{ secrets.SUDOKURA_SIGNING_ENV_KEYSTORE_PASSWORD }}' in signed
assert 'secrets.SUDOKURA_APP_KEYSTORE_BASE64' not in signed
assert 'secrets.SUDOKURA_APP_KEYSTORE_PASSWORD' not in signed
assert "if: needs.android-debug.outputs.sign == 'true'" in signed
assert 'REQUIRE_SIGNED: "true"' in signed
assert 'sudokura-android-unsigned-production' in signed
assert 'ref: $' + '{{ needs.android-debug.outputs.source }}' in signed
print('PASS: ordinary CI is keyless, protected signing is source-bound')
