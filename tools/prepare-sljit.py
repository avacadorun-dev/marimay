#!/usr/bin/env python3
"""Prepare verified, locally supplied SLJIT inputs without network access."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil


def digest(data):
    return hashlib.sha256(data).hexdigest()


def apply_zero_context(source, patch):
    lines = patch.splitlines(keepends=True)
    if lines[:2] != ['--- a/sljitNativeARM_64.c\n', '+++ b/sljitNativeARM_64.c\n']:
        raise ValueError('unexpected SLJIT patch target')
    hunks = []
    index = 2
    while index < len(lines):
        match = re.fullmatch(r'@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@\n', lines[index])
        if match is None:
            raise ValueError('expected a zero-context hunk')
        start, old_count, _, new_count = match.groups()
        index += 1
        old, new = [], []
        while index < len(lines) and not lines[index].startswith('@@ '):
            line = lines[index]
            if line.startswith('-'):
                old.append(line[1:])
            elif line.startswith('+'):
                new.append(line[1:])
            else:
                raise ValueError('only zero-context additions/removals are permitted')
            index += 1
        if len(old) != int(old_count or 1) or len(new) != int(new_count or 1):
            raise ValueError('patch hunk length mismatch')
        # An empty old range denotes insertion after the stated line.
        hunks.append((int(start) if not old else int(start) - 1, old, new))
    output = source.splitlines(keepends=True)
    for start, old, new in reversed(hunks):
        if output[start:start + len(old)] != old:
            raise ValueError('patch original text mismatch')
        output[start:start + len(old)] = new
    return ''.join(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['source', 'output', 'manifest', 'config', 'patch']:
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    source, output = args.source.resolve(), args.output.resolve()
    if source == output or output in source.parents:
        raise ValueError('output cannot replace the supplied dependency or its parent')
    manifest = json.loads(args.manifest.read_text())
    if digest(args.patch.read_bytes()) != manifest['arm64_patch_sha256']:
        raise ValueError('SLJIT patch differs from its recorded digest')
    marker = output / '.mmay-sljit-inputs.json'
    if marker.is_symlink():
        raise ValueError('ownership marker must be an ordinary file')
    if output.exists() and any(output.iterdir()):
        if not marker.is_file() or json.loads(marker.read_text()).get('schema') != 'marimay.sljit-inputs/1':
            raise ValueError('refusing to overwrite a nonempty directory not owned by this preparer')
    output.mkdir(parents=True, exist_ok=True)
    marker.write_text(json.dumps({'schema': 'marimay.sljit-inputs/1', 'status': 'preparing'}) + '\n')
    copied = {}
    for name, expected in manifest['files_sha256'].items():
        path = Path(name)
        if path.is_absolute() or '..' in path.parts:
            raise ValueError('invalid manifest path')
        data = (source / path).read_bytes()
        if digest(data) != expected:
            raise ValueError(f'SLJIT input differs from pinned {manifest["commit"]}: {name}')
        target = output / path
        if target.is_symlink() or output not in target.resolve().parents:
            raise ValueError('dependency output must remain within its owned directory')
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        copied[name] = digest(target.read_bytes())
        if copied[name] != expected:
            raise ValueError('copied dependency changed before verification')
    target = output / 'sljit_src/sljitNativeARM_64.c'
    transformed = apply_zero_context(target.read_bytes().decode('utf-8'),
                                     args.patch.read_bytes().decode('utf-8'))
    if digest(transformed.encode()) != manifest['patched_arm64_sha256']:
        raise ValueError('patched SLJIT source differs from its recorded digest')
    # Preserve the pinned LF bytes even on Windows; text output translates them.
    target.write_bytes(transformed.encode('utf-8'))
    if digest(target.read_bytes()) != manifest['patched_arm64_sha256']:
        raise ValueError('published dependency patch bytes changed')
    target = output / 'sljit_src/sljitConfigPre.h'
    if target.is_symlink():
        raise ValueError('custom config must be an owned ordinary file')
    shutil.copyfile(args.config, target)
    result = {'schema': 'marimay.sljit-inputs/1', 'status': 'prepared', 'commit': manifest['commit'],
              'upstream_sha256': copied, 'patch_sha256': digest(args.patch.read_bytes()),
              'patched_arm64_sha256': digest(transformed.encode()),
              'config_sha256': digest(target.read_bytes())}
    marker.write_text(json.dumps(result, indent=2) + '\n')
    print(f'Prepared {len(copied)} verified SLJIT inputs at {output}')


if __name__ == '__main__':
    main()
