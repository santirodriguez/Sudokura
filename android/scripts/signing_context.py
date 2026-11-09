#!/usr/bin/env python3
"""Fail closed on source/ref/event mismatch before offering protected APK signing."""
import os
import re
import subprocess

SHA_RE = re.compile(r'[0-9a-f]{40}')
VERSION_RE = re.compile(r'[0-9]+\.[0-9]+\.[0-9]+')


def signing_allowed(event, ref, source, event_commit, version, tag_commit=None,
                    on_main=False, approved_source=''):
    if not isinstance(source, str) or not SHA_RE.fullmatch(source):
        return False
    if source != event_commit:
        return False
    if event == 'workflow_dispatch':
        # Matching an operator-provided SHA is necessary, but the Environment
        # must independently authorize use of the actual signing credentials.
        return (ref == 'refs/heads/release/1.4.1' and version == '1.4.1'
                and isinstance(approved_source, str)
                and SHA_RE.fullmatch(approved_source) is not None
                and approved_source == source)
    # Only a version-matched tag whose source belongs to main may sign automatically
    # after the separate protected Environment gate.
    return (event == 'push' and VERSION_RE.fullmatch(version) is not None
            and ref == 'refs/tags/v' + version
            and tag_commit == source and on_main)


def git(*args):
    return subprocess.check_output(['git', *args], text=True).strip()


def main():
    source = git('rev-parse', 'HEAD')
    event = os.environ['GITHUB_EVENT_NAME']
    ref = os.environ['GITHUB_REF']
    version = subprocess.check_output(['sh', 'scripts/version.sh'], text=True).strip()
    event_sha = os.environ['GITHUB_SHA']
    if not SHA_RE.fullmatch(event_sha):
        raise SystemExit('Invalid event SHA')
    event_commit = git('rev-parse', '--verify', event_sha + '^{commit}')
    tag_commit, on_main = None, False
    if event == 'push' and ref == 'refs/tags/v' + version:
        tag_commit = git('rev-parse', '--verify', ref + '^{commit}')
        on_main = subprocess.run(['git', 'merge-base', '--is-ancestor', source,
                                  'refs/remotes/origin/main'], check=False).returncode == 0
    approved_source = os.environ.get('SUDOKURA_APPROVED_SOURCE_SHA', '')
    allowed = signing_allowed(event, ref, source, event_commit, version, tag_commit,
                              on_main, approved_source)
    required = os.environ.get('REQUIRE_SIGNED') == 'true'
    if not allowed and (required or approved_source):
        raise SystemExit('Signing denied: event, trusted ref, version or approved source differs')
    with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as output:
        output.write('sign=' + str(allowed).lower() + '\n')
        output.write('source=' + source + '\n')


if __name__ == '__main__':
    main()
