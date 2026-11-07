#!/usr/bin/env python3
"""Reject non-Android hosts and progress-bearing or non-disposable devices."""
import importlib.util
from pathlib import Path
path = Path(__file__).resolve().parents[1] / 'android/scripts/probe_upgrade_device.py'
spec = importlib.util.spec_from_file_location('device_probe', path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
assert module.eligible('arm64-v8a,armeabi-v7a', '36', '1', False, 'arm64-v8a', 'aarch64')
assert module.eligible('arm64-v8a', '28', '1', False, 'arm64-v8a', 'aarch64')
for case in [
    ('x86_64,x86', '36', '1', False, 'x86_64', 'x86_64'),
    ('x86_64,arm64-v8a', '36', '1', False, 'x86_64', 'x86_64'),
    ('arm64-v8a', '36', '1', False, 'arm64-v8a', 'x86_64'),
    ('arm64-v8a', '27', '1', False, 'arm64-v8a', 'aarch64'),
    ('arm64-v8a', '36', '0', False, 'arm64-v8a', 'aarch64'),
    ('arm64-v8a', '36', '1', True, 'arm64-v8a', 'aarch64'),
    ('arm64-v8a', 'unknown', '1', False, 'arm64-v8a', 'aarch64'),
]:
    assert not module.eligible(*case), case
print('PASS ARM64 device eligibility: 2 accepted, 7 rejected')
