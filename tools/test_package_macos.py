#!/usr/bin/env python3
"""D114 package inventory, installer ownership and activation regressions."""

from __future__ import annotations

import hashlib
import importlib.machinery
import importlib.util
import io
import json
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

from macos_payload import digest, unpack_archive, verify_checksum, verify_prefix


TOOL = Path(__file__).resolve().with_name("package-macos")
LOADER = importlib.machinery.SourceFileLoader("package_macos", str(TOOL))
SPEC = importlib.util.spec_from_loader(LOADER.name, LOADER)
PACKAGE = importlib.util.module_from_spec(SPEC)
LOADER.exec_module(PACKAGE)
SMOKE_LOADER = importlib.machinery.SourceFileLoader("macos_install_gate", str(TOOL.with_name("test-macos-install")))
SMOKE_SPEC = importlib.util.spec_from_loader(SMOKE_LOADER.name, SMOKE_LOADER)
SMOKE = importlib.util.module_from_spec(SMOKE_SPEC)
SMOKE_LOADER.exec_module(SMOKE)
VERSION = "0.1.0-alpha.1"
ARCH = "arm64"


def manifest(root, version):
    path = root / f"share/mmay/{version}/checksums.sha256"
    lines = []
    for file in sorted(root.rglob("*")):
        if file.is_file() and file != path:
            lines.append(f"{digest(file)}  {file.relative_to(root).as_posix()}\n")
    path.write_bytes("".join(lines).encode("utf-8"))
    return path


def provenance(root, version, arch):
    artifacts = {"bin/mmay": digest(root / "bin/mmay")}
    for profile in ("debug", "release"):
        artifacts[f"runtime-{profile}"] = digest(root /
            f"lib/mmay/{version}/native/{profile}/libmmay_native_runtime_{profile}.a")
    record = {"schema": "marimay.provenance/1", "version": version,
        "source": {"revision": "1" * 40, "epoch": 0},
        "host": {"system": "Darwin", "processor": arch},
        "profile": "debug-and-release", "artifacts": artifacts}
    (root / f"share/mmay/{version}/provenance.json").write_bytes(json.dumps(record).encode())


