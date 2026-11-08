"""Fail-closed checks for the shared application acceptance driver."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from mmay_acceptance import Application


class DriverTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="mmay проверка ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.app = Application(self.root, sys.executable)

    def command(self, code):
        return [sys.executable, "-c", code]

    def test_raw_bytes_cwd_and_argv(self):
        code = ("import os,sys; from pathlib import Path; "
                "assert Path.cwd().samefile(sys.argv[1]); "
                "sys.stdout.buffer.write(sys.stdin.buffer.read()); "
                "sys.stderr.buffer.write(b'\\x00\\xff')")
        engines = {name: self.command(code) for name in ("vm", "debug", "release")}
        observed = self.app.compare(engines, [str(self.root)], 0, data=b"\x00\xff\r\n")
        self.assertEqual(observed, (0, b"\x00\xff\r\n", b"\x00\xff"))

    def test_cwd_identity_accepts_alias_and_rejects_other_directory(self):
        # A child reports the physical cwd, which can have a different spelling
        # after symlink traversal or filesystem Unicode normalization.
        alias = self.root / "cwd-alias"
        try:
            alias.symlink_to(self.root, target_is_directory=True)
        except OSError:
            self.skipTest("directory symlinks are unavailable")
        code = ("import sys; from pathlib import Path; "
                "assert Path.cwd().samefile(sys.argv[1])")
        engines = {"vm": self.command(code)}
        self.app.compare(engines, [str(alias)], 0)
        other = self.root / "other"
        other.mkdir()
        with self.assertRaises(AssertionError):
            self.app.compare(engines, [str(other)], 0)

    def test_detects_each_observable_divergence(self):
        for changed in ("import sys; sys.exit(1)", "print('unexpected')",
                        "import sys; sys.stderr.write('unexpected')"):
            with self.subTest(changed=changed), self.assertRaises(AssertionError):
                self.app.compare({"vm": self.command("pass"),
                                  "native": self.command(changed)}, [], 0)

    def test_rejects_equally_wrong_exit(self):
        command = self.command("raise SystemExit(2)")
        with self.assertRaises(AssertionError):
            self.app.compare({"vm": command, "native": command}, [], 0)

    def test_prepares_and_verifies_every_engine(self):
        file = self.root / "state"
        seen = []
        command = self.command("from pathlib import Path; p=Path('state'); "
                               "assert p.read_bytes()==b'old'; p.write_bytes(b'new')")

        def verify(engine, result):
            self.assertEqual(file.read_bytes(), b"new")
            seen.append(engine)

        self.app.compare({name: command for name in ("vm", "debug", "release")}, [], 0,
                         prepare=lambda: file.write_bytes(b"old"), verify=verify)
        self.assertEqual(seen, ["vm", "debug", "release"])

    def test_app_checks_reject_equally_wrong_outputs(self):
        def verify(engine, result):
            self.assertEqual(result.stdout, b"expected")

        with self.assertRaises(AssertionError):
            self.app.compare({"vm": self.command("print('wrong')")}, [], 0, verify=verify)

    def test_empty_engine_set_and_disabled_assertions_are_rejected(self):
        with self.assertRaises(ValueError):
            self.app.compare({}, [], 0)
        module = str(Path(__file__).resolve().parent)
        result = subprocess.run([sys.executable, "-O", "-c",
                                 "import sys; sys.path.insert(0,sys.argv[1]); "
                                 "from mmay_acceptance import Application; "
                                 "Application('.',sys.executable)", module], capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"acceptance requires enabled Python assertions", result.stderr)

    def test_timeout_is_a_failure(self):
        self.app.timeout = 0.1
        with self.assertRaises(subprocess.TimeoutExpired):
            self.app.compare({"vm": self.command("import time; time.sleep(10)")}, [], 0)


if __name__ == "__main__":
    unittest.main()
