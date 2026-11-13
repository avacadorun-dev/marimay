"""Release boundaries: malformed inputs, failed gates and immutable publication."""
import contextlib
import hashlib
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest import mock

import assets
import freeze
from common import ReleaseError
import prepare
import publish
import verify_ci

VERSION = "0.2.0-alpha.4"
SHA = "d" * 40
REPOSITORY = "marimay-lang/marimay"
SOURCE = ('set(MMAY_COMPILER_VERSION "' + VERSION + '")\n'
          'set(MMAY_RELEASE_CHANNEL "experimental-alpha")\n'
          'set(MMAY_RELEASE_PUBLICATION_READY_JSON "false")\n')


def archive(path, entries):
    with tarfile.open(path, "w:gz") as output:
        for name, data in entries.items():
            member = tarfile.TarInfo(name)
            member.size = len(data)
            output.addfile(member, io.BytesIO(data))


def host_entries(arch):
    prefix = "share/mmay/" + VERSION + "/"
    metadata = {"schema": "marimay.version/2", "compiler": VERSION, "runtime": VERSION,
                "standardLibrary": {"version": VERSION},
                "host": {"system": "Darwin", "processor": arch},
                "release": {"channel": "experimental-alpha", "publicationReady": False,
                            "claim": "local-checker-backed-slice"}}
    entries = {"bin/mmay": b"fixture-binary", "include/mmay/native.h": b"fixture-header",
               prefix + "mmay-release-metadata.json": json.dumps(metadata).encode(),
               prefix + "provenance.json": json.dumps({"schema": "marimay.provenance/1",
                   "version": VERSION, "source": {"revision": SHA}}).encode()}
    for profile in ("debug", "release"):
        runtime = "lib/mmay/" + VERSION + "/native/" + profile + "/"
        entries[runtime + "runtime.a"] = profile.encode()
        entries[runtime + "mmay-native-runtime.manifest"] = b"fixture-manifest"
    entries[prefix + "checksums.sha256"] = "".join(
        hashlib.sha256(data).hexdigest() + "  " + name + "\n"
        for name, data in sorted(entries.items())).encode()
    return entries


def make_assets(root):
    for arch in ("arm64", "x86_64"):
        folder = root / arch
        folder.mkdir()
        host = folder / ("marimay-" + VERSION + "-darwin-" + arch + ".tar.gz")
        archive(host, host_entries(arch))
        (folder / ("marimay-" + VERSION + "-darwin-" + arch + ".pkg")).write_bytes(b"xar!" + b"fixture" * 8)
        source = folder / ("marimay-" + VERSION + "-source.tar.gz")
        archive(source, {"marimay-" + VERSION + "-source/README.md": b"source"})
    # A real source archive is identical for both host builders.
    source_name = "marimay-" + VERSION + "-source.tar.gz"
    (root / "x86_64" / source_name).write_bytes((root / "arm64" / source_name).read_bytes())
    for arch in ("arm64", "x86_64"):
        folder = root / arch
        evidence = {"schema": "marimay.macos-install-verification/1", "sourceRevision": SHA,
                    "version": VERSION, "arch": arch,
                    "checks": {key: True for key in (
                        "pkgutil-layout", "installer", "doctor-installed", "vm-run", "test-both",
                        "native-debug", "native-release", "archive-relocation",
                        "foreign-command-rejected", "same-version-rejected")}}
        for key, extension in (("package", ".pkg"), ("archive", ".tar.gz")):
            path = folder / ("marimay-" + VERSION + "-darwin-" + arch + extension)
            evidence[key] = {"name": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                             "bytes": path.stat().st_size}
        (folder / ("macos-verification-" + arch + ".json")).write_text(json.dumps(evidence))
    for path in list(root.rglob("*")):
        if path.is_file():
            path.with_name(path.name + ".sha256").write_text(
                hashlib.sha256(path.read_bytes()).hexdigest() + "  " + path.name + "\n")


class ReleasePreparationTest(unittest.TestCase):
    def test_pr_cannot_publish_and_tag_must_match_version(self):
        self.assertEqual(prepare.prepare(SOURCE, "pull_request", "untrusted", SHA)["publish"], "false")
        self.assertEqual(prepare.prepare(SOURCE, "push", "v" + VERSION, SHA)["publish"], "true")
        for tag in ("main", "v0.2.0-alpha.5", "v" + VERSION + "\noutput=bad", "../../other"):
            with self.subTest(tag=tag), self.assertRaises(ReleaseError):
                prepare.prepare(SOURCE, "workflow_dispatch", tag, SHA)

    def test_reject_false_claims_and_malformed_identity(self):
        for source in (SOURCE.replace("experimental-alpha", "cli-alpha"),
                       SOURCE.replace('"false"', '"true"'),
                       SOURCE.replace(VERSION, "0.2.0-alpha.04")):
            with self.subTest(source=source), self.assertRaises(ReleaseError):
                prepare.prepare(source, "push", "v" + VERSION, SHA)
        with self.assertRaises(ReleaseError):
            prepare.prepare(SOURCE, "push", "v" + VERSION, "main")


