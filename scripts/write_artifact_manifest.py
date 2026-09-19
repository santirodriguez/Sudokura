#!/usr/bin/env python3
import argparse
import hashlib
import json
import pathlib
import subprocess

def command(*args):
    return subprocess.check_output(args, text=True).strip()

def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()

parser = argparse.ArgumentParser()
parser.add_argument('--platform', required=True)
parser.add_argument('--architecture', required=True)
parser.add_argument('--minimum', required=True)
parser.add_argument('--kind', required=True)
parser.add_argument('--output', required=True)
parser.add_argument('--artifact', action='append', required=True)
parser.add_argument('--baseline', action='append', type=int, required=True)
args = parser.parse_args()
if len(args.artifact) != len(args.baseline):
    parser.error('--artifact and --baseline counts must match')

version = command('./scripts/version.sh')
commit = command('git', 'rev-parse', 'HEAD')
artifacts = []
for artifact_name, baseline in zip(args.artifact, args.baseline):
    path = pathlib.Path(artifact_name)
    if not path.is_file():
        raise SystemExit(f'missing artifact: {path}')
    size = path.stat().st_size
    comparison = {'baseline_version': '1.2.0'}
    if baseline > 0:
        comparison.update({
            'baseline_bytes': baseline,
            'delta_bytes': size - baseline,
            'delta_percent': round((size - baseline) * 100.0 / baseline, 3),
            'note': 'Size growth is measured, not optimized by removing approved resources.',
        })
    else:
        comparison.update({
            'baseline_bytes': None,
            'delta_bytes': None,
            'delta_percent': None,
            'note': 'New artifact in v1.3.0; no v1.2 equivalent exists.',
        })
    artifacts.append({
        'name': path.name,
        'bytes': size,
        'sha256': sha256(path),
        'comparison': comparison,
    })

document = {
    'schema': 1,
    'product': 'Sudokura',
    'version': version,
    'source_commit': commit,
    'artifact_kind': args.kind,
    'platform': args.platform,
    'architecture': args.architecture,
    'declared_minimum': args.minimum,
    'reproducibility_claim': 'same scripted inputs and recorded toolchain; not claimed bit-for-bit reproducible',
    'artifacts': artifacts,
}
pathlib.Path(args.output).write_text(json.dumps(document, indent=2, sort_keys=True) + '\n', encoding='utf-8')
print(json.dumps(document, indent=2, sort_keys=True))
