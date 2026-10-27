"""The AI image uses a different app offset; verify every flash boundary before USB access."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

path=Path(__file__).resolve().parents[1]/'scripts/orbit_ai_artifacts.py'
spec=importlib.util.spec_from_file_location('orbit_ai_artifacts',path)
artifacts=importlib.util.module_from_spec(spec);spec.loader.exec_module(artifacts)

class AiArtifactsTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.directory=Path(self.temp.name)
        table=b''.join(struct.pack('<HBBII16sI',0x50aa,1,0,address,size,name.encode(),0) for name,address,size in [
            ('nvs',0x9000,0x5000),('otadata',0xe000,0x2000),('ota_0',0x20000,0x3f0000),
            ('ota_1',0x410000,0x3f0000),('assets',0x800000,0x800000)])
        self.manifest={'format':1,'board':'orbit-clock-ai','files':[]}
        for address,(name,limit) in artifacts.LAYOUT.items():
            data=table if name=='partition-table.bin' else b'ESP32-test-image'
            (self.directory/name).write_bytes(data)
            self.manifest['files'].append({'offset':hex(address),'name':name,'size':len(data),'sha256':hashlib.sha256(data).hexdigest()})
        self.write_manifest()
    def write_manifest(self):
        (self.directory/'flash_manifest.json').write_text(json.dumps(self.manifest))
    def rejected(self):
        self.write_manifest()
        with self.assertRaises(RuntimeError):artifacts.validate(self.directory)
    def test_valid_images_never_write_existing_nvs(self):
        self.assertEqual([int(a,0) for a,p in artifacts.validate(self.directory)],sorted(artifacts.LAYOUT))
    def test_old_arduino_app_offset_rejected(self):
        self.manifest['files'][3]['offset']='0x10000';self.rejected()
    def test_nvs_image_and_duplicate_offsets_rejected(self):
        self.manifest['files'][0]['offset']='0x9000';self.rejected()
        self.manifest['files'][0]['offset']='0x8000';self.rejected()
    def test_missing_partition_rejected(self):
        self.manifest['files'].pop();self.rejected()
    def test_corrupt_bytes_rejected_before_serial_access(self):
        (self.directory/'orbit-clock-ai.bin').write_bytes(b'bad');self.rejected()
    def test_path_traversal_and_other_board_rejected(self):
        self.manifest['files'][0]['name']='../bootloader.bin';self.rejected()
        self.manifest['files'][0]['name']='bootloader.bin';self.manifest['board']='esp32-s3-touch-amoled-2.16';self.rejected()
    def test_wrong_partition_layout_rejected_even_with_valid_hash(self):
        path=self.directory/'partition-table.bin';data=bytearray(path.read_bytes());struct.pack_into('<I',data,8,0x4000);path.write_bytes(data)
        self.manifest['files'][1]['sha256']=hashlib.sha256(data).hexdigest();self.rejected()
    def test_oversized_bootloader_cannot_cross_partition_table(self):
        data=b'x'*(0x8000+1);(self.directory/'bootloader.bin').write_bytes(data)
        self.manifest['files'][0].update(size=len(data),sha256=hashlib.sha256(data).hexdigest());self.rejected()

if __name__=='__main__':unittest.main()
