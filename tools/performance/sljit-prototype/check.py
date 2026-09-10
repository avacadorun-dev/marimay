#!/usr/bin/env python3
"""Replay the frozen 32 source-run commands for correctness, without timing claims."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mmay', type=Path, required=True)
    parser.add_argument('--benchmarks', type=Path, required=True)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    reference = json.loads(args.reference.read_text())
    if reference['status'] != 'passed' or len(reference['workloads']) != 32:
        raise ValueError('a passing complete 32-workload reference is required')
    bench, compiler = args.benchmarks.resolve(), args.mmay.resolve()
    hashes = reference['metadata']['source_sha256']
    def verify():
        for name, digest in hashes.items():
            if sha256(bench / name) != digest:
                raise ValueError(f'reference input changed: {name}')
    verify()
    result = {'schema': 'marimay.vm-correctness-replay/1', 'status': 'running',
              'note': 'Correctness replay only; no performance measurements.',
              'reference_sha256': sha256(args.reference), 'source_sha256': hashes,
              'compiler_sha256': sha256(compiler), 'workloads': {}}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    for name, row in reference['workloads'].items():
        command = list(row['commands']['marimay-vm'])
        source = Path('marimay/benchmarks') / name.replace('-', '_') if row.get('anchor') else Path('marimay/catalog')
        if command[1] != 'run' or not Path(command[2]).as_posix().endswith('/' + source.as_posix()):
            raise ValueError(f'unexpected reference command: {name}')
        command[0], command[2] = str(compiler), str(bench / source)
        process = subprocess.run(command, cwd=bench, capture_output=True, text=True, timeout=180)
        passed = process.returncode == 0 and process.stdout == row['expected_stdout'] and not process.stderr
        result['workloads'][name] = {'command': command, 'exit_code': process.returncode,
                                    'stdout': process.stdout, 'stderr': process.stderr, 'passed': passed}
        args.output.write_text(json.dumps(result, indent=2) + '\n')
        print(name, 'passed' if passed else 'FAILED', flush=True)
        if not passed:
            result['status'] = 'failed'
            args.output.write_text(json.dumps(result, indent=2) + '\n')
            raise SystemExit(1)
    verify()
    if sha256(compiler) != result['compiler_sha256']:
        raise ValueError('compiler changed during the replay')
    result['status'] = 'passed'
    args.output.write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
