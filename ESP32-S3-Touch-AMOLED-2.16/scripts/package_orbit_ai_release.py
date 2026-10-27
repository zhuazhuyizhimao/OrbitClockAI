#!/usr/bin/env python3
"""Create an allowlisted GitHub distribution and audit loose files + ZIP bytes."""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile
from check_release_privacy import inspect_tree, inspect_zip
from orbit_ai_artifacts import validate

REPO = Path(__file__).resolve().parents[1]
BASE = REPO.parent
PROJECT = REPO/'examples/esp-idf/06_OrbitClockAI'
OUTPUT = BASE/'outputs'
DEST = OUTPUT/'OrbitClockAI-GitHub'
REVISION = '49b4380b3faa0ceadd28a3e78aaf0f311c46c57e'
SCRIPTS = ('orbit_ai.py', 'orbit_ai_artifacts.py', 'orbit_clock.py',
           'generate_orbit_ai_port.py', 'generate_orbit_ai_font.py',
           'generate_orbit_ai_assets.py', 'generate_orbit_fonts.py',
           'generate_retro_assets.py', 'generate_pikachu_audio.py',
           'check_orbit_ai_ui.py', 'check_orbit_ai_emotion.py',
           'check_release_privacy.py', 'package_orbit_ai_source.py',
           'package_orbit_ai_release.py')


def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def copy_sources(source, target):
    for path in sorted(source.rglob('*')):
        if not path.is_file() or path.is_symlink():
            continue
        relative = path.relative_to(source)
        if {'__pycache__', '.git', 'build', 'managed_components'}.intersection(relative.parts):
            continue
        if path.name == 'clock_secrets.h' or path.suffix in ('.pyc', '.pyo'):
            continue
        copy(path, target/relative)


def licenses():
    result = {'Waveshare-Apache-2.0.txt': REPO/'LICENSE',
              'XiaoZhi-MIT.txt': BASE/'work/xiaozhi-upstream/LICENSE',
              'SensorLib-MIT.txt': REPO/'examples/arduino/libraries/SensorLib/LICENSE',
              'LVGL-MIT.txt': REPO/'examples/arduino/libraries/lvgl/LICENCE.txt',
              'Arduino-GFX-LICENSE.txt': REPO/'examples/arduino/libraries/GFX_Library_for_Arduino/license.txt',
              'esp-wifi-connect-MIT.txt': PROJECT/'network/esp-wifi-connect/LICENSE'}
    upstream = BASE/'work/xiaozhi-upstream'
    lvgl = upstream/'managed_components/lvgl__lvgl/scripts/built_in_font/font_license'
    for name in ('Montserrat', 'NotoSansSC', 'NotoColorEmoji'):
        path = lvgl/name/'OFL.txt'
        if path.is_file():
            result[name+'-OFL.txt'] = path
    gif_license = upstream/'main/display/lvgl_display/gif/LICENSE.txt'
    if gif_license.is_file():
        result['gifdec-LICENSE.txt'] = gif_license
    description = json.loads((upstream/'build/project_description.json').read_text())
    for text in description.get('build_component_paths', []):
        if not text:
            continue
        folder = Path(text)
        for path in folder.iterdir():
            if path.is_file() and path.name.lower().startswith(('license', 'licence', 'copying', 'notice')):
                # Component names and license basenames only; never export build paths.
                result[f'components/{folder.name}/{path.name}'] = path
    sdk = BASE/'work/esp-idf-6.0.1/LICENSE'
    if sdk.is_file():
        result['ESP-IDF-LICENSE.txt'] = sdk
    return result


def workspace(folder):
    tasks = []
    for label, args in (
        ('小智合并版：编译', ['build']),
        ('小智合并版：烧录并校时（先备份）', ['flash']),
        ('小智合并版：直接烧录并校时（不备份）', ['flash', '--skip-backup']),
        ('小智合并版：仅 USB 校时', ['sync']),
    ):
        tasks.append({'label': label, 'type': 'process', 'command': 'python3',
                      'windows': {'command': 'python'},
                      'args': ['${workspaceFolder}/scripts/orbit_ai.py', *args],
                      'problemMatcher': ['$gcc'] if args == ['build'] else [],
                      'presentation': {'reveal': 'always', 'panel': 'shared'}})
    return {'folders': [{'path': folder}], 'tasks': {'version': '2.0.0', 'tasks': tasks}}


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')


