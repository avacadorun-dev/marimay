"""D114 physical distribution inventories shared by Mac package gates."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import stat
import tarfile


COMPONENT = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]*\Z")
CHECKSUM = re.compile(r"([0-9a-f]{64})  (.+)\Z")
ARCHITECTURES = ("arm64", "x86_64")
MAX_MEMBERS = 50000
MAX_UNPACKED_BYTES = 2 * 1024 * 1024 * 1024


def component(value: str) -> str:
    if COMPONENT.fullmatch(value) is None or value in (".", ".."):
        raise ValueError("version is not a safe path component")
    return value


def relative_name(value: str) -> str:
    if (not value or value in (".", "..") or value.startswith("/") or "\\" in value or ";" in value
            or any(ord(c) < 32 or ord(c) == 127 for c in value)
            or re.match(r"^[A-Za-z]:", value)):
        raise ValueError("unsafe distribution path")
    if len(value.encode("utf-8", errors="strict")) > 32768:
        raise ValueError("distribution path exceeds the tool path limit")
    path = PurePosixPath(value)
    if path.as_posix() != value or any(p in (".", "..") for p in path.parts):
        raise ValueError("noncanonical distribution path")
    return value


def physical(path: Path) -> None:
    for current in (path, *path.parents):
        if current.is_symlink():
            raise ValueError("distribution path contains a symbolic link")


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def verify_checksum(path: Path) -> None:
    physical(path)
    sibling = path.with_name(path.name + ".sha256")
    physical(sibling)
    expected = f"{digest(path)}  {path.name}\n".encode("utf-8")
    if sibling.read_bytes() != expected:
        raise ValueError("archive/package checksum sibling mismatch")


def verify_prefix(root: Path, version: str, arch: str, expected_revision=None) -> dict[str, str]:
    component(version)
    if arch not in ARCHITECTURES:
        raise ValueError("unsupported macOS architecture")
    physical(root)
    if not root.is_dir():
        raise ValueError("install root is not a physical directory")
    manifests = list(root.glob("share/mmay/*/checksums.sha256"))
    manifest_name = f"share/mmay/{version}/checksums.sha256"
    manifest = root / manifest_name
    if manifests != [manifest]:
        raise ValueError("install root must have exactly the requested version manifest")
    physical(manifest)
    raw = manifest.read_bytes()
    if not raw.endswith(b"\n") or b"\r" in raw:
        raise ValueError("distribution manifest must use final LF")
    hashes = {}
    for line in raw.decode("utf-8", errors="strict").splitlines():
        match = CHECKSUM.fullmatch(line)
        if match is None:
            raise ValueError("malformed distribution checksum")
        expected, name = match.groups()
        relative_name(name)
        if name in hashes or name == manifest_name:
            raise ValueError("duplicate or self-referencing checksum path")
        path = root / name
        physical(path)
        if not path.is_file() or digest(path) != expected:
            raise ValueError("distribution checksum mismatch or missing file")
        hashes[name] = expected
    if not hashes:
        raise ValueError("empty distribution checksum manifest")
    inventory = set(hashes) | {manifest_name}
    directories = set()
    for name in inventory:
        directories.update(str(p) for p in PurePosixPath(name).parents if str(p) != ".")
    actual = set()
    for entry in root.rglob("*"):
        relative = relative_name(entry.relative_to(root).as_posix())
        mode = entry.lstat().st_mode
        if stat.S_ISLNK(mode) or not (stat.S_ISREG(mode) or stat.S_ISDIR(mode)):
            raise ValueError("distribution contains a link or special file")
        actual.add(relative)
    if actual != inventory | directories:
        raise ValueError("distribution inventory contains unlisted entries")
    required = {
        "bin/mmay", "include/mmay/runtime/core.h",
        f"lib/mmay/{version}/native/debug/libmmay_native_runtime_debug.a",
        f"lib/mmay/{version}/native/release/libmmay_native_runtime_release.a",
        f"lib/mmay/{version}/native/debug/mmay-native-runtime.manifest",
        f"lib/mmay/{version}/native/release/mmay-native-runtime.manifest",
        f"share/mmay/{version}/mmay-release-metadata.json",
        f"share/mmay/{version}/provenance.json", f"share/mmay/{version}/sbom.spdx.json",
        f"share/mmay/{version}/std/io/io.my",
    }
    if not required <= inventory or not (root / "bin/mmay").stat().st_mode & 0o111:
        raise ValueError("distribution omits required executable/runtime/header/data files")
    metadata = json.loads((root / f"share/mmay/{version}/mmay-release-metadata.json").read_bytes())
    if (not isinstance(metadata, dict) or metadata.get("schema") != "marimay.version/2"
            or metadata.get("compiler") != version or metadata.get("runtime") != version
            or metadata.get("host") != {"system": "Darwin", "processor": arch}
            or not isinstance(metadata.get("standardLibrary"), dict)
            or metadata["standardLibrary"].get("version") != version
            or metadata.get("release") != {"channel": "experimental-alpha",
                "claim": "local-checker-backed-slice", "publicationReady": False}
            or metadata["release"]["publicationReady"] is not False):
        raise ValueError("distribution metadata does not match experimental Mac package identity")
    provenance = json.loads((root / f"share/mmay/{version}/provenance.json").read_bytes())
    source = provenance.get("source", {}) if isinstance(provenance, dict) else {}
    expected_artifacts = {"bin/mmay": hashes["bin/mmay"]}
    for profile in ("debug", "release"):
        expected_artifacts[f"runtime-{profile}"] = hashes[
            f"lib/mmay/{version}/native/{profile}/libmmay_native_runtime_{profile}.a"]
    if (not isinstance(provenance, dict) or not isinstance(source, dict)
            or provenance.get("schema") != "marimay.provenance/1"
            or provenance.get("version") != version
            or provenance.get("host") != {"system": "Darwin", "processor": arch}
            or provenance.get("profile") != "debug-and-release"
            or provenance.get("artifacts") != expected_artifacts
            or not isinstance(source.get("revision"), str)
            or re.fullmatch(r"[0-9a-f]{40}", source["revision"]) is None
            or type(source.get("epoch")) is not int or source["epoch"] < 0
            or (expected_revision is not None and source["revision"] != expected_revision)):
        raise ValueError("distribution provenance does not match its files or source revision")
    sbom = json.loads((root / f"share/mmay/{version}/sbom.spdx.json").read_bytes())
    if not isinstance(sbom, dict) or sbom.get("spdxVersion") != "SPDX-2.3":
        raise ValueError("distribution SBOM is not SPDX 2.3 JSON")
    return hashes


def unpack_archive(archive: Path, destination: Path) -> None:
    """Copy only bounded regular tar members; never delegate link extraction."""
    physical(archive)
    physical(destination)
    if destination.exists():
        raise ValueError("archive destination already exists")
    with tarfile.open(archive, "r:gz") as source:
        members = []
        total = 0
        names = set()
        for member in source:
            relative_name(member.name)
            if (not member.isfile() or member.issparse() or member.name in names
                    or member.size < 0):
                raise ValueError("archive contains duplicate, linked or special entries")
            names.add(member.name)
            total += member.size
            if len(names) > MAX_MEMBERS or total > MAX_UNPACKED_BYTES:
                raise ValueError("archive exceeds distribution extraction limits")
            members.append(member)
        if any(str(parent) in names for name in names
               for parent in PurePosixPath(name).parents if str(parent) != "."):
            raise ValueError("archive file collides with a parent directory")
        destination.mkdir(mode=0o700)
        for member in members:
            output = destination / member.name
            output.parent.mkdir(parents=True, exist_ok=True)
            stream = source.extractfile(member)
            if stream is None:
                raise ValueError("archive file has no data")
            with stream, output.open("xb") as target:
                remaining = member.size
                while remaining:
                    block = stream.read(min(remaining, 1024 * 1024))
                    if not block:
                        raise ValueError("truncated archive member")
                    target.write(block)
                    remaining -= len(block)
            output.chmod(0o755 if member.mode & 0o100 else 0o644)
