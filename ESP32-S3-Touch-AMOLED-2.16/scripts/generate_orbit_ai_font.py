#!/usr/bin/env python3
"""Regenerate the small XiaoZhi menu font; only regeneration requires Pillow."""
from pathlib import Path
import re
from generate_orbit_fonts import generate
root=Path(__file__).resolve().parents[1]
labels=(root/'examples/esp-idf/06_OrbitClockAI/board/orbit_time_sync.h').read_text()
labels+=(root/'examples/esp-idf/06_OrbitClockAI/board/orbit_touch_calibration_ui.h').read_text()
characters=set('小智') | set(re.findall(r'[\u3000-\uffff]',labels))
text=generate(20,sorted(characters)).replace('orbit_font_zh_20','orbit_font_ai_20')
text=re.sub(r'static lv_font_fmt_txt_glyph_cache_t cache_20;\n','',text)
text=text.replace(', .cache = &cache_20','').replace('.fallback = &lv_font_montserrat_20','.static_bitmap = 1, .fallback = &orbit_font_zh_20')
(root/'examples/esp-idf/06_OrbitClockAI/board/orbit_ai_font.c').write_text('/* Generated from bundled SimSun.woff by generate_orbit_ai_font.py. */\n#include "orbit_ai_font.h"\n'+text)
