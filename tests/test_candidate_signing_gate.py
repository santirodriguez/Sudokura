#!/usr/bin/env python3
"""Negative tests for manual-only exact-source candidate signing trust."""
import importlib.util
from pathlib import Path

path = Path(__file__).resolve().parents[1] / 'android/scripts/candidate_signing_gate.py'
spec = importlib.util.spec_from_file_location('candidate_gate', path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
source = module.CANDIDATE_SHA
control = 'b' * 40
valid = dict(event='workflow_dispatch', ref=module.CONTROL_REF,
             event_sha=control, checkout_sha=control, run_attempt='1',
             source_sha=source, version='1.4.1', code=2,
             application_id='org.cajapersonal.sudokura', ancestor=True)
assert module.gate_allowed(**valid)
mutations = (
    dict(event='push'), dict(event='pull_request'),
    dict(ref='refs/heads/release/1.4.1'), dict(ref='refs/heads/main'),
    dict(event_sha='a' * 40), dict(checkout_sha='a' * 40),
    dict(event_sha='invalid'), dict(run_attempt='2'),
    dict(source_sha='a' * 40), dict(version='1.4.0'),
    dict(code=1), dict(code=3),
    dict(application_id='org.sudokura.prototype.debug'),
    dict(ancestor=False),
)
for change in mutations:
    case = {**valid, **change}
    assert not module.gate_allowed(**case), change
print(f'PASS candidate signing gate: 1 accepted, {len(mutations)} rejected')
