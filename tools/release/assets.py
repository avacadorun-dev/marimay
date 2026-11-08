"""Validate the exact release asset inventory before privileged publication."""
import hashlib
import json
from pathlib import PurePosixPath
import re
import tarfile
import unicodedata

from common import ReleaseError, digest, version_value


def names(version, architectures=("arm64", "x86_64")):
    version_value(version)
    if (not architectures or len(set(architectures)) != len(architectures)
            or any(arch not in ("arm64", "x86_64") for arch in architectures)):
        raise ReleaseError("invalid macOS asset architecture set")
    archives = ["marimay-" + version + "-darwin-" + arch + extension
                for arch in architectures for extension in (".tar.gz", ".pkg")]
    archives.append("marimay-" + version + "-source.tar.gz")
    archives.extend("macos-verification-" + arch + ".json" for arch in architectures)
    return set(archives + [name + ".sha256" for name in archives])


def inspect_tar(path, version, sha):
    count, total = 0, 0
    found = set()
    normalized = set()
    hashes = {}
    manifest = None
    metadata = None
    source = path.name.endswith("-source.tar.gz")
    with tarfile.open(path, "r:gz") as archive:
        for member in archive:
            count += 1
            total += member.size
            name = PurePosixPath(member.name)
            folded = unicodedata.normalize("NFD", member.name).casefold()
            if (count > 20000 or total > 1024 * 1024 * 1024 or member.size > 256 * 1024 * 1024
                    or name.is_absolute() or ".." in name.parts
                    or name.as_posix() != member.name or folded in normalized
                    or any(ord(char) < 32 or ord(char) == 127 for char in member.name)
                    or not member.isfile() or member.name in found
                    or any(part in (".git", ".mmay") for part in name.parts)):
                raise ReleaseError("unsafe or oversized release archive")
            found.add(member.name)
            normalized.add(folded)
            stream = archive.extractfile(member)
            value = hashlib.sha256()
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                value.update(block)
            hashes[member.name] = value.hexdigest()
            if member.name == "share/mmay/" + version + "/checksums.sha256":
                if member.size > 8 * 1024 * 1024:
                    raise ReleaseError("oversized installed inventory")
                manifest = archive.extractfile(member).read().decode("utf-8")
            if member.name == "share/mmay/" + version + "/mmay-release-metadata.json":
                if member.size > 1024 * 1024:
                    raise ReleaseError("oversized installed metadata")
                metadata = json.load(archive.extractfile(member))
        if not count:
            raise ReleaseError("empty release archive")
    if source:
        if not all(name.startswith("marimay-" + version + "-source/") for name in found):
            raise ReleaseError("invalid source archive root")
    else:
        arch = next((arch for arch in ("arm64", "x86_64")
                     if path.name == "marimay-" + version + "-darwin-" + arch + ".tar.gz"), None)
        if arch is None:
            raise ReleaseError("host archive name is not bound to an architecture")
        if "bin/mmay" not in found or not metadata:
            raise ReleaseError("host archive lacks the complete installed toolchain")
        manifest_name = "share/mmay/" + version + "/checksums.sha256"
        inventory = {}
        for line in (manifest or "").splitlines():
            match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
            if not match or match[2] in inventory:
                raise ReleaseError("invalid installed checksum inventory")
            inventory[match[2]] = match[1]
        if inventory != {name: value for name, value in hashes.items() if name != manifest_name}:
            raise ReleaseError("installed archive inventory/checksum mismatch")
        for profile in ("debug", "release"):
            prefix = "lib/mmay/" + version + "/native/" + profile + "/"
            if (prefix + "mmay-native-runtime.manifest" not in found
                    or not any(name.startswith(prefix) and name.endswith(".a") for name in found)):
                raise ReleaseError("host archive lacks both native runtime profiles")
        if not any(name.startswith("include/mmay/") for name in found):
            raise ReleaseError("host archive lacks required headers")
        if (metadata.get("compiler") != version
                or metadata.get("schema") != "marimay.version/2"
                or metadata.get("release", {}).get("channel") != "experimental-alpha"
                or metadata.get("release", {}).get("claim") != "local-checker-backed-slice"
                or metadata.get("release", {}).get("publicationReady") is not False
                or metadata.get("runtime") != version
                or metadata.get("host", {}).get("system") != "Darwin"
                or metadata.get("host", {}).get("processor") != arch
                or metadata.get("standardLibrary", {}).get("version") != version):
            raise ReleaseError("host archive metadata does not match the release")
        required = "share/mmay/" + version + "/provenance.json"
        if required not in found:
            raise ReleaseError("host archive provenance is missing")
        with tarfile.open(path, "r:gz") as archive:
            member = archive.getmember(required)
            if member.size > 1024 * 1024:
                raise ReleaseError("oversized provenance")
            provenance = json.load(archive.extractfile(member))
        # The provenance schema uses the build's exact source revision.
        if (provenance.get("schema") != "marimay.provenance/1"
                or provenance.get("version") != version
                or provenance.get("source", {}).get("revision") != sha):
            raise ReleaseError("host archive provenance has another source revision")


