from pathlib import Path
import hashlib
import json
import subprocess
import sys
import tempfile
import unittest

PREPARER = Path(sys.argv.pop(1)).resolve()


def digest(data):
    return hashlib.sha256(data).hexdigest()


class PinnedInputs(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / 'source'
        self.output = self.root / 'output'
        self.source.mkdir()
        (self.source / 'sljit_src').mkdir()
        self.original = b'head\nold\nnegate\n'
        self.expected = b'head\nnew\nguard\nnegate\n'
        files = {'LICENSE': b'fixture license\n',
                 'sljit_src/sljitLir.c': b'fixture\n',
                 'sljit_src/sljitNativeARM_64.c': self.original}
        for name, data in files.items():
            (self.source / name).write_bytes(data)
        self.patch = self.root / 'fix.patch'
        self.patch.write_text('--- a/sljitNativeARM_64.c\n+++ b/sljitNativeARM_64.c\n'
                              '@@ -2 +2 @@\n-old\n+new\n'
                              '@@ -2,0 +3 @@\n+guard\n')
        self.config = self.root / 'config.h'
        self.config.write_text('owned configuration\n')
        self.manifest = self.root / 'manifest.json'
        self.metadata = {'commit': '0' * 40,
                         'files_sha256': {name: digest(data) for name, data in files.items()},
                         'arm64_patch_sha256': digest(self.patch.read_bytes()),
                         'patched_arm64_sha256': digest(self.expected)}
        self.save_manifest()

    def save_manifest(self):
        self.manifest.write_text(json.dumps(self.metadata))

    def run_prepare(self, output=None):
        return subprocess.run([sys.executable, str(PREPARER), '--source', str(self.source),
                               '--output', str(output or self.output), '--manifest', str(self.manifest),
                               '--config', str(self.config), '--patch', str(self.patch)],
                              capture_output=True, text=True)

    def assert_prepared(self):
        result = self.run_prepare()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.output / 'sljit_src/sljitNativeARM_64.c').read_bytes(), self.expected)
        self.assertEqual((self.output / 'sljit_src/sljitConfigPre.h').read_bytes(), self.config.read_bytes())
        marker = json.loads((self.output / '.mmay-sljit-inputs.json').read_text())
        self.assertEqual(marker['status'], 'prepared')
        self.assertEqual(marker['patched_arm64_sha256'], digest(self.expected))

    def test_repeat_and_insertion_after_original_line(self):
        self.assert_prepared()
        self.assert_prepared()
        self.assertEqual((self.source / 'sljit_src/sljitNativeARM_64.c').read_bytes(), self.original)

    def test_unlisted_upstream_config_cannot_shadow_owned_hooks(self):
        (self.source / 'sljit_src/sljitConfigPre.h').write_text('unlisted hooks\n')
        self.assert_prepared()

    def test_changed_input_rejected_and_recoverable(self):
        target = self.source / 'sljit_src/sljitNativeARM_64.c'
        target.write_bytes(b'changed\n')
        self.assertNotEqual(self.run_prepare().returncode, 0)
        target.write_bytes(self.original)
        self.assert_prepared()

    def test_changed_patch_and_result_digest_rejected(self):
        original_patch = self.patch.read_bytes()
        self.patch.write_bytes(original_patch + b'changed\n')
        self.assertNotEqual(self.run_prepare().returncode, 0)
        self.patch.write_bytes(original_patch)
        self.metadata['patched_arm64_sha256'] = '0' * 64
        self.save_manifest()
        self.assertNotEqual(self.run_prepare().returncode, 0)

    def test_foreign_output_and_source_preserved(self):
        self.output.mkdir()
        foreign = self.output / 'keep'
        foreign.write_bytes(b'untouched')
        self.assertNotEqual(self.run_prepare().returncode, 0)
        self.assertEqual(foreign.read_bytes(), b'untouched')
        self.assertNotEqual(self.run_prepare(self.source).returncode, 0)
        self.assertNotEqual(self.run_prepare(self.root).returncode, 0)
        self.assertEqual((self.source / 'sljit_src/sljitNativeARM_64.c').read_bytes(), self.original)

    def test_path_escape_rejected(self):
        self.metadata['files_sha256']['../escape'] = digest(b'escape')
        (self.root / 'escape').write_bytes(b'escape')
        self.save_manifest()
        self.assertNotEqual(self.run_prepare().returncode, 0)
        self.assertEqual((self.root / 'escape').read_bytes(), b'escape')


if __name__ == '__main__':
    unittest.main()