def prefix(root, version=VERSION, arch=ARCH):
    files = {
        "bin/mmay": b"#!/bin/sh\nexit 0\n", "include/mmay/runtime/core.h": b"header\n",
        f"share/mmay/{version}/std/io/io.my": b"module\n",
        f"share/mmay/{version}/provenance.json": b"{}\n",
        f"share/mmay/{version}/sbom.spdx.json": b'{"spdxVersion":"SPDX-2.3"}\n',
    }
    for profile in ("debug", "release"):
        base = f"lib/mmay/{version}/native/{profile}"
        files[f"{base}/libmmay_native_runtime_{profile}.a"] = b"runtime\n"
        files[f"{base}/mmay-native-runtime.manifest"] = b"runtime identity\n"
    metadata = {"schema": "marimay.version/2", "compiler": version, "runtime": version,
        "host": {"system": "Darwin", "processor": arch},
        "standardLibrary": {"version": version},
        "release": {"channel": "experimental-alpha", "claim": "local-checker-backed-slice",
                    "publicationReady": False}}
    files[f"share/mmay/{version}/mmay-release-metadata.json"] = (
        json.dumps(metadata, separators=(",", ":")) + "\n").encode()
    for name, data in files.items():
        file = root / name
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_bytes(data)
        file.chmod(0o755 if name == "bin/mmay" else 0o644)
    provenance(root, version, arch)
    return manifest(root, version)


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="mmay package проверка ")
        self.addCleanup(self.temporary.cleanup)
        self.work = Path(self.temporary.name).resolve()
        self.root = self.work / "source prefix"
        prefix(self.root)

    def fake_run(self, arguments, **kwargs):
        if arguments[0] == "/usr/bin/lipo":
            return subprocess.CompletedProcess(arguments, 0, (ARCH + "\n").encode(), b"")
        self.assertEqual(arguments[0], "/usr/bin/pkgbuild")
        payload = Path(arguments[arguments.index("--root") + 1])
        self.assertEqual(verify_prefix(payload, VERSION, ARCH), verify_prefix(self.root, VERSION, ARCH))
        self.assertFalse((payload / "usr/local/bin/mmay").exists())
        self.assertEqual(arguments[arguments.index("--install-location") + 1],
                         f"/usr/local/lib/marimay/{VERSION}")
        self.assertEqual(arguments[arguments.index("--identifier") + 1], PACKAGE.receipt_identifier(VERSION))
        self.assertEqual(arguments[arguments.index("--ownership") + 1], "recommended")
        self.assertEqual(arguments[arguments.index("--filter") + 1], "^$")
        scripts = Path(arguments[arguments.index("--scripts") + 1])
        self.assertEqual({p.name for p in scripts.iterdir()},
                         {"preinstall", "postinstall", "installer-common.sh"})
        for name in ("preinstall", "postinstall"):
            self.assertEqual((scripts / name).stat().st_mode & 0o7777, 0o755)
        self.assertEqual((payload / "bin/mmay").stat().st_mode & 0o7777, 0o755)
        Path(arguments[-1]).write_bytes(b"synthetic native package\n")
        return subprocess.CompletedProcess(arguments, 0, b"", b"")

    def test_package_is_complete_and_source_is_unchanged(self):
        (self.root / "bin/mmay").chmod(0o4755)
        before = {p.relative_to(self.root).as_posix(): (p.read_bytes(), p.stat().st_mode)
                  for p in self.root.rglob("*") if p.is_file()}
        with mock.patch.object(PACKAGE.platform, "system", return_value="Darwin"), \
                mock.patch.object(PACKAGE.subprocess, "run", side_effect=self.fake_run):
            output = PACKAGE.build_package(self.root, self.work / "output", VERSION, ARCH)
        verify_checksum(output)
        self.assertEqual(output.name, f"marimay-{VERSION}-darwin-{ARCH}.pkg")
        self.assertEqual(before, {p.relative_to(self.root).as_posix(): (p.read_bytes(), p.stat().st_mode)
                                 for p in self.root.rglob("*") if p.is_file()})
        with self.assertRaisesRegex(ValueError, "already exists"), \
                mock.patch.object(PACKAGE.platform, "system", return_value="Darwin"), \
                mock.patch.object(PACKAGE.subprocess, "run", side_effect=self.fake_run):
            PACKAGE.build_package(self.root, output.parent, VERSION, ARCH)

    def test_bad_inventory_is_rejected_before_output(self):
        for case in ("tampered", "missing", "unlisted", "directory", "duplicate", "unsafe", "self"):
            with self.subTest(case=case):
                root = self.work / case
                shutil.copytree(self.root, root)
                path = root / f"share/mmay/{VERSION}/checksums.sha256"
                if case == "tampered": (root / "bin/mmay").write_bytes(b"changed")
                elif case == "missing": (root / "include/mmay/runtime/core.h").unlink()
                elif case == "unlisted": (root / "unlisted").write_bytes(b"unowned")
                elif case == "directory": (root / "unowned empty directory").mkdir()
                elif case == "duplicate": path.write_bytes(path.read_bytes() + path.read_bytes().splitlines()[0] + b"\n")
                elif case == "unsafe": path.write_bytes(b"0" * 64 + b"  ../escape\n")
                else: path.write_bytes(b"0" * 64 + f"  share/mmay/{VERSION}/checksums.sha256\n".encode())
                with self.assertRaises(ValueError):
                    PACKAGE.build_package(root, self.work / (case + " output"), VERSION, ARCH)
                self.assertFalse((self.work / (case + " output")).exists())

    def test_linked_roots_and_members_fail(self):
        linked = self.work / "linked root"
        linked.symlink_to(self.root, target_is_directory=True)
        with self.assertRaises(ValueError): verify_prefix(linked, VERSION, ARCH)
        (self.root / "unexpected link").symlink_to(self.work)
        with self.assertRaises(ValueError): verify_prefix(self.root, VERSION, ARCH)

    def test_metadata_and_archive_components_fail_closed(self):
        path = self.root / f"share/mmay/{VERSION}/mmay-release-metadata.json"
        metadata = json.loads(path.read_bytes())
        for field, value in (("compiler", "stale"), ("host", {"system": "Darwin", "processor": "x86_64"}),
                             ("release", {"channel": "private-preview"})):
            changed = dict(metadata)
            changed[field] = value
            path.write_bytes(json.dumps(changed).encode())
            manifest(self.root, VERSION)
            with self.assertRaisesRegex(ValueError, "metadata"):
                verify_prefix(self.root, VERSION, ARCH)
        for version in ("../escape", "a/b", "a;bad", "", ".", "..", "-option"):
            with self.subTest(version=version), self.assertRaises(ValueError): PACKAGE.wrapper(version)

    def test_provenance_and_sbom_cannot_disagree_with_payload(self):
        with self.assertRaisesRegex(ValueError, "provenance"):
            verify_prefix(self.root, VERSION, ARCH, expected_revision="2" * 40)
        for kind in ("artifact", "source", "sbom", "boolean"):
            with self.subTest(kind=kind):
                root = self.work / kind
                shutil.copytree(self.root, root)
                if kind == "sbom":
                    path = root / f"share/mmay/{VERSION}/sbom.spdx.json"
                    path.write_bytes(b'{"spdxVersion":"incorrect"}')
                elif kind == "boolean":
                    path = root / f"share/mmay/{VERSION}/mmay-release-metadata.json"
                    data = json.loads(path.read_bytes())
                    data["release"]["publicationReady"] = 0
                    path.write_bytes(json.dumps(data).encode())
                else:
                    path = root / f"share/mmay/{VERSION}/provenance.json"
                    data = json.loads(path.read_bytes())
                    if kind == "artifact": data["artifacts"]["bin/mmay"] = "0" * 64
                    else: data["source"]["revision"] = "unknown"
                    path.write_bytes(json.dumps(data).encode())
                manifest(root, VERSION)
                with self.assertRaises(ValueError): verify_prefix(root, VERSION, ARCH)

    def test_wrong_binary_or_runtime_architecture_is_rejected(self):
        for index in range(3):
            calls = []
            def run(arguments, **kwargs):
                calls.append(arguments)
                data = b"x86_64\n" if len(calls) == index + 1 else b"arm64\n"
                return subprocess.CompletedProcess(arguments, 0, data, b"")
            with self.subTest(index=index), self.assertRaises(ValueError), \
                    mock.patch.object(PACKAGE.subprocess, "run", side_effect=run):
                PACKAGE.check_architecture(self.root, VERSION, ARCH)

    def test_pkgbuild_failure_never_publishes(self):
        def run(arguments, **kwargs):
            if arguments[0] == "/usr/bin/lipo": return self.fake_run(arguments, **kwargs)
            Path(arguments[-1]).write_bytes(b"partial")
            raise subprocess.CalledProcessError(1, arguments, b"", b"fixture failure")
        output = self.work / "failed output"
        with self.assertRaises(subprocess.CalledProcessError), \
                mock.patch.object(PACKAGE.platform, "system", return_value="Darwin"), \
                mock.patch.object(PACKAGE.subprocess, "run", side_effect=run):
            PACKAGE.build_package(self.root, output, VERSION, ARCH)
        self.assertEqual(list(output.iterdir()), [])

    def test_package_archive_drift_is_reported_without_weakening_byte_checks(self):
        archive_root = self.work / "archive prefix"
        shutil.copytree(self.root, archive_root)
        io_source = archive_root / f"share/mmay/{VERSION}/std/io/io.my"
        io_source.write_bytes(b"different valid prefix\n")
        manifest(archive_root, VERSION)
        expanded = self.work / "expanded package"
        shutil.copytree(self.root, expanded / "Payload")
        info = ET.Element("pkg-info", {"identifier": PACKAGE.receipt_identifier(VERSION), "version": VERSION,
                                      "install-location": f"/usr/local/lib/marimay/{VERSION}"})
        ET.ElementTree(info).write(expanded / "PackageInfo")
        result = subprocess.CompletedProcess([], 0, b"", b"")
        with mock.patch.object(SMOKE, "command", return_value=result), \
                self.assertRaisesRegex(AssertionError, r"changed=\['share/mmay/.*/std/io/io.my'\]"):
            SMOKE.inspect_package(self.work / "fixture.pkg", archive_root, self.work, VERSION, ARCH)

    def test_concurrently_created_output_is_never_overwritten(self):
        output = self.work / "concurrent output"
        def run(arguments, **kwargs):
            result = self.fake_run(arguments, **kwargs)
            if arguments[0] == "/usr/bin/pkgbuild":
                (output / f"marimay-{VERSION}-darwin-{ARCH}.pkg").write_bytes(b"foreign output")
            return result
        with self.assertRaises(FileExistsError), \
                mock.patch.object(PACKAGE.platform, "system", return_value="Darwin"), \
                mock.patch.object(PACKAGE.subprocess, "run", side_effect=run):
            PACKAGE.build_package(self.root, output, VERSION, ARCH)
        self.assertEqual((output / f"marimay-{VERSION}-darwin-{ARCH}.pkg").read_bytes(), b"foreign output")
        self.assertFalse((output / f"marimay-{VERSION}-darwin-{ARCH}.pkg.sha256").exists())

    @unittest.skipUnless(platform.system() == "Darwin", "native macOS package container gate")
    def test_real_pkgbuild_and_pkgutil_preserve_the_complete_prefix(self):
        arch = platform.machine()
        prefix(self.root, VERSION, arch)
        source = self.work / "fixture.c"
        source.write_bytes(b"int main(void) { return 0; }\n")
        object_file = self.work / "fixture.o"
        subprocess.run(["/usr/bin/clang", "-arch", arch, "-mmacosx-version-min=15.0",
                        "-c", str(source), "-o", str(object_file)], check=True, capture_output=True)
        subprocess.run(["/usr/bin/clang", "-arch", arch, "-mmacosx-version-min=15.0",
                        str(object_file), "-o", str(self.root / "bin/mmay")], check=True, capture_output=True)
        for profile in ("debug", "release"):
            archive = self.root / f"lib/mmay/{VERSION}/native/{profile}/libmmay_native_runtime_{profile}.a"
            archive.unlink()
            subprocess.run(["/usr/bin/ar", "rcs", str(archive), str(object_file)],
                           check=True, capture_output=True)
        provenance(self.root, VERSION, arch)
        manifest(self.root, VERSION)
        output = PACKAGE.build_package(self.root, self.work / "native package", VERSION, arch)
        verify_checksum(output)
        expanded = self.work / "expanded package"
        subprocess.run(["/usr/sbin/pkgutil", "--expand-full", str(output), str(expanded)],
                       check=True, capture_output=True)
        info = ET.parse(expanded / "PackageInfo").getroot()
        self.assertEqual(info.attrib["identifier"], PACKAGE.receipt_identifier(VERSION))
        self.assertEqual(info.attrib["version"], VERSION)
        self.assertEqual(info.attrib["install-location"], f"/usr/local/lib/marimay/{VERSION}")
        self.assertEqual(verify_prefix(expanded / "Payload", VERSION, arch),
                         verify_prefix(self.root, VERSION, arch))
        self.assertFalse((expanded / "Payload/usr/local/bin/mmay").exists())
        for script in ("preinstall", "postinstall", "installer-common.sh"):
            self.assertTrue((expanded / "Scripts" / script).is_file())


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="mmay installer Мир ")
        self.addCleanup(self.temporary.cleanup)
        self.work = Path(self.temporary.name).resolve()
        self.volume = self.work / "volume"
        self.volume.mkdir()
        self.source = self.work / "source"
        checksum = prefix(self.source)
        self.scripts = self.work / "scripts"
        PACKAGE.render_scripts(self.scripts, VERSION, ARCH, digest(checksum))
        self.candidate = self.volume / f"usr/local/lib/marimay/{VERSION}"
        self.command = self.volume / "usr/local/bin/mmay"

    def hook(self, operation, expected=0, prefix_code=""):
        # Exercise the real installer primitives in an isolated target volume.
        # Actual host probes and Installer integration run on both Mac CI hosts.
        reader = self.receipt_reader() if hasattr(self, "active_version") else "mmay_receipt_list() { return 1; }"
        code = ('set -eu; . "$1/installer-common.sh"; '
                'mmay_setup package / "$2"; ' + reader + '; '
                + (prefix_code or ":") + '; mmay_' + operation)
        result = subprocess.run(["/bin/sh", "-c", code, "test", str(self.scripts), str(self.volume)],
                                capture_output=True, timeout=30)
        self.assertEqual(result.returncode, expected, result.stderr)
        return result

    def publish_candidate(self):
        self.candidate.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(self.source, self.candidate)

    def old(self, version="0.1.0-alpha.0"):
        root = self.volume / f"usr/local/lib/marimay/{version}"
        prefix(root, version)
        self.command.parent.mkdir(parents=True, exist_ok=True)
        self.command.write_bytes(PACKAGE.wrapper(version))
        self.command.chmod(0o755)
        self.active_version = version
        self.receipt_files(version)
        return root

    def test_install_then_same_version_is_refused_unchanged(self):
        self.hook("preinstall")
        self.publish_candidate()
        self.hook("postinstall")
        self.assertEqual(self.command.read_bytes(), PACKAGE.wrapper(VERSION))
        self.assertEqual(self.command.stat().st_mode & 0o7777, 0o755)
        self.assertEqual(self.command.parent.stat().st_mode & 0o7777, 0o755)
        before = self.command.read_bytes()
        self.hook("preinstall", 1)
        self.assertEqual(self.command.read_bytes(), before)

    def test_upgrade_preserves_old_prefix_and_unrelated_files(self):
        old = self.old()
        (old / "user notes").write_bytes(b"preserve")
        before = {p.relative_to(old).as_posix(): p.read_bytes() for p in old.rglob("*") if p.is_file()}
        self.hook("preinstall")
        self.publish_candidate()
        self.hook("postinstall")
        self.assertEqual(self.command.read_bytes(), PACKAGE.wrapper(VERSION))
        self.assertEqual(before, {p.relative_to(old).as_posix(): p.read_bytes()
                                 for p in old.rglob("*") if p.is_file()})

    def test_foreign_modified_and_linked_commands_are_preserved(self):
        self.command.parent.mkdir(parents=True)
        for content in (b"foreign\n", b"foreign" * 300,
                        PACKAGE.wrapper("0.1.0-alpha.0") + b"# modified\n"):
            self.command.write_bytes(content)
            self.hook("preinstall", 1)
            self.assertEqual(self.command.read_bytes(), content)
        self.command.unlink()
        target = self.work / "foreign"
        target.write_bytes(b"untouched")
        self.command.symlink_to(target)
        self.hook("preinstall", 1)
        self.assertEqual(target.read_bytes(), b"untouched")

    def test_modified_owned_prefix_and_linked_ancestor_are_refused(self):
        old = self.old()
        (old / "bin/mmay").write_bytes(b"modified")
        before = self.command.read_bytes()
        self.hook("preinstall", 1)
        self.assertEqual(self.command.read_bytes(), before)
        shutil.rmtree(self.volume / "usr")
        outside = self.work / "outside"
        outside.mkdir()
        (self.volume / "usr").symlink_to(outside, target_is_directory=True)
        self.hook("preinstall", 1)
        self.assertEqual(list(outside.iterdir()), [])

    def test_payload_or_activation_failure_preserves_active_command(self):
        self.old()
        before = self.command.read_bytes()
        self.hook("preinstall")
        self.publish_candidate()
        (self.candidate / "bin/mmay").write_bytes(b"corrupt")
        self.hook("postinstall", 1)
        self.assertEqual(self.command.read_bytes(), before)
        shutil.copyfile(self.source / "bin/mmay", self.candidate / "bin/mmay")
        # Override only mv in this private test shell to force the actual final
        # rename branch to fail after complete payload/ownership validation.
        common = self.scripts / "installer-common.sh"
        common.write_bytes(common.read_bytes().replace(b"/bin/mv -f", b"mmay_test_mv -f"))
        self.hook("postinstall", 1, "mmay_test_mv() { return 1; }")
        self.assertEqual(self.command.read_bytes(), before)
        self.assertEqual(list(self.command.parent.glob(".mmay-activate.*")), [])

    def test_unlisted_or_linked_candidate_entries_never_activate(self):
        self.old()
        before = self.command.read_bytes()
        self.publish_candidate()
        for kind in ("file", "directory", "link", "newline"):
            with self.subTest(kind=kind):
                extra = self.candidate / ("unexpected\n" if kind == "newline" else "unexpected")
                if kind == "directory": extra.mkdir()
                elif kind == "link": extra.symlink_to(self.work)
                else: extra.write_bytes(b"unlisted")
                self.hook("postinstall", 1)
                self.assertEqual(self.command.read_bytes(), before)
                if kind == "directory": extra.rmdir()
                else: extra.unlink()

    def test_platform_checks_reject_wrong_arch_old_os_and_foreign_host(self):
        common = self.scripts / "installer-common.sh"
        for system, arch, version, expected in (("Darwin", ARCH, "15.0", 0),
                ("Darwin", "x86_64", "15.0", 1), ("Darwin", ARCH, "14.9", 1),
                ("Linux", ARCH, "99", 1), ("Darwin", ARCH, "garbled", 1)):
            result = subprocess.run(["/bin/sh", "-c", '. "$1"; mmay_platform_values "$2" "$3" "$4"',
                                     "test", str(common), system, arch, version], capture_output=True)
            self.assertEqual(result.returncode, expected, result.stderr)

    def receipt_files(self, version="0.1.0-alpha.0", identifier=None):
        identifier = identifier or SMOKE.receipt_identifier(version)
        base = self.volume / f"private/var/db/receipts/{identifier}"
        base.parent.mkdir(parents=True, exist_ok=True)
        files = [Path(str(base) + "." + suffix) for suffix in ("plist", "bom")]
        for path in files:
            path.write_bytes(b"synthetic receipt boundary\n")
        return files

    def receipt_reader(self, **changed):
        version = getattr(self, "active_version", "0.1.0-alpha.0")
        identifier = SMOKE.receipt_identifier(version)
        fields = {"pkgid": identifier, "pkg-version": version,
                  "install-location": f"usr/local/lib/marimay/{version}",
                  "volume": str(self.volume)}
        fields.update(changed)
        cases = []
        for name, value in fields.items():
            body = "return 1" if value is None else "printf '%s\\n' " + shlex.quote(value)
            cases.append(shlex.quote(name) + ") " + body + ";;")
        return ('mmay_receipt_list() { if [ "$1" = ' + shlex.quote(identifier)
                + " ]; then printf '%s\\n' " + shlex.quote(identifier) + "; else return 1; fi; }; "
                "mmay_receipt_info() { printf 'synthetic plist reader boundary\\n'; }; "
                'mmay_receipt_field() { case "$1" in ' + " ".join(cases) + " esac; }")

    def test_receipt_absence_requires_exact_query_and_no_partial_files(self):
        self.hook("preinstall")
        for reader in ("return 2", "return 127", "return 0",
                       "printf 'org.marimay.mmay\\n'; return 1",
                       "printf 'query failed\\n' >&2; return 1"):
            self.hook("preinstall", 1, "mmay_receipt_list() { " + reader + "; }")
        files = self.receipt_files(VERSION)
        for path in files:
            other = files[1] if path == files[0] else files[0]
            other.unlink(missing_ok=True)
            path.write_bytes(b"preserve partial receipt\n")
            self.hook("preinstall", 1)
            self.assertEqual(path.read_bytes(), b"preserve partial receipt\n")
            path.unlink()
        files[0].symlink_to(self.work / "missing receipt")
        self.hook("preinstall", 1)

    def test_active_version_receipt_requires_exact_fields_and_complete_files(self):
        files = self.receipt_files()
        before = [path.read_bytes() for path in files]
        # An inactive version's receipt is retained without enumerating it.
        self.hook("preinstall", prefix_code=self.receipt_reader())
        self.old()
        command_before = self.command.read_bytes()
        self.hook("preinstall", prefix_code=self.receipt_reader())
        for field, value in (("pkgid", "foreign.identifier"), ("pkg-version", "0.0.0"),
                ("install-location", "/foreign/prefix"), ("volume", "/other-volume"),
                ("install-location", "/usr/local/lib/marimay/0.1.0-alpha.0"),
                ("install-location", "usr/local/lib//marimay/0.1.0-alpha.0"),
                ("install-location", "usr/local/lib/other/0.1.0-alpha.0"),
                ("install-location", "usr/local/lib/../marimay/0.1.0-alpha.0"),
                ("install-location", "usr/local/lib\\marimay/0.1.0-alpha.0"),
                ("install-location", ""), ("install-location", None),
                ("pkg-version", "0.1.0-alpha.0\n"), ("pkg-version", None)):
            with self.subTest(field=field, value=value):
                self.hook("preinstall", 1, self.receipt_reader(**{field: value}))
        for extra in ("mmay_receipt_info() { return 1; }",
                      "mmay_receipt_info() { printf 'error' >&2; }",
                      "mmay_receipt_list() { printf 'foreign.identifier\\n'; }",
                      "mmay_receipt_field() { printf 'error' >&2; }"):
            self.hook("preinstall", 1, self.receipt_reader() + "; " + extra)
        files[1].unlink()
        self.hook("preinstall", 1, self.receipt_reader())
        self.assertEqual(files[0].read_bytes(), before[0])
        self.assertEqual(self.command.read_bytes(), command_before)
        self.assertFalse(self.candidate.exists())

    def test_candidate_and_retired_receipts_are_refused_and_active_receipt_is_required(self):
        for identifier in (SMOKE.receipt_identifier(VERSION), SMOKE.IDENTIFIER):
            files = self.receipt_files(identifier=identifier)
            reader = ('mmay_receipt_list() { if [ "$1" = ' + shlex.quote(identifier)
                      + " ]; then printf '%s\\n' " + shlex.quote(identifier) + "; else return 1; fi; }")
            self.hook("preinstall", 1, reader)
            self.assertTrue(all(path.read_bytes() == b"synthetic receipt boundary\n" for path in files))
            for path in files: path.unlink()
        self.old()
        before = self.command.read_bytes()
        for path in self.receipt_files(): path.unlink()
        self.hook("preinstall", 1, "mmay_receipt_list() { return 1; }")
        self.assertEqual(self.command.read_bytes(), before)

    def test_receipt_mismatch_identifies_field_and_digest_without_raw_value(self):
        self.old()
        self.receipt_files()
        value = "synthetic mismatched receipt version"
        result = self.hook("preinstall", 1, self.receipt_reader(**{"pkg-version": value}))
        fingerprint = hashlib.sha256((value + "\n").encode()).hexdigest()
        self.assertIn((f"package receipt pkg-version mismatch (sha256={fingerprint}, "
                       f"bytes={len(value) + 1})").encode(), result.stderr)
        self.assertNotIn(value.encode(), result.stderr)

    @unittest.skipUnless(platform.system() == "Darwin", "stock macOS plist reader boundary")
    def test_stock_plutil_receipt_strings_are_exact_and_typed(self):
        import plistlib
        self.old()
        self.receipt_files()
        fixture = self.work / "queried receipt.plist"
        fields = {"pkgid": SMOKE.receipt_identifier("0.1.0-alpha.0"), "pkg-version": "0.1.0-alpha.0",
                  "install-location": "usr/local/lib/marimay/0.1.0-alpha.0",
                  "volume": str(self.volume)}
        identifier = SMOKE.receipt_identifier("0.1.0-alpha.0")
        reader = ('. "$1/installer-common.sh"; mmay_receipt_list() { if [ "$1" = ' + shlex.quote(identifier)
                  + " ]; then printf '%s\\n' " + shlex.quote(identifier) + "; else return 1; fi; }; "
                  "mmay_receipt_info() { /bin/cat " + shlex.quote(str(fixture)) + "; }")
        fixture.write_bytes(plistlib.dumps(fields))
        self.hook("preinstall", prefix_code=reader)
        for value in (7, "0.1.0-alpha.0\n", None):
            malformed = dict(fields)
            if value is None: del malformed["pkg-version"]
            else: malformed["pkg-version"] = value
            fixture.write_bytes(plistlib.dumps(malformed))
            self.hook("preinstall", 1, reader)


