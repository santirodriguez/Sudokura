#!/usr/bin/env python3
"""Validate manually approved signing stays outside automatic CI."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
ci = (root / '.github/workflows/ci.yml').read_text(encoding='utf-8')
preview = (root / '.github/workflows/android-preview.yml').read_text(encoding='utf-8')
release = (root / '.github/workflows/release.yml').read_text(encoding='utf-8')

# The routine Android worker is keyless, even when called from a PR or a push.
assert 'SUDOKURA_APP_KEYSTORE_' not in ci
assert 'SUDOKURA_APP_KEYSTORE_' not in preview
assert 'SUDOKURA_SIGNING_ENV_' not in preview
assert '  android-sign:' not in preview
assert 'INCLUDE_ANDROID: "false"' in ci
assert "github.base_ref == 'release/1.4.1'" in ci
assert 'name: sudokura-android-unsigned-production' in preview
assert 'name: sudokura-android-signed-github' not in preview
assert 'source: ${{ steps.signing.outputs.source }}' in preview

# Exactly one explicit manual workflow can request the original signing key.
assert '  workflow_dispatch:' in release
assert '  push:' not in release
assert '  draft-release:' not in release
assert 'approved_source_sha:' in release
assert 'required: true' in release
assert 'SUDOKURA_APP_KEYSTORE_' not in release.split('\n  android-sign:\n', 1)[0]
assert 'secrets: inherit' not in release
assert 'environment: sudokura-app-signing' not in release

manual, after = release.split('\n  android-sign:\n', 1)
signed, _ = after.split('\n  linux:\n', 1)
assert 'needs: [version, android]' in signed
assert "github.event_name == 'workflow_dispatch' && github.actor == 'santirodriguez'" in signed
assert 'REQUIRE_SIGNED: "true"' in signed
assert 'SUDOKURA_APPROVED_SOURCE_SHA: ${{ inputs.approved_source_sha }}' in signed
assert 'Verify approved source and downloaded unsigned inputs' in signed
assert 'python3 android/scripts/signing_context.py' in signed
assert 'python3 android/scripts/verify_signing_input.py' in signed
assert 'name: sudokura-android-unsigned-production' in signed
assert 'name: sudokura-android-signed-github' in signed
assert 'SUDOKURA_APP_KEYSTORE_BASE64: ${{ secrets.SUDOKURA_APP_KEYSTORE_BASE64 }}' in signed
assert 'SUDOKURA_APP_KEYSTORE_PASSWORD: ${{ secrets.SUDOKURA_APP_KEYSTORE_PASSWORD }}' in signed
assert signed.count('secrets.SUDOKURA_APP_KEYSTORE_BASE64') == 1
assert signed.count('secrets.SUDOKURA_APP_KEYSTORE_PASSWORD') == 1
assert 'SUDOKURA_SIGNING_ENV_' not in signed
assert 'environment:' not in signed

# The source and actor gate runs before any potentially credential-bearing step.
assert 'test "$GITHUB_ACTOR" = santirodriguez' in manual
assert 'test "$GITHUB_REF" = refs/heads/release/1.4.1' in manual
assert 'test "$GITHUB_SHA" = "$APPROVED_SOURCE_SHA"' in manual
assert 'test "$(sh scripts/version.sh)" = 1.4.1' in manual
assert signed.index('Verify approved source and downloaded unsigned inputs') < signed.index(
    'Sign production APK with the original Sudokura app key')
assert 'needs: [version, linux, linux-ubuntu24, linux-fedora44, windows, macos, android-sign]' in release

print('PASS: automatic builds are keyless; only manual exact-source signer references repo secrets')
