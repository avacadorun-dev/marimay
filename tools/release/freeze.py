#!/usr/bin/env python3
"""Preserve validated host assets before CTest regenerates distribution outputs."""
import argparse
from pathlib import Path
import shutil
import sys

import assets
from common import ReleaseError, digest, sha_value

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from macos_payload import physical


def freeze(source, destination, version, sha, arch):
    sha_value(sha)
    expected = assets.names(version, (arch,))
    physical(source)
    physical(destination)
    if destination.exists():
        raise ReleaseError("verified asset destination already exists")
    snapshot = {}
    for name in sorted(expected):
        path = source / name
        physical(path)
        if not path.is_file():
            raise ReleaseError("verified host asset is missing")
        snapshot[name] = (digest(path), path.stat().st_size)
    destination.mkdir(parents=True)
    for name in sorted(expected):
        shutil.copyfile(source / name, destination / name)
    checked = assets.collect(destination, version, sha, (arch,))
    for path in checked:
        if (digest(path), path.stat().st_size) != snapshot[path.name] or (
                digest(source / path.name), (source / path.name).stat().st_size) != snapshot[path.name]:
            raise ReleaseError("verified asset bytes changed while freezing")
    return checked


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--sha", required=True)
    parser.add_argument("--arch", choices=("arm64", "x86_64"), required=True)
    args = parser.parse_args()
    try:
        checked = freeze(args.source.absolute(), args.destination.absolute(),
                         args.version, args.sha, args.arch)
    except (ReleaseError, ValueError, OSError) as error:
        parser.exit(1, "release asset freeze: " + str(error) + "\n")
    print("Preserved %d verified host assets" % len(checked))


if __name__ == "__main__":
    main()
