from pathlib import Path
import io
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
from check_release_privacy import inspect_bytes, inspect_tree, inspect_zip


class ReleasePrivacyTests(unittest.TestCase):
    def test_compiled_binary_paths_and_private_key_are_detected(self):
        for secret in (b'/'.join((b'', b'Users', b'alice', b'project', b'file.cc')),
                       b'/'.join((b'', b'home', b'alice', b'build')),
                       b'/'.join((b'', b'private', b'var', b'folders', b'xy', b'testfolder')),
                       b'\\'.join((b'C:', b'Users', b'alice', b'project'))):
            self.assertIn('personal_absolute_path', inspect_bytes('firmware.bin', b'\0'+secret+b'\0'))
        pem = b'-----BEGIN '+b'OPENSSH PRIVATE KEY-----\n'+b'Q'*128+b'\n-----END '+b'OPENSSH PRIVATE KEY-----'
        self.assertIn('private_key', inspect_bytes('file.txt', pem))
        self.assertEqual(inspect_bytes('parser.bin', b'-----BEGIN '+b'PRIVATE KEY-----\0'), [])

    def test_wifi_examples_allowed_but_configured_values_rejected(self):
        self.assertEqual(inspect_bytes('clock_secrets.example.h', b'#define CLOCK_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"'), [])
        self.assertEqual(inspect_bytes('clock_config.h', b'#define CLOCK_WIFI_SSID ""'), [])
        self.assertIn('configured_wifi_literal', inspect_bytes('config.h', b'#define CLOCK_WIFI_PASSWORD "test-only-value"'))
        self.assertIn('private_or_generated_file', inspect_bytes('clock_secrets.h', b''))

    def test_backup_nvs_cache_and_unsafe_members_rejected(self):
        for name in ('work/anything', 'outputs/board-backups/file.bin', 'nvs.bin',
                     'source/build/file', 'firmware.elf', 'before-orbit.bin', '../file', '/file'):
            self.assertTrue(inspect_bytes(name, b''), name)
        self.assertIn('full_device_dump', inspect_bytes('unlabelled.bin', b'\0'*(16*1024*1024)))

    def test_nested_archives_are_scanned_and_values_not_exposed(self):
        inner = io.BytesIO()
        with zipfile.ZipFile(inner, 'w') as archive:
            archive.writestr('image.bin', b'prefix '+b'/'.join((b'', b'home', b'testperson', b'private suffix')))
        outer = io.BytesIO()
        with zipfile.ZipFile(outer, 'w') as archive:
            archive.writestr('nested.zip', inner.getvalue())
        findings = inspect_zip(outer.getvalue())
        self.assertIn({'file': 'nested.zip!image.bin', 'rule': 'personal_absolute_path'}, findings)
        self.assertTrue(all(item['rule'] == 'personal_absolute_path' for item in findings))
        self.assertNotIn('testperson', str(findings))

    def test_clean_tree_and_symlink_rejection(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root/'README.md').write_text('Public docs and relative paths.')
            self.assertTrue(inspect_tree(root)['passed'])
            (root/'linked').symlink_to(root/'README.md')
            self.assertFalse(inspect_tree(root)['passed'])


if __name__ == '__main__':
    unittest.main()