class ArchiveTests(unittest.TestCase):
    def test_only_bounded_regular_members_are_copied(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory).resolve()
            for kind in ("valid", "parent", "absolute", "symlink", "hardlink", "fifo", "duplicate"):
                archive = work / (kind + ".tar.gz")
                with tarfile.open(archive, "w:gz") as output:
                    entry = tarfile.TarInfo({"parent": "../escape", "absolute": "/escape"}.get(kind, "bin/mmay"))
                    entry.size = 1
                    if kind in ("symlink", "hardlink", "fifo"):
                        entry.type = {"symlink": tarfile.SYMTYPE, "hardlink": tarfile.LNKTYPE,
                                      "fifo": tarfile.FIFOTYPE}[kind]
                        entry.linkname = "../../escape"
                        entry.size = 0
                    output.addfile(entry, io.BytesIO(b"x") if entry.isfile() else None)
                    if kind == "duplicate": output.addfile(entry, io.BytesIO(b"x"))
                destination = work / (kind + " output")
                if kind == "valid":
                    unpack_archive(archive, destination)
                    self.assertEqual((destination / "bin/mmay").read_bytes(), b"x")
                else:
                    with self.assertRaises(ValueError): unpack_archive(archive, destination)
                    self.assertFalse(destination.exists())
            self.assertFalse((work / "escape").exists())


