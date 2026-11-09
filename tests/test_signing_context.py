#!/usr/bin/env python3
"""Test permitted and denied signing contexts without a production key."""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    'signing', Path(__file__).resolve().parents[1] / 'android/scripts/signing_context.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
allowed = module.signing_allowed
a, b = 'a' * 40, 'b' * 40
positive = [
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a, a, '1.4.1', None, False, a),
    ('push', 'refs/tags/v1.4.0', a, a, '1.4.0', a, True, ''),
    ('push', 'refs/tags/v1.4.1', a, a, '1.4.1', a, True, ''),
]
negative = [
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a, a, '1.4.1', None, False, ''),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a, a, '1.4.1', None, False, b),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a, a, '1.4.1', None, False, 'invalid'),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a, b, '1.4.1', None, False, a),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', b, a, '1.4.1', None, False, b),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a, a, '1.4.0', None, False, a),
    ('workflow_dispatch', 'refs/heads/release/1.4.0', a, a, '1.4.0', None, False, a),
    ('workflow_dispatch', 'refs/heads/main', a, a, '1.4.1', None, False, a),
    ('workflow_dispatch', 'refs/heads/feature/fake-release', a, a, '1.4.1', None, False, a),
    ('workflow_dispatch', 'refs/tags/v1.4.1', a, a, '1.4.1', a, True, a),
    ('pull_request', 'refs/heads/release/1.4.1', a, a, '1.4.1', None, False, a),
    ('push', 'refs/heads/release/1.4.1', a, a, '1.4.1', None, False, a),
    ('push', 'refs/heads/release/1.4.0', a, a, '1.4.0', None, False, a),
    ('push', 'refs/heads/main', a, a, '1.4.1', None, True, a),
    ('push', 'refs/tags/v1.4.1', a, a, '1.4.1', b, True, ''),
    ('push', 'refs/tags/v1.4.1', a, a, '1.4.1', a, False, ''),
    ('push', 'refs/tags/v1.4.1', a, b, '1.4.1', a, True, ''),
    ('push', 'refs/tags/v1.4.1', a, a, '1.4.0', a, True, ''),
    ('push', 'refs/tags/v1.4.1-preview', a, a, '1.4.1', a, True, ''),
    ('push', 'refs/tags/v1.4.1', 'invalid', 'invalid', '1.4.1', a, True, ''),
]
for args in positive:
    assert allowed(*args), args
for args in negative:
    assert not allowed(*args), args
print(f'PASS: {len(positive)} permitted contexts and {len(negative)} denied contexts')
