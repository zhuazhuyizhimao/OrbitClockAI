#!/usr/bin/env python3
"""Generate small LVGL 8 CJK fonts from the repository's bundled SimSun.woff.

Requires Pillow only when regenerating. Arduino builds use the generated C file.
The source font is the same font bundled with LVGL's built-in CJK font resources.
"""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "examples/arduino/08_OrbitClock"
SOURCE = ROOT / "examples/arduino/libraries/lvgl/scripts/built_in_font/SimSun.woff"


def generate(size, chars):
    font = ImageFont.truetype(str(SOURCE), size)
    bitmap = bytearray()
    descriptors = ["    {0, 0, 0, 0, 0, 0}, /* reserved */"]
    for char in chars:
        left, top, right, bottom = font.getbbox(char, anchor="ls")
        width, height = right - left, bottom - top
        image = Image.new("L", (width, height))
        ImageDraw.Draw(image).text((-left, -top), char, font=font, fill=255, anchor="ls")
        values = image.get_flattened_data() if hasattr(image, "get_flattened_data") else image.getdata()
        pixels = [(value * 15 + 127) // 255 for value in values]
        index = len(bitmap)
        for i in range(0, len(pixels), 2):
            bitmap.append((pixels[i] << 4) | (pixels[i + 1] if i + 1 < len(pixels) else 0))
        descriptors.append(f"    {{{index}, {round(font.getlength(char) * 16)}, {width}, {height}, {left}, {-bottom}}}, /* {char} */")
    name = f"orbit_font_zh_{size}"
    data = ",\n".join("    " + ", ".join(f"0x{x:02x}" for x in bitmap[i:i + 20]) for i in range(0, len(bitmap), 20))
    start = ord(chars[0])
    codes = ", ".join(str(ord(c) - start) for c in chars)
    ascent, descent = font.getmetrics()
    return f'''
static const uint8_t bitmap_{size}[] = {{
{data}
}};
static const lv_font_fmt_txt_glyph_dsc_t glyphs_{size}[] = {{
{chr(10).join(descriptors)}
}};
static const uint16_t unicode_{size}[] = {{{codes}}};
static const lv_font_fmt_txt_cmap_t cmap_{size}[] = {{{{
    .range_start = {start}, .range_length = {ord(chars[-1]) - start + 1}, .glyph_id_start = 1,
    .unicode_list = unicode_{size}, .glyph_id_ofs_list = NULL,
    .list_length = {len(chars)}, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
}}}};
static lv_font_fmt_txt_glyph_cache_t cache_{size};
static const lv_font_fmt_txt_dsc_t description_{size} = {{
    .glyph_bitmap = bitmap_{size}, .glyph_dsc = glyphs_{size}, .cmaps = cmap_{size},
    .kern_dsc = NULL, .kern_scale = 0, .cmap_num = 1, .bpp = 4,
    .kern_classes = 0, .bitmap_format = 0, .cache = &cache_{size}
}};
const lv_font_t {name} = {{
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = {ascent + descent + 2}, .base_line = {descent + 1}, .subpx = LV_FONT_SUBPX_NONE,
    .underline_position = -1, .underline_thickness = 1, .dsc = &description_{size},
    .fallback = &lv_font_montserrat_{size}
}};
'''


def generate_coin_glyphs():
    font = ImageFont.truetype(str(SOURCE), 16)
    rows = []
    for char in "正反":
        image = Image.new("L", (16, 16))
        ImageDraw.Draw(image).text((0, -1), char, font=font, fill=255)
        # Preserve thin anti-aliased strokes, including the bottom bar of 正.
        rows.append([sum(1 << (15-x) for x in range(16) if image.getpixel((x, y)) >= 48)
                     for y in range(16)])
    content = '#pragma once\n#include <cstdint>\n// 16px SimSun glyphs from generate_orbit_fonts.py.\n'
    content += 'static constexpr uint16_t COIN_GLYPHS[2][16]={\n'
    content += ',\n'.join('  {' + ','.join(hex(value) for value in row) + '}' for row in rows) + '\n};\n'
    content += '\nstatic constexpr float COIN_RING[16][2]={\n'
    content += ',\n'.join('  {' + f'{math.cos(i*math.pi/8):.8f}f,{math.sin(i*math.pi/8):.8f}f' + '}'
                         for i in range(16)) + '\n};\n'
    (APP / 'coin_glyphs.h').write_text(content)


def main():
    text = '\n'.join((APP / name).read_text() for name in ("08_OrbitClock.ino", "dice_ui.h", "sand_ui.h", "player_ui.h"))
    chars = sorted({c for c in text if 0x3000 <= ord(c) <= 0xffff})
    content = '/* Generated from LVGL bundled SimSun.woff; see scripts/generate_orbit_fonts.py. */\n#include "orbit_fonts.h"\n'
    content += "".join(generate(size, chars) for size in (16, 20))
    content += generate(32, sorted(set("正反面")))
    (APP / "orbit_fonts.c").write_text(content)
    generate_coin_glyphs()
    print(f"Generated 16/20px subsets with {len(chars)} glyphs each and a 32px coin-result subset.")


if __name__ == "__main__":
    main()
