#!/usr/bin/env python3
"""Permit original-key Android signing only for an exact, manually approved source."""
import os
import re
import subprocess

SHA_RE = re.compile(r'[0-9a-f]{40}')


def signing_allowed(event, ref, source, event_commit, version, approved_source=''):
    # A full source SHA binds one explicitly dispatched release-branch run.
    # Repository secrets remain usable by other privileged workflows; this
    # check is not a GitHub secret access boundary or an independent approval.
    return (
        event == 'workflow_dispatch'
        and ref == 'refs/heads/release/1.4.1'
        and version == '1.4.1'
        and isinstance(source, str)
        and SHA_RE.fullmatch(source) is not None
        and source == event_commit
        and isinstance(approved_source, str)
        and SHA_RE.fullmatch(approved_source) is not None
        and approved_source == source
    )


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
    approved_source = os.environ.get('SUDOKURA_APPROVED_SOURCE_SHA', '')
    allowed = signing_allowed(event, ref, source, event_commit, version, approved_source)
    required = os.environ.get('REQUIRE_SIGNED') == 'true'
    if not allowed and (required or approved_source):
        raise SystemExit('Signing denied: manual event, trusted branch, version or source SHA mismatch')
    with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as output:
        output.write('sign=' + str(allowed).lower() + '\n')
        output.write('source=' + source + '\n')


if __name__ == '__main__':
    main()
