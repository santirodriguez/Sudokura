#!/usr/bin/env python3
"""Validate trusted source/ref context before exposing signing credentials."""
import os
import re
import subprocess


def signing_allowed(event, ref, source, event_commit, version, tag_commit=None, on_main=False):
    if not re.fullmatch(r'[0-9a-f]{40}', source) or source != event_commit:
        return False
    if event == 'pull_request':
        return False
    if ref == 'refs/heads/release/1.4.0':
        return event in ('push', 'workflow_dispatch') and version == '1.4.0'
    # A prepublication 1.4.1 candidate can be signed only by manual dispatch.
    # A merge/push or PR must never unlock the production signing key.
    if ref == 'refs/heads/release/1.4.1':
        return event == 'workflow_dispatch' and version == '1.4.1'
    return (event == 'push' and re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', version)
            and ref == 'refs/tags/v' + version and tag_commit == source and on_main)


def git(*args):
    return subprocess.check_output(['git', *args], text=True).strip()


def main():
    source = git('rev-parse', 'HEAD')
    event = os.environ['GITHUB_EVENT_NAME']
    ref = os.environ['GITHUB_REF']
    version = subprocess.check_output(['sh', 'scripts/version.sh'], text=True).strip()
    event_sha = os.environ['GITHUB_SHA']
    if not re.fullmatch(r'[0-9a-f]{40}', event_sha):
        raise SystemExit('Invalid event SHA')
    event_commit = git('rev-parse', '--verify', event_sha + '^{commit}')
    tag_commit, on_main = None, False
    if event == 'push' and ref == 'refs/tags/v' + version:
        tag_commit = git('rev-parse', '--verify', ref + '^{commit}')
        on_main = subprocess.run(['git', 'merge-base', '--is-ancestor', source,
                                  'refs/remotes/origin/main']).returncode == 0
    allowed = bool(signing_allowed(event, ref, source, event_commit, version, tag_commit, on_main))
    if os.environ.get('REQUIRE_SIGNED') == 'true' and not allowed:
        raise SystemExit('Signed packaging requires the exact trusted release branch or a version-matched tag on main')
    with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as output:
        output.write('sign=' + str(allowed).lower() + '\n')
        output.write('source=' + source + '\n')


if __name__ == '__main__':
    main()
