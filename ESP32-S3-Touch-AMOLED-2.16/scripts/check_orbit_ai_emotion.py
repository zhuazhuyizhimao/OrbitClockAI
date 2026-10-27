#!/usr/bin/env python3
"""Check the GIF with XiaoZhi's actual decoder and compare every pixel to Pillow.

Requires Pillow, clang and a prior successful check_orbit_ai_ui.py run.
"""
from pathlib import Path
import subprocess
import tempfile
from PIL import Image
import orbit_ai

REPO = Path(__file__).resolve().parents[1]
BASE = REPO.parent
UPSTREAM = BASE/'work/xiaozhi-upstream'
LVGL = UPSTREAM/'managed_components/lvgl__lvgl'
NATIVE = BASE/'work/lvgl9-check'
PROJECT = REPO/'examples/esp-idf/06_OrbitClockAI'


def main():
    task_lock = orbit_ai.acquire_lock()
    assert task_lock
    objects = sorted(NATIVE.glob('src_*.o'))
    if not objects or not (NATIVE/'lv_conf.h').exists():
        raise RuntimeError('请先运行 check_orbit_ai_ui.py 准备原生 LVGL。')
    gif_path = PROJECT/'assets/screen-eyes.gif'
    with tempfile.TemporaryDirectory(dir=BASE/'work') as directory:
        temp = Path(directory)
        (temp/'esp_log.h').write_text('''#pragma once
#define ESP_LOGD(...)
#define ESP_LOGI(...)
#define ESP_LOGW(...)
#define ESP_LOGE(...)
''')
        display = UPSTREAM/'main/display/lvgl_display'
        flags = ['-O0', '-DLV_CONF_INCLUDE_SIMPLE', '-I'+str(temp), '-I'+str(NATIVE),
                 '-I'+str(LVGL), '-I'+str(display)]
        decoder = temp/'gifdec.o'
        controller = temp/'lvgl_gif.o'
        subprocess.run(['clang', *flags, '-c', str(display/'gif/gifdec.c'), '-o', str(decoder)], check=True)
        subprocess.run(['clang++', '-std=c++17', *flags, '-c', str(display/'gif/lvgl_gif.cc'),
                        '-o', str(controller)], check=True)
        executable = temp/'check'
        subprocess.run(['clang++', '-std=c++17', *flags, str(PROJECT/'tests/native_emotion.cpp'),
                        str(decoder), str(controller), *(str(p) for p in objects), '-o', str(executable)], check=True)
        decoded = temp/'decoded.bgra'
        subprocess.run([str(executable), str(gif_path), str(decoded)], check=True, timeout=45)
        with Image.open(gif_path) as gif, decoded.open('rb') as actual:
            assert gif.n_frames == 116 and gif.size == (400, 180)
            for loop in range(2):
                for i in range(gif.n_frames):
                    gif.seek(i)
                    expected = gif.convert('RGBA').tobytes('raw', 'BGRA')
                    frame = actual.read(len(expected))
                    assert frame == expected, f'帧像素不一致：loop={loop}, frame={i}'
                gif.seek(0)
            assert not actual.read(1)
        print('PASS: 全部 232 帧与 Pillow 解码逐像素一致，没有局部帧残留或循环错误。')


if __name__ == '__main__':
    main()
