#!/usr/bin/env python3
"""Fail-closed trust gate for one manually dispatched internal Android candidate."""
import os
from pathlib import Path
import re
import subprocess

CANDIDATE_SHA = '810888803430d0664c6d02e1b49f67b5e590141d'
CONTROL_REF = 'refs/heads/probe/sudokura-1.4.1-signing-prep'
APPLICATION_ID = 'org.cajapersonal.sudokura'


def gate_allowed(event, ref, event_sha, checkout_sha, run_attempt,
                 source_sha, version, code, application_id, ancestor):
    return (event == 'workflow_dispatch' and ref == CONTROL_REF
            and re.fullmatch(r'[0-9a-f]{40}', event_sha) is not None
            and event_sha == checkout_sha and run_attempt == '1'
            and source_sha == CANDIDATE_SHA and ancestor
            and version == '1.4.1' and code == 2
            and application_id == APPLICATION_ID)


def git(*args):
    return subprocess.check_output(['git', *args], text=True).strip()


def current_identity():
    header = Path('src/version.h').read_text(encoding='utf-8')
    code = re.findall(r'^#define SUDOKURA_VERSION_ANDROID_CODE\s+(\d+)\s*$',
                      header, re.MULTILINE)
    if len(code) != 1:
        raise ValueError('Ambiguous Android version code')
    gradle = Path('android/app/build.gradle').read_text(encoding='utf-8')
    defaults = gradle.split('defaultConfig {', 1)
    if len(defaults) != 2:
        raise ValueError('Missing default application identity')
    ids = re.findall(r"^\s*applicationId\s+'([^']+)'\s*$",
                     defaults[1].split('flavorDimensions', 1)[0], re.MULTILINE)
    if len(ids) != 1:
        raise ValueError('Ambiguous default application identity')
    version = subprocess.check_output(['sh', 'scripts/version.sh'], text=True).strip()
    return version, int(code[0]), ids[0]


def main():
    checkout_sha = git('rev-parse', 'HEAD')
    source_sha = git('rev-parse', '--verify', CANDIDATE_SHA + '^{commit}')
    ancestor = subprocess.run(['git', 'merge-base', '--is-ancestor',
                               source_sha, checkout_sha], check=False).returncode == 0
    version, code, application_id = current_identity()
    if not gate_allowed(os.environ.get('GITHUB_EVENT_NAME', ''),
                        os.environ.get('GITHUB_REF', ''),
                        os.environ.get('GITHUB_SHA', ''), checkout_sha,
                        os.environ.get('GITHUB_RUN_ATTEMPT', ''),
                        source_sha, version, code, application_id, ancestor):
        raise SystemExit('Candidate signing denied: source/ref/event/identity mismatch')
    with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as output:
        output.write('approved=true\nsource=' + CANDIDATE_SHA + '\n')
    print('Approved exact-source internal signing preview; no signing performed by this gate')


if __name__ == '__main__':
    main()
