#!/usr/bin/env python3
"""Keep signing unavailable to PRs, arbitrary refs and unrelated source commits."""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location('signing', Path(__file__).resolve().parents[1] / 'android/scripts/signing_context.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
allowed = module.signing_allowed
a, b = 'a' * 40, 'b' * 40
assert allowed('push', 'refs/heads/release/1.4.0', a, a, '1.4.0')
assert allowed('workflow_dispatch', 'refs/heads/release/1.4.0', a, a, '1.4.0')
assert allowed('push', 'refs/tags/v1.4.0', a, a, '1.4.0', a, True)
assert allowed('workflow_dispatch', 'refs/heads/release/1.4.1', a, a, '1.4.1')
assert allowed('push', 'refs/tags/v1.4.1', a, a, '1.4.1', a, True)
for event, ref, source, event_commit, version, tag, on_main in [
    ('pull_request', 'refs/heads/release/1.4.0', a, a, '1.4.0', None, False),
    ('push', 'refs/heads/main', a, a, '1.4.0', None, True),
    ('push', 'refs/heads/other', a, a, '1.4.0', None, True),
    ('workflow_dispatch', 'refs/tags/v1.4.0', a, a, '1.4.0', a, True),
    ('push', 'refs/tags/v1.4.0', a, a, '1.4.1', a, True),
    ('push', 'refs/tags/v1.4.0', a, b, '1.4.0', a, True),
    ('push', 'refs/tags/v1.4.0', a, a, '1.4.0', b, True),
    ('push', 'refs/tags/v1.4.0', a, a, '1.4.0', a, False),
    ('push', 'refs/heads/release/1.4.0', b, a, '1.4.0', None, False),
    ('push', 'refs/heads/release/1.4.0', a, a, '1.4.1', None, False),
    ('push', 'refs/heads/release/1.4.1', a, a, '1.4.1', None, False),
    ('pull_request', 'refs/heads/release/1.4.1', a, a, '1.4.1', None, False),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a, a, '1.4.0', None, False),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a, b, '1.4.1', None, False),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', 'invalid', 'invalid', '1.4.1', None, False),
    ('workflow_dispatch', 'refs/heads/release/1.4.1-preview', a, a, '1.4.1', None, False),
    ('workflow_dispatch', 'refs/heads/fix/v1.4.1-signing-context-guard', a, a, '1.4.1', None, False),
    ('workflow_dispatch', 'refs/tags/v1.4.1', a, a, '1.4.1', a, True),
    ('push', 'refs/tags/v1.4.1', a, a, '1.4.1', a, False),
]:
    assert not allowed(event, ref, source, event_commit, version, tag, on_main), (event, ref)
print('PASS: five trusted signing contexts and 19 denied contexts')
