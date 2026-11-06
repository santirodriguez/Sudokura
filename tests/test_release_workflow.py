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
debug = ci['jobs']['android-ui-debug']
assert "github.event_name == 'push'" in debug['if']
assert "refs/heads/feature/unified-ui" in debug['if']
assert "refs/heads/release-prep/1.4.0-unified-ui" in debug['if']
assert debug['uses'] == './.github/workflows/android-preview.yml'
assert debug['with']['source_commit'] == '${{ github.sha }}'
assert 'secrets' not in debug
ubuntu = ci['jobs']['linux-ubuntu24']
assert ubuntu['timeout-minutes'] == '15'
install = next(s for s in ubuntu['steps']
               if s.get('name') == 'Install Ubuntu runtime validation dependencies')
assert 'timeout --signal=TERM --kill-after=10s 180s' in install['run']
assert 'timeout --signal=TERM --kill-after=10s 480s' in install['run']
assert 'install -y --no-install-recommends' in install['run']
assert 'binutils libfuse2t64 xvfb weston dbus-x11 gdb strace' in install['run']
assert 'validate-linux-runtime.sh' in ubuntu['steps'][-2]['run']
release_ubuntu = release['jobs']['linux-ubuntu24']
assert release_ubuntu['timeout-minutes'] == '15'
release_install = next(s for s in release_ubuntu['steps']
                       if s.get('name') == 'Install Ubuntu runtime validation dependencies')
assert release_install['run'] == install['run']
assert 'validate-linux-runtime.sh' in release_ubuntu['steps'][-2]['run']
assert 'ubuntu24 required' in release_ubuntu['steps'][-2]['run']
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
