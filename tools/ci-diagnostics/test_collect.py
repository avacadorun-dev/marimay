"""Exercise failure packets with rejected/damaged/missing native artifacts."""

from contextlib import redirect_stdout
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from collect import collect, native_references


class CaptureTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="mmay diagnostic тест ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        subprocess.run(["git", "-C", str(self.root), "-c", "user.name=Fixture",
                        "-c", "user.email=fixture@example.invalid", "commit",
                        "--allow-empty", "-qm", "Synthetic evidence fixture"], check=True)
        self.build = self.root / "build"
        logs = self.build / "Testing" / "Temporary"
        logs.mkdir(parents=True)
        self.log = logs / "LastTest.log"
        self.output = self.build / "diagnostics"
        (self.build / "mmay").write_bytes(b"synthetic compiler")
        (self.build / "CMakeCache.txt").write_text(
            "CMAKE_BUILD_TYPE:STRING=Debug\nPRIVATE_VALUE:STRING=do-not-collect\n")
        self.source = (self.root / "tests" / "cp38 fixture" / ".mmay" / "build" /
                       "native" / "debug" / ("a" * 64) / "program.c")
        self.source.parent.mkdir(parents=True)

    def capture(self):
        with redirect_stdout(io.StringIO()) as console:
            packet = collect(self.root, self.build, self.output)
        self.assertEqual(json.loads(console.getvalue()), packet)
        self.assertEqual(json.loads((self.output / "manifest.json").read_text()), packet)
        return packet

    def test_missing_declaration_preserves_exact_bytes_and_deduplicates_reports(self):
        name = b"mmay_g_c_4d3b87da860e433267f36fcc78f5b8c8_1"
        data = b"void keep(void) {\n(void)" + name + b";\n}\n\x00\xff"
        self.source.write_bytes(data)
        self.log.write_text(f"{self.source}:2:9: error: undeclared identifier\n" * 2)
        (self.log.parent / "LastTestsFailed.log").write_text("1:mmay_cp38_json\n")
        packet = self.capture()
        self.assertEqual(packet["capture_errors"], [])
        self.assertEqual(len(packet["native_sources"]), 1)
        native = packet["native_sources"][0]
        self.assertEqual(native["sha256"], hashlib.sha256(data).hexdigest())
        self.assertEqual(native["names_without_emitter_declaration"], [name.decode()])
        self.assertEqual(native["nul_bytes"], 1)
        self.assertEqual(native["reported_lines"], [2])
        self.assertEqual((self.output / "files" / self.source.relative_to(self.root)).read_bytes(), data)
        self.assertEqual(packet["cmake_cache"], {"CMAKE_BUILD_TYPE": "Debug"})

    def test_existing_declaration_is_not_reported_missing(self):
        name = "mmay_g_c_" + "b" * 32 + "_12"
        self.source.write_text(f"static const unsigned char {name}[1] = {{0}};\n"
                               f"void keep(void) {{ (void){name}; }}\n")
        self.log.write_text(f"{self.source}:2: unrelated error\n{self.source}:999: error\n")
        native = self.capture()["native_sources"][0]
        self.assertEqual(native["constant_declarations"], 1)
        self.assertEqual(native["names_without_emitter_declaration"], [])
        self.assertEqual(native["excerpts"][1]["lines"], [])

    def test_deleted_artifact_is_visible_and_logs_survive(self):
        self.log.write_text(f"{self.source}:1771:9: error\n")
        packet = self.capture()
        self.assertTrue(packet["capture_errors"])
        self.assertIn("capture_error", packet["native_sources"][0])
        self.assertEqual((self.output / "files" / self.log.relative_to(self.root)).read_bytes(),
                         self.log.read_bytes())

    def test_unrelated_paths_and_other_checkouts_are_not_collected(self):
        for path in (str(self.source).replace(str(self.root), str(self.root) + "-other"),
                     str(self.root / "credentials" / "program.c")):
            self.assertEqual(native_references(path + ":2: error", self.root), {})
        self.log.write_text("1:mmay_allocation failed; no generated C diagnostic\n")
        self.assertEqual(self.capture()["native_sources"], [])

    def test_symlink_cannot_collect_outside_checkout(self):
        with tempfile.TemporaryDirectory() as outside:
            secret = Path(outside) / "program.c"
            secret.write_text("private bytes")
            self.source.symlink_to(secret)
            self.log.write_text(f"{self.source}:1: error\n")
            packet = self.capture()
            self.assertTrue(packet["capture_errors"])
            self.assertFalse((self.output / "files" / self.source.relative_to(self.root)).exists())

    def test_missing_logs_and_existing_output_are_not_silently_accepted(self):
        self.assertIn("no CTest logs available", self.capture()["capture_errors"])
        with self.assertRaises(FileExistsError):
            self.capture()


if __name__ == "__main__":
    unittest.main()