class InstallGateTests(unittest.TestCase):
    def test_log_delta_never_reads_history_rotated_or_truncated_content(self):
        log = SMOKE.install_log
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory).resolve() / "install.log"
            path.write_bytes(b"Marimay installer: historical data must not appear\n")
            before = log.marker(path)
            with path.open("ab") as stream:
                stream.write(b"unrelated private system log\nMarimay installer: current guard reason\n")
            self.assertEqual(log.delta(before, path), ["Marimay installer: current guard reason"])
            path.rename(path.with_suffix(".old"))
            path.write_bytes(b"Marimay installer: rotated data must not appear\n")
            with self.assertRaisesRegex(ValueError, "rotated or was truncated"):
                log.delta(before, path)
            before = log.marker(path)
            path.write_bytes(b"short\n")
            with self.assertRaisesRegex(ValueError, "rotated or was truncated"):
                log.delta(before, path)
            before = log.marker(path)
            with path.open("ab") as stream:
                stream.write(b"Marimay installer: excluded beyond bound\n" + b"x" * 128
                             + b"\nMarimay installer: bounded current reason\n")
            with mock.patch.object(log, "MAX_DELTA", 64):
                self.assertEqual(log.delta(before, path), ["Marimay installer: bounded current reason"])
            lines = (b"package_script_service[42]: Running script org.marimay.mmay\n"
                     b"package_script_service[42]: ./postinstall: missing: unbound variable\n"
                     b"package_script_service[42]: [stderr] installer-common.sh: line 12: unbound variable\n"
                     b"package_script_service[42]: [stderr] find: own-prefix: permission denied\n"
                     b"package_script_service[99]: ./postinstall: unrelated private failure\n")
            filtered = log.own_lines(lines)
            self.assertEqual(len(filtered), 4)
            self.assertTrue(any("unbound variable" in line for line in filtered))
            self.assertTrue(any("installer-common.sh: line 12" in line for line in filtered))
            self.assertTrue(any("find: own-prefix" in line for line in filtered))
            self.assertFalse(any("unrelated private" in line for line in filtered))
            later = b"\n".join(b"PackageKit org.marimay.mmay script failed summary" for _ in range(20))
            preferred = log.own_lines(b"Marimay installer: original guard reason\n" + later)
            self.assertEqual(len(preferred), 8)
            self.assertEqual(preferred[0], "Marimay installer: original guard reason")

    def test_log_permission_fallback_is_ci_guarded_fixed_and_bounded(self):
        before = {"device": 1, "inode": 2, "size": 300}
        result = subprocess.CompletedProcess([], 0, b'["Marimay installer: scoped reason"]', b"")
        with mock.patch.object(SMOKE, "require_ci") as guard, \
                mock.patch.object(SMOKE.install_log, "delta", side_effect=PermissionError), \
                mock.patch.object(SMOKE.subprocess, "run", return_value=result) as run:
            self.assertEqual(SMOKE.current_install_log("delta", before), ["Marimay installer: scoped reason"])
        guard.assert_called_once_with(True)
        arguments = run.call_args.args[0]
        self.assertEqual(arguments[:3], ["/usr/bin/sudo", "-n", "--preserve-env=GITHUB_ACTIONS,RUNNER_OS"])
        self.assertEqual(arguments[4], str(Path(SMOKE.install_log.__file__).resolve()))
        self.assertEqual(arguments[5:], ["delta", "1", "2", "300"])
        with mock.patch.object(SMOKE, "require_ci", side_effect=ValueError("not CI")), \
                mock.patch.object(SMOKE.subprocess, "run") as run, self.assertRaises(ValueError):
            SMOKE.current_install_log("delta", before)
        run.assert_not_called()

    def test_failed_upgrade_reports_only_owned_state_and_reraises_original(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory).resolve()
            candidate = work / "candidate"
            prefix(candidate)
            previous = work / "previous"
            previous.mkdir()
            (previous / "changed").write_bytes(b"new contents must not be reported")
            old_version = "0.0.0-alpha.ci-old"
            wrapper = work / "wrapper"
            wrapper.write_bytes(SMOKE.launcher(old_version))
            old_files = {"missing": b"old private contents", "changed": b"old private contents"}
            original = AssertionError("original Installer failure")
            with mock.patch.object(SMOKE, "current_install_log", side_effect=[
                    {"device": 1, "inode": 2, "size": 300}, ["Marimay installer: scoped reason"]]), \
                    mock.patch.object(SMOKE, "sudo", side_effect=original), \
                    mock.patch.object(SMOKE.sys, "stderr", new_callable=io.StringIO) as stderr, \
                    self.assertRaises(AssertionError) as caught:
                SMOKE.install_upgrade(work / "own.pkg", work, candidate=candidate,
                    previous=previous, previous_files=old_files, wrapper=wrapper,
                    old_version=old_version, version=VERSION, arch=ARCH)
            self.assertIs(caught.exception, original)
            state = json.loads(stderr.getvalue().split("failure state: ", 1)[1])
            self.assertEqual(state["candidateValidation"], "ok")
            self.assertEqual(state["previousMissing"], ["missing"])
            self.assertEqual(state["previousChanged"], ["changed"])
            self.assertTrue(state["wrapperUnchanged"])
            self.assertEqual(state["ownScriptLogDelta"], ["Marimay installer: scoped reason"])
            self.assertNotIn("contents", stderr.getvalue())

    def test_installer_failure_reports_only_bounded_own_script_diagnostics(self):
        arguments = ["/usr/bin/sudo", "-n", "/usr/sbin/installer", "-pkg", "own.pkg", "-target", "/"]
        other = "unrelated system log must not be reported"
        own = ["syslog prefix Marimay installer: reason %d " % index + "x" * 600
               for index in range(12)]
        result = subprocess.CompletedProcess(arguments, 23,
            (other + "\n" + "\n".join(own)).encode(), (other + "\n").encode())
        with mock.patch.object(SMOKE.subprocess, "run", return_value=result), \
                self.assertRaisesRegex(AssertionError, "Installer command failed: exit=23") as caught:
            SMOKE.command(arguments, Path("/unused"))
        message = str(caught.exception)
        self.assertNotIn(other, message)
        self.assertNotIn("syslog prefix", message)
        details = json.loads(message.split("own-script=", 1)[1])
        self.assertEqual(len(details), 8)
        self.assertTrue(all(len(line) == 512 for line in details))
        self.assertTrue(details[0].startswith("Marimay installer: reason 4 "))
        with mock.patch.object(SMOKE.subprocess, "run", return_value=result):
            self.assertIs(SMOKE.command(arguments, Path("/unused"), expected=None), result)
        successful = subprocess.CompletedProcess(arguments, 0, b"", b"")
        with mock.patch.object(SMOKE.subprocess, "run", return_value=successful), \
                self.assertRaisesRegex(AssertionError, "collision was accepted"):
            SMOKE.command(arguments, Path("/unused"), expected=None)

    def test_installer_invocations_capture_parsable_current_invocation_logs(self):
        with mock.patch.object(SMOKE, "command") as run:
            SMOKE.sudo(["/usr/sbin/installer", "-pkg", "own.pkg", "-target", "/"],
                       Path("/unused"), expected=None)
        run.assert_called_once_with(["/usr/bin/sudo", "-n", "/usr/sbin/installer", "-dumplog",
            "-verboseR", "-pkg", "own.pkg", "-target", "/"], Path("/unused"), None, None)

    def test_installer_failure_fallback_is_scoped_to_own_package_errors(self):
        arguments = ["/usr/bin/sudo", "-n", "/usr/sbin/installer", "-pkg", "own.pkg"]
        own = ["PackageKit: org.marimay.mmay/Scripts/preinstall: ERROR cannot load helper",
               "PackageKit: org.marimay.ci.receipt-probe script FAILED status 127"]
        excluded = ["PackageKit: unrelated.identifier script failed with private data",
                    "PackageKit: org.marimay.mmay installed successfully",
                    "unrelated system error must not be reported"]
        result = subprocess.CompletedProcess(arguments, 1, b"", "\n".join(own + excluded).encode())
        with mock.patch.object(SMOKE.subprocess, "run", return_value=result), \
                self.assertRaisesRegex(AssertionError, "Installer command failed") as caught:
            SMOKE.command(arguments, Path("/unused"))
        details = json.loads(str(caught.exception).split("own-script=", 1)[1])
        self.assertEqual(details, own)
        self.assertTrue(all(line not in str(caught.exception) for line in excluded))

    def test_receipt_preflight_requires_explicit_hosted_mac_consent_before_work(self):
        cases = ((False, {"GITHUB_ACTIONS": "true", "RUNNER_OS": "macOS"}, "Darwin"),
                 (True, {}, "Darwin"),
                 (True, {"GITHUB_ACTIONS": "true", "RUNNER_OS": "macOS"}, "Linux"))
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory).resolve() / "must not exist"
            for allowed, environment, host in cases:
                arguments = ["test-macos-install", "--receipt-preflight", "--arch", ARCH,
                             "--work", str(work)] + (["--allow-system-install"] if allowed else [])
                with self.subTest(allowed=allowed, host=host), \
                        mock.patch.dict(SMOKE.os.environ, environment, clear=True), \
                        mock.patch.object(SMOKE.platform, "system", return_value=host), \
                        mock.patch.object(SMOKE.sys, "argv", arguments), \
                        mock.patch.object(SMOKE, "command") as run, \
                        mock.patch.object(SMOKE, "sudo") as sudo, \
                        self.assertRaises(ValueError):
                    SMOKE.main()
                self.assertFalse(work.exists())
                run.assert_not_called()
                sudo.assert_not_called()

    def test_receipt_preflight_uses_isolated_actual_installer_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory).resolve() / "synthetic preflight Мир"
            version = SMOKE.previous_version(work)
            next_version = version.replace("0.0.0-", "0.1.0-", 1)
            location = f"/usr/local/lib/marimay-ci-receipt-probe/{version}"
            with mock.patch.object(SMOKE, "require_ci") as guard, \
                    mock.patch.object(SMOKE, "receipt_absent") as absent, \
                    mock.patch.object(SMOKE, "command") as run, \
                    mock.patch.object(SMOKE, "sudo") as sudo, \
                    mock.patch.object(SMOKE, "probe_snapshot", side_effect=["old", "new", "old"]) as snapshots, \
                    mock.patch("builtins.print"):
                SMOKE.receipt_preflight(work)
            guard.assert_called_once_with(True)
            self.assertEqual(absent.call_args_list, [
                mock.call(SMOKE.PROBE_IDENTIFIER, SMOKE.REPOSITORY),
                mock.call(SMOKE.receipt_identifier(version, SMOKE.PROBE_IDENTIFIER), SMOKE.REPOSITORY),
                mock.call(SMOKE.receipt_identifier(next_version, SMOKE.PROBE_IDENTIFIER), SMOKE.REPOSITORY)])
            self.assertEqual(run.call_count, 2)
            self.assertEqual(sudo.call_count, 2)
            for index, value in enumerate((version, next_version)):
                arguments = run.call_args_list[index].args[0]
                self.assertEqual(arguments[0], "/usr/bin/pkgbuild")
                self.assertEqual(arguments[arguments.index("--identifier") + 1],
                                 SMOKE.receipt_identifier(value, SMOKE.PROBE_IDENTIFIER))
                self.assertEqual(arguments[arguments.index("--version") + 1], value)
                self.assertEqual(arguments[arguments.index("--install-location") + 1],
                                 f"/usr/local/lib/marimay-ci-receipt-probe/{value}")
                self.assertNotIn("--scripts", arguments)
                self.assertEqual(sudo.call_args_list[index], mock.call(["/usr/sbin/installer", "-pkg",
                    work / (value + ".pkg"), "-target", "/"], work))
                self.assertEqual((work / ("probe payload " + value) / "receipt-probe").read_bytes(),
                                 b"Marimay CI receipt probe\n")
            self.assertEqual(snapshots.call_args_list,
                             [mock.call(version, work), mock.call(next_version, work), mock.call(version, work)])

    def test_receipt_preflight_cli_routes_without_release_assets_and_checks_arch(self):
        arguments = ["test-macos-install", "--receipt-preflight", "--arch", ARCH,
                     "--work", "/unused-preflight", "--allow-system-install"]
        environment = {"GITHUB_ACTIONS": "true", "RUNNER_OS": "macOS"}
        for host_arch in (ARCH, "x86_64"):
            with mock.patch.dict(SMOKE.os.environ, environment, clear=True), \
                    mock.patch.object(SMOKE.platform, "system", return_value="Darwin"), \
                    mock.patch.object(SMOKE.platform, "machine", return_value=host_arch), \
                    mock.patch.object(SMOKE.sys, "argv", arguments), \
                    mock.patch.object(SMOKE, "receipt_preflight") as preflight:
                if host_arch == ARCH:
                    SMOKE.main()
                    preflight.assert_called_once_with(Path("/unused-preflight"))
                else:
                    with self.assertRaisesRegex(ValueError, "architecture"):
                        SMOKE.main()
                    preflight.assert_not_called()

    def test_receipt_preflight_refuses_existing_work_or_receipt_before_install(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory).resolve() / "preflight"
            with mock.patch.object(SMOKE, "require_ci"), \
                    mock.patch.object(SMOKE, "receipt_absent", side_effect=ValueError("existing receipt")), \
                    mock.patch.object(SMOKE, "command") as run, \
                    mock.patch.object(SMOKE, "sudo") as sudo, self.assertRaises(ValueError):
                SMOKE.receipt_preflight(work)
            self.assertFalse(work.exists())
            run.assert_not_called()
            sudo.assert_not_called()
            work.mkdir()
            with mock.patch.object(SMOKE, "require_ci"), \
                    mock.patch.object(SMOKE, "receipt_absent") as absent, \
                    mock.patch.object(SMOKE, "sudo") as sudo, self.assertRaises(ValueError):
                SMOKE.receipt_preflight(work)
            absent.assert_not_called()
            sudo.assert_not_called()

    def test_receipt_mismatch_reports_only_queried_identity_fields(self):
        import plistlib
        fields = {"pkgid": SMOKE.receipt_identifier("0.0.0-alpha.ci-fixture"), "pkg-version": "0.0.0",
                  "install-location": "usr/local/lib/foreign/0.0.0-alpha.ci-fixture",
                  "volume": "/", "unrelated-system-metadata": "must not report"}
        result = subprocess.CompletedProcess([], 0, plistlib.dumps(fields), b"")
        with mock.patch.object(SMOKE, "command", return_value=result) as command, \
                self.assertRaisesRegex(AssertionError, "Installer receipt identity mismatch") as caught:
            SMOKE.receipt_snapshot("0.0.0-alpha.ci-fixture", Path("/unused"))
        command.assert_called_once()
        self.assertIn('"pkg-version"', str(caught.exception))
        self.assertIn('"install-location"', str(caught.exception))
        self.assertNotIn("unrelated-system-metadata", str(caught.exception))
        self.assertNotIn("must not report", str(caught.exception))

    def test_receipt_identity_requires_exact_volume_relative_location(self):
        import plistlib
        location = "/usr/local/lib/marimay/" + VERSION
        fields = {"pkgid": SMOKE.IDENTIFIER, "pkg-version": VERSION,
                  "install-location": location[1:], "volume": "/"}
        result = subprocess.CompletedProcess([], 0, plistlib.dumps(fields), b"")
        with mock.patch.object(SMOKE, "command", return_value=result):
            self.assertEqual(SMOKE.receipt_identity(SMOKE.IDENTIFIER, VERSION, location,
                                                  Path("/unused")), fields)
        for value in (location, location[1:] + "/", location[1:].replace("/lib/", "/lib//"),
                      location[1:].replace("/marimay/", "/foreign/"),
                      location[1:].replace("/marimay/", "/../marimay/"),
                      location[1:].replace("/marimay/", "\\marimay/"), "", None, 7):
            changed = dict(fields)
            if value is None: del changed["install-location"]
            else: changed["install-location"] = value
            result = subprocess.CompletedProcess([], 0, plistlib.dumps(changed), b"")
            with self.subTest(location=value), mock.patch.object(SMOKE, "command", return_value=result), \
                    self.assertRaisesRegex(AssertionError, "Installer receipt identity mismatch"):
                SMOKE.receipt_identity(SMOKE.IDENTIFIER, VERSION, location, Path("/unused"))

    def test_previous_fixture_registers_real_receipt_with_installer(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory).resolve()
            version = "0.0.0-alpha.ci-fixture"
            with mock.patch.object(SMOKE, "command") as run, \
                    mock.patch.object(SMOKE, "sudo") as sudo, \
                    mock.patch.object(SMOKE, "receipt_snapshot", return_value="registered") as receipt:
                self.assertEqual(SMOKE.install_previous_fixture(work, version), "registered")
            arguments = run.call_args.args[0]
            self.assertEqual(arguments[0], "/usr/bin/pkgbuild")
            self.assertNotIn("--scripts", arguments)
            self.assertEqual(arguments[arguments.index("--identifier") + 1], SMOKE.receipt_identifier(version))
            self.assertEqual(arguments[arguments.index("--install-location") + 1],
                             f"/usr/local/lib/marimay/{version}")
            sudo.assert_called_once_with(["/usr/sbin/installer", "-pkg",
                work / "synthetic previous.pkg", "-target", "/"], work)
            receipt.assert_called_once_with(version, work)
            self.assertFalse((work / "synthetic previous prefix/unrelated user note").exists())

    def test_local_or_incompletely_enabled_execution_is_refused(self):
        for allowed, environment, host in ((False, {"GITHUB_ACTIONS": "true", "RUNNER_OS": "macOS"}, "Darwin"),
                (True, {}, "Darwin"), (True, {"GITHUB_ACTIONS": "true", "RUNNER_OS": "macOS"}, "Linux")):
            with self.subTest(allowed=allowed, environment=environment, host=host), \
                    mock.patch.dict(SMOKE.os.environ, environment, clear=True), \
                    mock.patch.object(SMOKE.platform, "system", return_value=host), \
                    self.assertRaises(ValueError):
                SMOKE.require_ci(allowed)

    def test_ordinary_cli_execution_stops_before_reading_assets_or_creating_work(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            result = subprocess.run([sys.executable, str(TOOL.with_name("test-macos-install")),
                "--package", str(root / "missing.pkg"), "--archive", str(root / "missing.tar.gz"),
                "--version", VERSION, "--arch", ARCH, "--work", str(root / "must not exist"),
                "--evidence-output", str(root / f"macos-verification-{ARCH}.json")], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"restricted to explicitly enabled ephemeral macOS CI", result.stderr)
            self.assertEqual(list(root.iterdir()), [])

    def test_smoke_uses_actual_wrapper_and_rejects_build_tree_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            tool = Path("/usr/local/bin/mmay")
            identity = {"schema": "marimay.version/2", "compiler": VERSION, "runtime": VERSION,
                "host": {"system": "Darwin", "processor": ARCH},
                "standardLibrary": {"version": VERSION},
                "release": {"channel": "experimental-alpha", "claim": "local-checker-backed-slice",
                            "publicationReady": False}}
            doctor = {"ok": True, "layout": "installed"}
            observed = []
            def run(arguments, cwd, **kwargs):
                observed.append(arguments)
                if arguments[0] == sys.executable:
                    self.assertEqual(arguments[2], tool)
                    output = b""
                elif arguments[1] == "version": output = json.dumps(identity).encode()
                elif arguments[1] == "doctor": output = json.dumps(doctor).encode()
                else: output = b"test result: ok. 1 passed; 0 failed\n"
                return subprocess.CompletedProcess(arguments, 0, output, b"")
            with mock.patch.object(SMOKE, "command", side_effect=run):
                SMOKE.verify_tool(tool, root / "good", VERSION, ARCH)
                self.assertTrue(any(args[1] == "test" for args in observed if args[0] == tool))
                doctor["layout"] = "build-tree"
                with self.assertRaisesRegex(AssertionError, "installed prefix"):
                    SMOKE.verify_tool(tool, root / "bad", VERSION, ARCH)

    def test_evidence_binds_only_completed_checks_and_unchanged_asset_bytes(self):
        with tempfile.TemporaryDirectory(prefix="mmay evidence ") as directory:
            root = Path(directory).resolve()
            package, archive = root / "package.pkg", root / "archive.tar.gz"
            package.write_bytes(b"synthetic package")
            archive.write_bytes(b"synthetic archive")
            before = {"package": SMOKE.asset(package), "archive": SMOKE.asset(archive)}
            checks = dict.fromkeys(SMOKE.CHECKS, True)
            output = root / f"macos-verification-{ARCH}.json"
            revision = "1" * 40
            result = subprocess.CompletedProcess([], 0, (revision + "\n").encode(), b"")
            with mock.patch.object(SMOKE, "command", return_value=result):
                SMOKE.write_evidence(output, VERSION, ARCH, package, archive, before, checks)
            verify_checksum(output)
            record = json.loads(output.read_bytes())
            self.assertEqual(record["schema"], "marimay.macos-install-verification/1")
            self.assertEqual(record["sourceRevision"], revision)
            self.assertEqual(record["package"], before["package"])
            self.assertEqual(record["archive"], before["archive"])
            self.assertEqual(record["checks"], checks)
            self.assertNotIn(str(root).encode(), output.read_bytes())
            output.unlink()
            output.with_name(output.name + ".sha256").unlink()
            with self.assertRaises(AssertionError):
                SMOKE.write_evidence(output, VERSION, ARCH, package, archive, before,
                                     {**checks, "installer": False})
            with self.assertRaises(AssertionError):
                SMOKE.write_evidence(output, VERSION, ARCH, package, archive, before,
                                     {**checks, "installer": 1})
            self.assertFalse(output.exists())
            package.write_bytes(b"changed package")
            with self.assertRaises(AssertionError):
                SMOKE.write_evidence(output, VERSION, ARCH, package, archive, before, checks)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
