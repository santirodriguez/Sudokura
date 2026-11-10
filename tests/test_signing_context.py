#!/usr/bin/env python3
"""Validate manual-only source-bound signing without any production secrets."""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    'signing', Path(__file__).resolve().parents[1] / 'android/scripts/signing_context.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
allowed = module.signing_allowed
a, b = 'a' * 40, 'b' * 40

positive = [
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a, a, '1.4.1', a),
]
negative = [
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a, a, '1.4.1', ''),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a, a, '1.4.1', b),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a, a, '1.4.1', 'not-a-sha'),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a, b, '1.4.1', a),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', b, a, '1.4.1', b),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a, a, '1.4.0', a),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a, a, '1.4.2', a),
    ('workflow_dispatch', 'refs/heads/release/1.4.0', a, a, '1.4.1', a),
    ('workflow_dispatch', 'refs/heads/main', a, a, '1.4.1', a),
    ('workflow_dispatch', 'refs/heads/security/v1.4.1-protected-signing', a, a, '1.4.1', a),
    ('workflow_dispatch', 'refs/tags/v1.4.1', a, a, '1.4.1', a),
    ('pull_request', 'refs/heads/release/1.4.1', a, a, '1.4.1', a),
    ('push', 'refs/heads/release/1.4.1', a, a, '1.4.1', a),
    ('push', 'refs/heads/main', a, a, '1.4.1', a),
    ('push', 'refs/tags/v1.4.1', a, a, '1.4.1', a),
    ('pull_request_target', 'refs/heads/release/1.4.1', a, a, '1.4.1', a),
    ('workflow_run', 'refs/heads/release/1.4.1', a, a, '1.4.1', a),
    ('schedule', 'refs/heads/release/1.4.1', a, a, '1.4.1', a),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', 'bad', 'bad', '1.4.1', 'bad'),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a.upper(), a.upper(), '1.4.1', a.upper()),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a + '0', a + '0', '1.4.1', a + '0'),
    ('workflow_dispatch', 'refs/heads/release/1.4.1', a, a, '1.4.1', a.upper()),
]
for args in positive:
    assert allowed(*args), args
for args in negative:
    assert not allowed(*args), args
print(f'PASS: {len(positive)} exact manual context; {len(negative)} denied contexts')