class ReleaseCiTest(unittest.TestCase):
    def test_complete_matrix_cannot_hide_failed_skipped_or_missing_jobs(self):
        jobs = [{"status": "completed", "conclusion": "success"} for _ in range(26)]
        self.assertTrue(verify_ci.complete_jobs({"total_count": 26, "jobs": jobs}))
        for conclusion in ("skipped", "failure", "cancelled", "timed_out", None):
            changed = [*jobs[:-1], {"status": "completed", "conclusion": conclusion}]
            self.assertFalse(verify_ci.complete_jobs({"total_count": 26, "jobs": changed}))
        self.assertFalse(verify_ci.complete_jobs({"total_count": 26, "jobs": jobs[:-1]}))

    def test_exact_trusted_workflow_identity(self):
        run = {"head_sha": SHA, "status": "completed", "conclusion": "success",
               "event": "push", "head_repository": {"full_name": REPOSITORY},
               "path": ".github/workflows/ci.yml"}
        self.assertEqual(verify_ci.successful_runs([run], REPOSITORY, SHA), [run])
        for key, value in (("head_sha", "a" * 40), ("path", "other.yml"),
                           ("head_repository", {"full_name": "foreign/fork"}),
                           ("conclusion", "failure"), ("event", "workflow_run"),
                           ("event", "pull_request")):
            with self.subTest(key=key):
                self.assertEqual(verify_ci.successful_runs([{**run, key: value}], REPOSITORY, SHA), [])


