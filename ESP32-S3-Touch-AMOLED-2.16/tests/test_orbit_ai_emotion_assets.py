"""Round-trip the real official resource format and preserve wake/font assets."""
from pathlib import Path
import importlib.util
import json
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
import generate_orbit_ai_assets as custom

PACKER = ROOT.parent/'work/xiaozhi-upstream/scripts/build_default_assets.py'
GIF = ROOT/'examples/esp-idf/06_OrbitClockAI/assets/screen-eyes.gif'


@unittest.skipUnless(PACKER.exists(), 'prepare pinned XiaoZhi first')
class EmotionAssetsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.temp = Path(self.temporary.name)
        spec = importlib.util.spec_from_file_location('official_packer', PACKER)
        self.packer = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.packer)

    def pack(self, resources, path):
        folder = self.temp/'input'
        folder.mkdir(exist_ok=True)
        for p in folder.iterdir():
            p.unlink()
        for name, data in resources.items():
            (folder/name).write_bytes(data)
        self.packer.pack_assets_simple(str(folder), str(self.temp/'include'), str(path), 'assets', 32)

    def base(self):
        index = {'version': 1, 'srmodels': 'srmodels.bin', 'text_font': 'font.bin',
                 'text_font_meta': {'charset': 'common', 'size': 20, 'bpp': 4, 'bundle': 'noto-v1'},
                 'emoji_collection': [{'name': 'neutral', 'file': 'old.png'},
                                      {'name': 'warning', 'file': 'old.png'}],
                 'skin': {'light': {'background_image': 'old.png', 'border_color': '#222222'}}}
        resources = {'index.json': json.dumps(index).encode(), 'srmodels.bin': bytes(range(256))*3,
                     'font.bin': b'font\0\xff\x12', 'old.png': b'old-image'}
        path = self.temp/'base.bin'
        self.pack(resources, path)
        return path, resources

    def test_all_emotions_share_one_gif_and_preserve_model_font_and_metadata(self):
        base, resources = self.base()
        output = self.temp/'out.bin'
        custom.generate(base, output, GIF, PACKER)
        actual = custom.unpack_assets(output.read_bytes())
        index = json.loads(actual['index.json'])
        self.assertEqual(actual[custom.GIF_NAME], GIF.read_bytes())
        for name in ('srmodels.bin', 'font.bin', 'old.png'):
            self.assertEqual(actual[name], resources[name])
        self.assertEqual(index['text_font_meta'], json.loads(resources['index.json'])['text_font_meta'])
        self.assertEqual(set(item['name'] for item in index['emoji_collection']), set(custom.EMOTIONS)|{'warning'})
        self.assertEqual({item['file'] for item in index['emoji_collection']}, {custom.GIF_NAME})
        for mode in ('light', 'dark'):
            self.assertEqual(index['skin'][mode]['background_color'], '#000000')
            self.assertEqual(index['skin'][mode]['text_color'], '#FFFFFF')
            self.assertNotIn('background_image', index['skin'][mode])
        self.assertEqual(index['skin']['light']['border_color'], '#222222')

    def test_regeneration_is_deterministic_and_in_place_safe(self):
        base, _ = self.base()
        custom.generate(base, base, GIF, PACKER)
        first = base.read_bytes()
        custom.generate(base, base, GIF, PACKER)
        self.assertEqual(first, base.read_bytes())

    def test_corrupt_or_truncated_container_rejected_without_replacing_output(self):
        base, _ = self.base()
        valid = base.read_bytes()
        output = self.temp/'out.bin'
        output.write_bytes(b'keep')
        damaged = bytearray(valid)
        damaged[-1] ^= 1
        for data in (b'', valid[:11], valid[:-1], damaged):
            base.write_bytes(data)
            with self.assertRaises(ValueError):
                custom.generate(base, output, GIF, PACKER)
            self.assertEqual(output.read_bytes(), b'keep')

    def test_unsafe_or_overlapping_resource_index_rejected(self):
        base, _ = self.base()
        valid = base.read_bytes()
        def repaired(data):
            struct.pack_into('<I', data, 4, sum(data[12:]) & 0xffff)
            return data
        unsafe = bytearray(valid)
        unsafe[12:44] = b'../x'.ljust(32, b'\0')
        with self.assertRaises(ValueError):
            custom.unpack_assets(repaired(unsafe))
        overlap = bytearray(valid)
        offset = struct.unpack_from('<I', overlap, 12+36)[0]
        struct.pack_into('<I', overlap, 12+custom.ENTRY.size+36, offset)
        with self.assertRaises(ValueError):
            custom.unpack_assets(repaired(overlap))

    def test_bad_or_oversize_gif_rejected_without_replacing_output(self):
        base, _ = self.base()
        output = self.temp/'out.bin'
        output.write_bytes(b'keep')
        bad = self.temp/'bad.gif'
        for data in (b'PNG', b'GIF89a'+struct.pack('<HH', 481, 180)+b'\0'*3):
            bad.write_bytes(data)
            with self.assertRaises(ValueError):
                custom.generate(base, output, bad, PACKER)
            self.assertEqual(output.read_bytes(), b'keep')


if __name__ == '__main__':
    unittest.main()
