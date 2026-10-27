#!/usr/bin/env python3
"""Add the recorded eyes GIF to XiaoZhi's official mmap assets container.

The index follows https://github.com/78/xiaozhi-assets-generator. Reuse the
pinned firmware's packer and preserve its wake models, fonts and font metadata.
"""
from pathlib import Path
import argparse
import importlib.util
import json
import struct
import tempfile

EMOTIONS = ('neutral', 'happy', 'laughing', 'funny', 'sad', 'angry', 'crying',
            'loving', 'embarrassed', 'surprised', 'shocked', 'thinking', 'winking',
            'cool', 'relaxed', 'delicious', 'kissy', 'confident', 'sleepy', 'silly',
            'confused')
GIF_NAME = 'screen-eyes.gif'
MAX_SIZE = 0x800000
ENTRY = struct.Struct('<32sIIHH')


def unpack_assets(data):
    if len(data) < 12:
        raise ValueError('资源头不完整')
    count, checksum, length = struct.unpack_from('<III', data)
    if not count or count > 1024 or length != len(data)-12:
        raise ValueError('资源文件数量或长度不正确')
    payload = data[12:]
    if sum(payload) & 0xffff != checksum or count*ENTRY.size > length:
        raise ValueError('资源校验失败')
    table_size = count*ENTRY.size
    result = {}
    occupied = []
    for i in range(count):
        raw_name, size, offset, _, _ = ENTRY.unpack_from(payload, i*ENTRY.size)
        name = raw_name.split(b'\0')[0].decode('ascii')
        start = table_size+offset
        end = start+2+size
        if (not name or name in result or Path(name).name != name or '\\' in name
                or name in ('.', '..') or start < table_size or end > length
                or payload[start:start+2] != b'ZZ'):
            raise ValueError('资源索引无效')
        if any(start < other_end and end > other_start for other_start, other_end in occupied):
            raise ValueError('资源范围重叠')
        occupied.append((start, end))
        result[name] = payload[start+2:end]
    if 'index.json' not in result:
        raise ValueError('缺少 index.json')
    return result


def generate(base, output, gif, packer):
    resources = unpack_assets(Path(base).read_bytes())
    index = json.loads(resources['index.json'])
    if index.get('version') != 1:
        raise ValueError('资源索引版本不支持')
    gif_data = Path(gif).read_bytes()
    if len(gif_data) < 13 or gif_data[:6] not in (b'GIF87a', b'GIF89a'):
        raise ValueError('表情不是有效的 GIF')
    width, height = struct.unpack_from('<HH', gif_data, 6)
    if not (0 < width <= 480 and 0 < height <= 480):
        raise ValueError('GIF 尺寸超出屏幕')
    # All known emotions share one file; no duplicated animation bytes.
    names = list(dict.fromkeys([*EMOTIONS, *(
        item['name'] for item in index.get('emoji_collection', [])
        if isinstance(item, dict) and isinstance(item.get('name'), str))]))
    index['emoji_collection'] = [{'name': name, 'file': GIF_NAME} for name in names]
    index['chip_model'] = 'esp32s3'
    index['display_config'] = {'width': 480, 'height': 480,
                               'monochrome': False, 'color': 'RGB565'}
    skin = index.setdefault('skin', {})
    for name in ('light', 'dark'):
        theme = skin.setdefault(name, {})
        theme.update(text_color='#FFFFFF', background_color='#000000')
        theme.pop('background_image', None)
    resources['index.json'] = json.dumps(index, ensure_ascii=False, separators=(',', ':')).encode()
    resources[GIF_NAME] = gif_data
    spec = importlib.util.spec_from_file_location('xiaozhi_official_assets_packer', packer)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent) as directory:
        staging = Path(directory)
        assets = staging/'assets'
        assets.mkdir()
        for name, data in resources.items():
            (assets/name).write_bytes(data)
        packed = staging/'assets.bin'
        module.pack_assets_simple(str(assets), str(staging/'include'), str(packed), 'assets', 32)
        data = packed.read_bytes()
        if len(data) > MAX_SIZE:
            raise ValueError('资源超出 8MB 分区')
        if unpack_assets(data) != resources:
            raise ValueError('资源打包回读不一致')
        packed.replace(output)
    print(f'自定义眼睛表情：{width}×{height}，{len(names)} 个情绪共用 GIF；'
          f'资源 {len(data)} / {MAX_SIZE} 字节；保留唤醒模型和字体。', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('base', 'output', 'gif', 'packer'):
        parser.add_argument('--'+name, required=True, type=Path)
    args = parser.parse_args()
    generate(args.base, args.output, args.gif, args.packer)


if __name__ == '__main__':
    main()