class ReleaseAssetsTest(unittest.TestCase):
    def test_frozen_host_assets_survive_distribution_regeneration(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            make_assets(root)
            destination = root / "verified"
            checked = freeze.freeze(root / "arm64", destination, VERSION, SHA, "arm64")
            self.assertEqual(len(checked), 8)
            for path in (root / "arm64").iterdir():
                path.write_bytes(b"regenerated by packaging test")
            self.assertEqual(len(assets.collect(destination, VERSION, SHA, ("arm64",))), 8)
            with self.assertRaises(ReleaseError):
                freeze.freeze(root / "x86_64", destination, VERSION, SHA, "x86_64")
            with self.assertRaises(ReleaseError):
                assets.collect(destination, VERSION, SHA)

    def test_freeze_rejects_missing_tampered_or_linked_install_proof(self):
        for mutation in ("missing", "checksum", "proof", "link"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as directory:
                root = Path(directory).resolve()
                make_assets(root)
                folder = root / "arm64"
                proof = folder / "macos-verification-arm64.json"
                if mutation == "missing":
                    proof.unlink()
                elif mutation == "checksum":
                    proof.write_bytes(b"tampered")
                elif mutation == "proof":
                    value = json.loads(proof.read_text())
                    value["sourceRevision"] = "a" * 40
                    proof.write_text(json.dumps(value))
                    proof.with_name(proof.name + ".sha256").write_text(
                        hashlib.sha256(proof.read_bytes()).hexdigest() + "  " + proof.name + "\n")
                else:
                    target = root / "proof"
                    proof.rename(target)
                    proof.symlink_to(target)
                with self.assertRaises((ReleaseError, ValueError)):
                    freeze.freeze(folder, root / "verified", VERSION, SHA, "arm64")

    def test_alias_arch_claim_and_install_proof_cannot_be_forged_by_checksums(self):
        for mutation in ("alias", "case", "arch", "claim"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / ("marimay-" + VERSION + "-darwin-arm64.tar.gz")
                entries = host_entries("arm64")
                prefix = "share/mmay/" + VERSION + "/"
                if mutation == "alias":
                    entries["bin/./mmay"] = b"changed"
                elif mutation == "case":
                    entries["bin/MMAY"] = b"changed"
                else:
                    data = json.loads(entries[prefix + "mmay-release-metadata.json"])
                    if mutation == "arch":
                        data["host"]["processor"] = "x86_64"
                    else:
                        data["release"]["claim"] = "wrong"
                    entries[prefix + "mmay-release-metadata.json"] = json.dumps(data).encode()
                entries.pop(prefix + "checksums.sha256")
                entries[prefix + "checksums.sha256"] = "".join(
                    hashlib.sha256(data).hexdigest() + "  " + name + "\n"
                    for name, data in sorted(entries.items())).encode()
                archive(path, entries)
                with self.assertRaises(ReleaseError):
                    assets.inspect_tar(path, VERSION, SHA)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            make_assets(root)
            path = root / "arm64" / "macos-verification-arm64.json"
            value = json.loads(path.read_text())
            value["package"]["sha256"] = "a" * 64
            path.write_text(json.dumps(value))
            path.with_name(path.name + ".sha256").write_text(
                hashlib.sha256(path.read_bytes()).hexdigest() + "  " + path.name + "\n")
            with self.assertRaises(ReleaseError):
                assets.collect(root, VERSION, SHA)

    def test_complete_inventory_and_consistent_source_duplicates(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            make_assets(root)
            self.assertEqual(len(assets.collect(root, VERSION, SHA)), 14)

    def test_malformed_artifacts_never_reach_publication(self):
        mutations = ("missing", "checksum", "foreign", "symlink", "source-duplicate", "pkg")
        for mutation in mutations:
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                make_assets(root)
                path = root / "arm64" / ("marimay-" + VERSION + "-darwin-arm64.pkg")
                if mutation == "missing":
                    path.unlink()
                elif mutation == "checksum":
                    path.with_name(path.name + ".sha256").write_text("bad\n")
                elif mutation == "foreign":
                    (root / "unexpected.txt").write_text("unowned")
                elif mutation == "symlink":
                    path.unlink()
                    path.symlink_to("missing")
                elif mutation == "source-duplicate":
                    (root / "arm64" / ("marimay-" + VERSION + "-source.tar.gz")).write_bytes(b"different")
                else:
                    path.write_bytes(b"not-a-package")
                    path.with_name(path.name + ".sha256").write_text(
                        hashlib.sha256(path.read_bytes()).hexdigest() + "  " + path.name + "\n")
                with self.assertRaises((ReleaseError, OSError)):
                    assets.collect(root, VERSION, SHA)

    def test_host_tampering_missing_runtime_and_wrong_provenance_are_rejected(self):
        for mutation in ("tamper", "runtime", "provenance", "traversal", "link"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / ("marimay-" + VERSION + "-darwin-arm64.tar.gz")
                entries = host_entries("arm64")
                if mutation == "tamper":
                    entries["bin/mmay"] = b"changed"
                elif mutation == "runtime":
                    del entries["lib/mmay/" + VERSION + "/native/debug/runtime.a"]
                elif mutation == "provenance":
                    entries["share/mmay/" + VERSION + "/provenance.json"] = b'{}'
                elif mutation == "traversal":
                    entries["../escape"] = b"bad"
                archive(path, entries)
                if mutation == "link":
                    with tarfile.open(path, "w:gz") as output:
                        member = tarfile.TarInfo("bin/mmay")
                        member.type = tarfile.SYMTYPE
                        member.linkname = "foreign"
                        output.addfile(member)
                with self.assertRaises(ReleaseError):
                    assets.inspect_tar(path, VERSION, SHA)


class ReleasePublicationTest(unittest.TestCase):
    def test_draft_uploads_verified_bytes_and_only_then_becomes_public(self):
        for failure in (None, "digest", "tag-race"):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                make_assets(root)
                paths = assets.collect(root, VERSION, SHA)
                notes = root.parent / (root.name + "-notes.md")
                try:
                    notes.write_text("Experimental alpha\n")
                    marker = "<!-- marimay-experimental-release:" + SHA + " -->"
                    draft = {"id": 1, "tag_name": "v" + VERSION, "draft": True,
                             "prerelease": True, "body": marker, "assets": []}
                    completed = {**draft, "assets": [{"name": path.name, "state": "uploaded",
                        "size": path.stat().st_size,
                        "digest": "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()}
                        for path in paths]}
                    if failure == "digest":
                        completed["assets"][0]["digest"] = "sha256:" + "a" * 64
                    response = [None, draft, completed]
                    if failure is None:
                        response.append({"html_url": "https://github.com/" + REPOSITORY + "/releases/tag/v" + VERSION})
                    identities = [None, ReleaseError("tag changed")] if failure == "tag-race" else [None, None]
                    with mock.patch("publish.api", side_effect=response) as api, \
                         mock.patch("publish.remote_identity", side_effect=identities), \
                         mock.patch("publish.subprocess.run") as process:
                        if failure:
                            with self.assertRaises(ReleaseError):
                                publish.publish(REPOSITORY, "v" + VERSION, VERSION, SHA, root, notes)
                            self.assertFalse(any(len(call.args) > 2 and call.args[2] == "PATCH"
                                                 for call in api.call_args_list))
                        else:
                            with contextlib.redirect_stdout(io.StringIO()):
                                publish.publish(REPOSITORY, "v" + VERSION, VERSION, SHA, root, notes)
                            self.assertEqual(api.call_args_list[-1].args[2], "PATCH")
                            self.assertFalse(api.call_args_list[-1].args[3]["draft"])
                        self.assertNotIn("--clobber", process.call_args.args[0])
                finally:
                    notes.unlink(missing_ok=True)

    @mock.patch("publish.collect", return_value=[])
    @mock.patch("publish.remote_identity")
    @mock.patch("publish.api", return_value={"draft": False})
    @mock.patch("publish.subprocess.run")
    def test_existing_public_assets_are_never_overwritten(self, process, api, identity, collect):
        with self.assertRaises(ReleaseError):
            publish.publish(REPOSITORY, "v" + VERSION, VERSION, SHA, Path("assets"), Path("notes"))
        process.assert_not_called()
        self.assertEqual(api.call_count, 1)

    @mock.patch("publish.collect", side_effect=ReleaseError("bad artifact"))
    @mock.patch("publish.api")
    def test_invalid_artifacts_do_not_create_a_draft(self, api, collect):
        with self.assertRaises(ReleaseError):
            publish.publish(REPOSITORY, "v" + VERSION, VERSION, SHA, Path("assets"), Path("notes"))
        api.assert_not_called()


if __name__ == "__main__":
    unittest.main()
