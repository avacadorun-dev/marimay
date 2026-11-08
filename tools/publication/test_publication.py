"""Regressions for publication privacy boundaries, using synthetic data."""
import gzip
import importlib.machinery
import importlib.util
from pathlib import Path
import struct
import unittest
import zlib

loader = importlib.machinery.SourceFileLoader('publication', str(Path(__file__).resolve().parents[1] / 'check-publication'))
spec = importlib.util.spec_from_loader(loader.name, loader)
publication = importlib.util.module_from_spec(spec)
loader.exec_module(publication)


def chunk(kind, payload):
    return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))


def png(kind, payload):
    return b'\x89PNG\r\n\x1a\n' + chunk(kind, payload) + chunk(b'IEND', b'')


class PublicationTest(unittest.TestCase):
    def test_release_permission_requires_real_complete_gates(self):
        root = Path(__file__).resolve().parents[2]
        workflow = (root / '.github/workflows/release.yml').read_text()
        label = '.github/workflows/release.yml'
        self.assertEqual(publication.workflow_issues(workflow, label), [])
        needs = '    needs: [prepare, security, build, verified-ci]'
        condition = "    if: github.event_name != 'pull_request' && needs.prepare.outputs.publish == 'true'"
        mutations = [
            workflow.replace(needs, '    needs: [prepare]\n    # ' + needs),
            workflow.replace(condition, '    if: true\n    # ' + condition),
            workflow.replace(condition, condition + '\n    if: true'),
            workflow.replace('permissions:\n  contents: read', '# permissions:\n  contents: read', 1),
            workflow.replace('      contents: read', '      contents: write', 1),
            workflow.replace('    permissions:\n      contents: write\n      actions: read', '    permissions: write-all'),
            workflow.replace('      actions: read', '      actions: write'),
            workflow.replace('    if: github.event_name', '    if: always() && github.event_name'),
        ]
        for number, changed in enumerate(mutations):
            with self.subTest(case=number):
                self.assertTrue(publication.workflow_issues(changed, label))
        self.assertTrue(publication.workflow_issues(workflow, '.github/workflows/unrelated.yml'))

    def test_synthetic_and_no_reply_addresses(self):
        for domain in ('example.invalid', 'service.example', 'users.noreply.github.com'):
            self.assertEqual(publication.inspect(('fixture' + '@' + domain).encode(), 'fixture'), [])
        self.assertTrue(publication.inspect(('person' + '@' + 'mail.test').encode(), 'fixture'))

    def test_credentials_and_private_endpoints(self):
        samples = [b'ghp' + b'_' + b'A' * 36, b'192' + b'.168.5.7',
                   b'https' + b'://service.internal/api', b'git' + b'@host:project.git']
        for data in samples:
            with self.subTest(data=data):
                self.assertTrue(publication.inspect(data, 'fixture'))

    def test_personal_paths_and_neutral_windows_fixture(self):
        for data in (b'/' + b'Users' + b'/developer/src', b'C:' + b'\\Users' + b'\\developer'):
            self.assertTrue(publication.inspect(data, 'fixture'))
        self.assertEqual(publication.inspect(b'C:/demo/x', 'fixture'), [])

    def test_full_gzip_input_is_scanned(self):
        data = b'x' * (33 * 1024 * 1024) + b' ghp' + b'_' + b'A' * 36
        self.assertTrue(publication.inspect(gzip.compress(data), 'fixture.gz'))
        self.assertTrue(publication.inspect(b'\x1f\x8bbroken', 'fixture.gz'))

    def test_png_pixels_are_not_text_but_metadata_is(self):
        private = b'/' + b'Users' + b'/developer'
        self.assertEqual(publication.inspect(png(b'IDAT', private), 'image'), [])
        self.assertTrue(publication.inspect(png(b'tEXt', b'Comment\0' + private), 'image'))
        self.assertTrue(publication.inspect(png(b'zTXt', b'Comment\0\0' + zlib.compress(private)), 'image'))
        self.assertTrue(publication.inspect(png(b'iTXt', b'Comment\0\1\0\0\0' + zlib.compress(private)), 'image'))
        self.assertTrue(publication.inspect(png(b'eXIf', b'fixture'), 'image'))
        self.assertTrue(publication.inspect(png(b'IDAT', b'') + private, 'image'))


if __name__ == '__main__':
    unittest.main()
