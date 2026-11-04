#!/usr/bin/env python3
"""Check the release dependency/asset contract without creating a tag or draft."""
from pathlib import Path
import yaml
root = Path(__file__).resolve().parents[1]
load = lambda name: yaml.load((root / '.github/workflows' / name).read_text(), Loader=yaml.BaseLoader)
release, android, ci = map(load, ['release.yml', 'android-preview.yml', 'ci.yml'])
assert 'branches' in ci['on']['push'] and 'tags' not in ci['on']['push']
assert "refs/heads/main" in ci['jobs']['android-changes']['if']
android_gate = ci['jobs']['android-changes']['if']
assert "github.event_name == 'pull_request'" in android_gate
assert "github.base_ref == 'main'" in android_gate
assert "github.head_ref == 'release/1.4.0'" in android_gate
assert release['jobs']['android']['with']['require_signed'] == 'true'
assert 'needs.version.outputs.commit' in release['jobs']['android']['with']['source_commit']
assert 'android' in release['jobs']['release-bundle']['needs']
assert 'release-bundle' in release['jobs']['draft-release']['needs']
steps = release['jobs']['release-bundle']['steps']
assert any('--include-android' in s.get('run', '') and ' build ' in s.get('run', '') for s in steps)
draft = release['jobs']['draft-release']
upload = next(s for s in draft['steps'] if s.get('id') == 'create-release')
assert upload['with']['draft'] == 'true'
assert '-Android-arm64.apk' in upload['with']['files']
assert len(upload['with']['files'].strip().splitlines()) == 7
validation = draft['steps'][-1]['run']
assert 'labels --version "$VERSION" --include-android' in validation
assert '--assets-json draft-release-assets.json --include-android' in validation
sign_step = next(s for s in android['jobs']['android-debug']['steps'] if s.get('name', '').startswith('Sign production'))
assert sign_step['if'] == "steps.signing.outputs.sign == 'true'"
assert all('GITHUB_SHA' not in s.get('run', '') for s in android['jobs']['android-debug']['steps'])
print('PASS tag routing, main Android checks, exact-source reusable build, signing guard and seven-file draft contract')