def collect(root, version, sha, architectures=("arm64", "x86_64")):
    expected = names(version, architectures)
    files = {}
    for path in root.rglob("*"):
        if path.is_symlink():
            raise ReleaseError("release artifact contains a symbolic link")
        if path.is_dir():
            continue
        if not path.is_file() or path.name not in expected:
            raise ReleaseError("unexpected release artifact")
        if path.name in files:
            if digest(files[path.name]) != digest(path):
                raise ReleaseError("inconsistent duplicate source artifact")
            if "-source.tar.gz" not in path.name:
                raise ReleaseError("duplicate host artifact")
        files[path.name] = path
    if set(files) != expected:
        raise ReleaseError("release artifact inventory is incomplete")
    for name in sorted(expected):
        path = files[name]
        if name.endswith(".sha256"):
            continue
        checksum = files[name + ".sha256"].read_text(encoding="ascii")
        match = re.fullmatch(r"([0-9a-f]{64}) [ *]([^\n]+)\n?", checksum)
        if not match or match[2] != name or match[1] != digest(path):
            raise ReleaseError("release checksum mismatch")
        if name.endswith(".tar.gz"):
            inspect_tar(path, version, sha)
        elif name.endswith(".pkg"):
            with path.open("rb") as stream:
                if path.stat().st_size < 28 or stream.read(4) != b"xar!":
                    raise ReleaseError("invalid macOS Installer container")
    checks = {key: True for key in (
        "pkgutil-layout", "installer", "doctor-installed", "vm-run", "test-both",
        "native-debug", "native-release", "archive-relocation",
        "foreign-command-rejected", "same-version-rejected")}
    for arch in architectures:
        path = files["macos-verification-" + arch + ".json"]
        if path.stat().st_size > 64 * 1024:
            raise ReleaseError("oversized macOS verification record")
        evidence = json.loads(path.read_text(encoding="utf-8"))
        if (evidence.get("schema") != "marimay.macos-install-verification/1"
                or evidence.get("sourceRevision") != sha or evidence.get("version") != version
                or evidence.get("arch") != arch or evidence.get("checks") != checks):
            raise ReleaseError("macOS installation verification is missing or mismatched")
        # The Mac job used pkgutil and real Installer. Bind that proof to the
        # exact uploaded opaque .pkg bytes; the XAR magic alone is not proof.
        for kind, extension in (("package", ".pkg"), ("archive", ".tar.gz")):
            name = "marimay-" + version + "-darwin-" + arch + extension
            asset = files[name]
            expected_record = {"name": name, "sha256": digest(asset),
                               "bytes": asset.stat().st_size}
            if evidence.get(kind) != expected_record:
                raise ReleaseError("macOS install proof is for different artifact bytes")
    return [files[name] for name in sorted(expected)]
