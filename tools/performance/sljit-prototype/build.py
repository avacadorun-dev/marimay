#!/usr/bin/env python3
"""Rebuild the isolated SLJIT evaluation; never modify production sources."""
from pathlib import Path
import argparse
import hashlib
import json
import shlex
import shutil
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=Path('build/release'))
    parser.add_argument('--sljit', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('build/sljit-prototype'))
    parser.add_argument('--arch', choices=['arm64', 'x86_64'])
    parser.add_argument('--unit', action='store_true', help='Also link the existing bytecode unit suite against this VM')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    root = here.parents[2]
    build, checkout, output = args.build.resolve(), args.sljit.resolve(), args.output.resolve()
    if output == root or output == here or root not in output.parents or root / 'build' not in output.parents:
        raise ValueError('output must be a subdirectory of this repository/build')
    origin = json.loads((here / 'ORIGIN.json').read_text())
    revision = subprocess.check_output(['git', '-C', str(checkout), 'rev-parse', 'HEAD'], text=True).strip()
    dirty = subprocess.check_output(['git', '-C', str(checkout), 'status', '--porcelain'], text=True)
    if revision != origin['sljit_commit'] or dirty:
        raise ValueError('SLJIT must be a clean checkout at the recorded commit')
    for path, expected in origin['baseline_sources_sha256'].items():
        if digest(root / path) != expected:
            raise ValueError(f'baseline source changed: {path}; review the adapter before rebasing')
    for path, expected in origin['files_sha256'].items():
        if digest(here / path) != expected:
            raise ValueError(f'prototype source changed: {path}; update its reviewed provenance')
    output.mkdir(parents=True, exist_ok=True)
    for name in origin['files_sha256']:
        shutil.copy2(here / name, output / name)
    for name in ['vm.c', 'vm_image.c']:
        shutil.copy2(root / 'bootstrap/src/bytecode' / name, output / name)
    with (here / 'vm-adapter.patch').open('rb') as patch:
        subprocess.run(['patch', '-p1', '--batch'], cwd=output, stdin=patch, check=True)
    flags = ['-O3', '-DNDEBUG', '-std=c17', '-fno-fast-math', '-frounding-math',
             '-ffp-contract=off', '-fexcess-precision=standard', '-Wall', '-Wextra', '-Wpedantic']
    architecture = ['-arch', args.arch] if args.arch else []
    for directory in [root / 'bootstrap/include', root / 'runtime/include', root / 'bootstrap/src/bytecode',
                      output, checkout / 'sljit_src', build / 'generated']:
        flags += ['-I', str(directory)]
    objects = []
    commands = []
    def run(command, cwd=root):
        commands.append({'argv': command, 'cwd': str(cwd)})
        subprocess.run(command, cwd=cwd, check=True)
    for name in ['vm', 'vm_jit', 'vm_jit_plan', 'vm_jit_bind', 'vm_jit_sljit', 'vm_image']:
        obj = output / (name + '.o')
        run(['cc', *architecture, *flags, '-c', str(output / (name + '.c')), '-o', str(obj)])
        objects.append(str(obj))
    obj = output / 'sljit.o'
    run(['cc', *architecture, '-O3', '-DNDEBUG', '-DSLJIT_WX_EXECUTABLE_ALLOCATOR=1', '-c',
         str(checkout / 'sljit_src/sljitLir.c'), '-o', str(obj)])
    objects.append(str(obj))
    run(['cc', *architecture, '-O3', *objects, str(build / 'CMakeFiles/mmay.dir/bootstrap/main.c.o'),
         str(build / 'libmmay_bootstrap.a'), '-lm', '-o', str(output / 'mmay')])
    # Keep the official immutable benchmark harness usable with a build layout.
    shutil.copytree(build / 'generated', output / 'generated', dirs_exist_ok=True)
    for path in [build / 'native-runtime']:
        if path.is_dir():
            shutil.copytree(path, output / path.name, dirs_exist_ok=True)
    if args.unit:
        directory = build / 'tests/c-unit'
        command = shlex.split((directory / 'CMakeFiles/mmay_c_unit.dir/link.txt').read_text())
        command[command.index('-o') + 1] = str(output / 'mmay_c_unit')
        command[1:1] = objects
        run(command, directory)
    manifest = {
        'status': 'built-experimental', 'sljit_commit': revision,
        'base_archive_sha256': digest(build / 'libmmay_bootstrap.a'),
        'main_object_sha256': digest(build / 'CMakeFiles/mmay.dir/bootstrap/main.c.o'),
        'prototype_sources_sha256': {path.name: digest(path) for path in sorted(output.glob('*.c'))},
        'compiler_sha256': digest(output / 'mmay'), 'commands': commands,
        'compiler_version': subprocess.check_output(['cc', '--version'], text=True),
        'limitations': origin['evaluation_limits'],
    }
    (output / 'build-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(output / 'mmay')


if __name__ == '__main__':
    main()