def check_and_zip(root, archive, prefix):
    report = inspect_tree(root)
    if not report['passed']:
        raise RuntimeError('隐私检查拒绝打包：'+json.dumps(report['findings'], ensure_ascii=False))
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
        for path in sorted(root.rglob('*')):
            if path.is_file():
                output.write(path, str(Path(prefix)/path.relative_to(root)))
    issues = inspect_zip(archive.read_bytes())
    if issues:
        archive.unlink()
        raise RuntimeError('ZIP 隐私检查失败：'+json.dumps(issues, ensure_ascii=False))
    return report


def main():
    fw = OUTPUT/'OrbitClockAI-firmware'
    validate(fw)
    audit = inspect_tree(fw)
    if not audit['passed']:
        raise RuntimeError('固件仍包含隐私信息，必须重新构建后打包。')
    if DEST.exists():
        # Only this script's generated distribution directory is replaced.
        shutil.rmtree(DEST)
    DEST.mkdir(parents=True)
    source = DEST/REPO.name
    copy_sources(PROJECT, source/PROJECT.relative_to(REPO))
    app = REPO/'examples/arduino/08_OrbitClock'
    copy_sources(app, source/app.relative_to(REPO))
    for name in SCRIPTS:
        copy(REPO/'scripts'/name, source/'scripts'/name)
    for path in (REPO/'tests').glob('test_*.py'):
        if path.name not in ('test_repository_policy.py', 'test_discover_and_routing.py'):
            copy(path, source/'tests'/path.name)
    vendor = REPO/'examples/arduino/libraries'
    copy_sources(vendor/'SensorLib/src', source/'examples/arduino/libraries/SensorLib/src')
    copy(vendor/'SensorLib/LICENSE', source/'examples/arduino/libraries/SensorLib/LICENSE')
    copy(vendor/'lvgl/src/font/lv_font_montserrat_48.c', source/'examples/arduino/libraries/lvgl/src/font/lv_font_montserrat_48.c')
    copy(vendor/'lvgl/LICENCE.txt', source/'examples/arduino/libraries/lvgl/LICENCE.txt')
    # The panel regression test compares the actual reference rotation method.
    for name in ('src/display/Arduino_CO5300.h', 'src/display/Arduino_CO5300.cpp', 'license.txt'):
        copy(vendor/'GFX_Library_for_Arduino'/name,
             source/'examples/arduino/libraries/GFX_Library_for_Arduino'/name)
    copy(REPO/'LICENSE', source/'LICENSE')
    (source/'README.md').write_text('# Orbit Clock AI source\n\nSee [中文说明](../README_CN.md) for the combined ESP-IDF board.\n', encoding='utf-8')
    (source/'requirements-flash.txt').write_text('esptool==5.3.1\npyserial==3.5\n')
    for name in ('README.md', 'README_CN.md', 'THIRD_PARTY_NOTICES.md'):
        copy(PROJECT/'release'/name, DEST/name)
    copy(REPO/'LICENSE', DEST/'LICENSE')
    texts = []
    for name, path in sorted(licenses().items()):
        copy(path, DEST/'LICENSES'/name)
        texts.append(f'===== {name} =====\n'+path.read_text(errors='replace')+'\n')
    # Keep the standalone firmware download's notices in flat files, too.
    copy(REPO/'LICENSE', fw/'LICENSE.txt')
    copy(PROJECT/'release/THIRD_PARTY_NOTICES.md', fw/'THIRD_PARTY_NOTICES.md')
    (fw/'THIRD_PARTY_LICENSES.txt').write_text('\n'.join(texts), encoding='utf-8')
    (fw/'README_CN.md').write_text('''# Orbit Clock AI 固件 · 2026-10-27

适用板卡：Waveshare ESP32-S3-Touch-AMOLED-2.16，16MB Flash / 8MB PSRAM。
包含像素时钟、互动音效、专注、骰子、流沙、玩家转盘、小智语音和动态眼睛。
小智页面已移除屏幕返回按钮，单击 Key3 返回。源代码与完整中文说明见配套 GitHub 发布包。

## 烧录

优先使用完整发布包的 VS Code 工作区，任务会校验五个镜像。此 ZIP 不包含设备 NVS、Wi-Fi 密码或整片备份。

手动烧录时，在本目录安装 esptool，并将下面的 COM3 换成实际端口。Windows 可使用 python 代替 python3。

```sh
python3 -m pip install esptool==5.3.1
python3 -m esptool --chip esp32s3 --port COM3 --baud 115200 --no-stub write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0 bootloader.bin 0x8000 partition-table.bin 0xe000 ota_data_initial.bin 0x20000 orbit-clock-ai.bin 0x800000 assets.bin
```

主应用地址为 0x20000；不要使用 Arduino 版的 0x10000。NVS 位于 0x9000、大小 0x5000，本包不写该范围，不需要整片擦除。SHA256SUMS 与 flash_manifest.json 提供文件摘要、大小和地址。

首次使用按屏幕提示配网并完成小智激活。三颗按键朝上为正方向；按住 Key3 三秒可进行触摸校准。Key2 用于手动联网或在线重新校时。

本次完成编译与主机检查，尚未通过自动化向实板烧录。第三方代码许可及演示媒体来源见同目录的 THIRD_PARTY_NOTICES.md 与 THIRD_PARTY_LICENSES.txt；用户提供素材不由代码许可证重新授权。
''', encoding='utf-8')
    write_json(fw/'PRIVACY_REPORT.json', {'passed': True, 'scope': 'five firmware images and public notices',
               'personal_paths': 0, 'device_backups': 0, 'nvs_images': 0,
               'credentials_files': 0, 'method': 'binary path scan, credential-file exclusion, partition-range validation'})
    check_and_zip(fw, OUTPUT/'OrbitClockAI-firmware.zip', fw.name)
    copy_sources(fw, DEST/'outputs/OrbitClockAI-firmware')
    allowed_images = {f'outputs/OrbitClockAI-firmware/{item[1].name}' for item in validate(fw)}
    if {p.relative_to(DEST).as_posix() for p in DEST.rglob('*.bin')} != allowed_images:
        raise RuntimeError('发布目录含五个固件镜像以外的二进制文件。')
    write_json(DEST/'OrbitClockAI.code-workspace', workspace(REPO.name))
    write_json(DEST/'outputs/OrbitClockAI.code-workspace', workspace('../'+REPO.name))
    (DEST/'.gitignore').write_text('''work/
**/build/
**/managed_components/
**/__pycache__/
**/.venv/
**/clock_secrets.h
**/.env
**/.env.*
*.py[cod]
*.elf
*.map
*.partial
*.parts/
**/board-backups/
.DS_Store
.vscode/
.idea/
''')
    for name, original in (('clock.png', 'OrbitClockAI-时钟与入口.png'),
                           ('touch-calibration.png', 'OrbitClockAI-触摸校准.png')):
        preview = OUTPUT/original
        if not preview.exists():
            preview = BASE/'docs/assets'/name
        copy(preview, DEST/'docs/assets'/name)
    preview = OUTPUT/'视频屏幕表情包/屏幕表情-完整动画-240.gif'
    if not preview.exists():
        preview = BASE/'docs/assets/eyes.gif'
    copy(preview, DEST/'docs/assets/eyes.gif')
    copy(OUTPUT/'OrbitClockAI-验证说明.md', DEST/'docs/VALIDATION_CN.md')
    copy(OUTPUT/'OrbitClockAI-验证说明.md', DEST/'outputs/OrbitClockAI-验证说明.md')
    write_json(DEST/'RELEASE.json', {'release_date': '2026-10-27', 'board': 'orbit-clock-ai',
               'xiaozhi_revision': REVISION, 'sdk_used': 'ESP-IDF 6.0.1',
               'firmware_manifest': 'outputs/OrbitClockAI-firmware/flash_manifest.json',
               'contains_nvs_or_device_dump': False, 'hardware_flashed_by_automation': False})
    report = inspect_tree(DEST)
    if not report['passed']:
        raise RuntimeError('发布内容未通过隐私检查：'+json.dumps(report['findings'], ensure_ascii=False))
    report.update({'scope': 'all release files before this report and root checksum file',
                   'firmware_partition_validation': 'passed; NVS range excluded',
                   'build_path_removal': 'compiler -ffile-prefix-map; no binary patching',
                   'device_backups_included': False, 'wifi_configuration_included': False})
    write_json(DEST/'docs/PRIVACY_REPORT.json', report)
    files = sorted(p for p in DEST.rglob('*') if p.is_file())
    (DEST/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+
        '  '+p.relative_to(DEST).as_posix()+'\n' for p in files))
    archive = OUTPUT/'OrbitClockAI-GitHub.zip'
    final = check_and_zip(DEST, archive, DEST.name)
    archive.with_suffix('.zip.sha256').write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+
                                                 '  '+archive.name+'\n')
    print(f'GitHub 发布包：{archive.name}, {archive.stat().st_size} bytes, '
          f'{final["files_scanned"]} files; loose files and ZIP privacy checks passed.')


if __name__ == '__main__':
    main()
