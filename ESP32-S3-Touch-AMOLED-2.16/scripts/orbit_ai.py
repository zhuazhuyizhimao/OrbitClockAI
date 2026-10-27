#!/usr/bin/env python3
"""Prepare/build the combined clock + XiaoZhi board using a pinned upstream SDK."""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess
import sys
import time
import orbit_clock
from orbit_ai_artifacts import package, validate

REPO=Path(__file__).resolve().parents[1]
BASE=REPO.parent
PROJECT=REPO/'examples/esp-idf/06_OrbitClockAI'
UPSTREAM=BASE/'work/xiaozhi-upstream'
REVISION='49b4380b3faa0ceadd28a3e78aaf0f311c46c57e'
BOARD='orbit-clock-ai'
FIRMWARE=BASE/'outputs/OrbitClockAI-firmware'


def acquire_lock():
    # The shared generated project must not be rewritten during another build
    # or native test. Closing this file releases the lock, including on errors.
    import fcntl
    path=BASE/'work/orbit-ai.lock';path.parent.mkdir(parents=True,exist_ok=True)
    handle=path.open('a')
    try:fcntl.flock(handle.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)
    except BlockingIOError:
        handle.close();raise RuntimeError('已有合并版任务正在运行，请等待它结束后再运行。')
    return handle


def write_generated(path,text):
    if path.exists() and path.read_text()==text:return
    temporary=path.with_name(path.name+'.orbit-tmp')
    temporary.write_text(text);temporary.replace(path)


def run(args,**kwargs):
    return subprocess.run([str(value) for value in args],check=True,**kwargs)


def environment():
    env=os.environ.copy()
    idf=Path(env.get('IDF_PATH',Path.home()/'.espressif/v6.1/esp-idf/v6.1/esp-idf'))
    tools=Path(env.get('IDF_TOOLS_PATH',Path.home()/'.espressif/tools'))
    venv=Path(env.get('IDF_PYTHON_ENV_PATH',tools/'python/v6.1/venv'))
    if not (idf/'tools/idf.py').is_file() or not (venv/'bin/python').exists():
        raise RuntimeError('需要 ESP-IDF 6.1 环境；请在 ESP-IDF 终端运行，或配置 IDF_PATH / IDF_PYTHON_ENV_PATH。')
    paths=[venv/'bin',tools/'ninja/1.12.1']
    if sys.platform=='darwin':paths.append(Path('/usr/bin'))
    paths+=sorted(tools.glob('cmake/*/CMake.app/Contents/bin'))
    paths+=sorted(tools.glob('xtensa-esp-elf/*/xtensa-esp-elf/bin'))
    env.update(IDF_PATH=str(idf),IDF_TOOLS_PATH=str(tools),IDF_PYTHON_ENV_PATH=str(venv),
               ESP_IDF_VERSION=env.get('ESP_IDF_VERSION','6.1'),IDF_COMPONENT_CACHE_PATH=str(BASE/'work/idf-component-cache'),
               ESP_ROM_ELF_DIR=str(tools/'esp-rom-elfs/20241011'),IDF_CCACHE_ENABLE='0',PATH=os.pathsep.join(str(p) for p in paths)+os.pathsep+env.get('PATH',''))
    return env,venv/'bin/python'


def original(path):
    return subprocess.check_output(['git','-C',str(UPSTREAM),'show',f'HEAD:{path}'],text=True)


def prepare():
    if not UPSTREAM.exists():
        run(['git','clone','--depth','1','--branch','v2.5.2','https://github.com/78/xiaozhi-esp32.git',UPSTREAM])
    revision=subprocess.check_output(['git','-C',str(UPSTREAM),'rev-parse','HEAD'],text=True).strip()
    if revision!=REVISION:raise RuntimeError(f'小智源码版本不匹配：{revision}')
    # Map compile-time paths in every component, including the local Wi-Fi
    # override outside the generated checkout. Never patch bytes in an image.
    root_cmake=original('CMakeLists.txt')
    project_include='include($ENV{IDF_PATH}/tools/cmake/project.cmake)'
    if root_cmake.count(project_include)!=1:
        raise RuntimeError('小智工程入口已变化，请检查发布路径清理配置。')
    root_cmake=root_cmake.replace(project_include,
        'get_filename_component(ORBIT_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)\n'
        'add_compile_options("-ffile-prefix-map=${ORBIT_SOURCE_ROOT}=/orbit")\n'+project_include,1)
    write_generated(UPSTREAM/'CMakeLists.txt',root_cmake)
    # Local maintained fork: last successful SSID first, with saved fallbacks.
    # Use the component manager's supported override, never edit its cache.
    manifest=original('main/idf_component.yml').replace('  78/esp-wifi-connect: ~3.3.1',
        '  78/esp-wifi-connect:\n    version: ~3.3.1\n    override_path: '+
        json.dumps(str(PROJECT/'network/esp-wifi-connect')))
    # The local ACK adapter is verified against this exact controller parser.
    manifest=manifest.replace('  waveshare/esp_lcd_touch_cst9217: ^1.0.3',
                              '  waveshare/esp_lcd_touch_cst9217: ==1.0.4')
    write_generated(UPSTREAM/'main/idf_component.yml',manifest)
    target=UPSTREAM/'main/boards'/BOARD;target.mkdir(exist_ok=True)
    for path in (PROJECT/'board').iterdir():
        if path.is_file():shutil.copy2(path,target/path.name)
    shutil.copytree(PROJECT/'assets',target/'assets',dirs_exist_ok=True)
    shutil.copy2(REPO/'scripts/generate_orbit_ai_assets.py',target/'generate_orbit_ai_assets.py')
    cmake=original('main/CMakeLists.txt')
    cmake=cmake.replace('if(CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI)',
        'if(CONFIG_BOARD_TYPE_ORBIT_CLOCK_AI)\n    set(BOARD_DIR "orbit-clock-ai")\n    set(BUILTIN_TEXT_FONT font_noto_sans_basic_20_4)\n    list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/orbit-clock-ai)\n'
        '    list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/orbit-clock-ai/orbit)\n'
        '    list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/orbit-clock-ai/sensorlib)\n'
        'elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI)',1)
    # Keep the official wake/font generation, then merge the recorded GIF with
    # the same official mmap packer. Both commands belong to one build rule.
    assets_command='''    add_custom_command(
        OUTPUT ${GENERATED_ASSETS_BIN}
        COMMAND python ${PROJECT_DIR}/scripts/build_default_assets.py ${BUILD_ARGS}
        DEPENDS ${DEFAULT_ASSETS_DEPENDENCIES}
        COMMENT "Building default assets.bin based on configuration"
        VERBATIM
    )'''
    if cmake.count(assets_command)!=1:
        raise RuntimeError('小智资源构建入口已变化，请检查适配代码。')
    custom_command='''    if(CONFIG_BOARD_TYPE_ORBIT_CLOCK_AI)
        set(ORBIT_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/boards/orbit-clock-ai")
        add_custom_command(
            OUTPUT ${GENERATED_ASSETS_BIN}
            COMMAND python ${PROJECT_DIR}/scripts/build_default_assets.py ${BUILD_ARGS}
            COMMAND python ${ORBIT_ASSETS_DIR}/generate_orbit_ai_assets.py
                --base ${GENERATED_ASSETS_BIN} --output ${GENERATED_ASSETS_BIN}
                --gif ${ORBIT_ASSETS_DIR}/assets/screen-eyes.gif
                --packer ${PROJECT_DIR}/scripts/build_default_assets.py
            DEPENDS ${DEFAULT_ASSETS_DEPENDENCIES}
                ${ORBIT_ASSETS_DIR}/generate_orbit_ai_assets.py
                ${ORBIT_ASSETS_DIR}/assets/screen-eyes.gif
            COMMENT "Building Orbit wake/font resources and custom eyes GIF"
            VERBATIM
        )
    else()
'''+assets_command+'''
    endif()'''
    cmake=cmake.replace(assets_command,custom_command,1)
    write_generated(UPSTREAM/'main/CMakeLists.txt',cmake)
    config=original('main/Kconfig.projbuild').replace('    config BOARD_TYPE_OTTO_ROBOT\n',
        '    config BOARD_TYPE_ORBIT_CLOCK_AI\n        bool "Orbit Clock AI (Waveshare S3 AMOLED 2.16)"\n        depends on IDF_TARGET_ESP32S3\n\n'
        '    config BOARD_TYPE_OTTO_ROBOT\n',1)
    config=config.replace('config OTA_URL\n',"""config OTA_ALLOW_FIRMWARE_UPGRADE
    bool "Allow network firmware upgrades"
    default y
    help
        Disable in a locally maintained combined application to keep its UI and
        features. Server activation and USB flashing remain available.

config OTA_APPLY_TIMEZONE_OFFSET
    bool "Apply server timezone offset to system time"
    default y
    help
        Disable when system time is UTC and the application uses the TZ
        environment variable, to avoid applying the timezone offset twice.

config OTA_URL
""",1)
    write_generated(UPSTREAM/'main/Kconfig.projbuild',config)
    ota=original('main/ota.cc')
    ota=ota.replace('            if (cJSON_IsNumber(timezone_offset)) {',
        '#ifdef CONFIG_OTA_APPLY_TIMEZONE_OFFSET\n            if (cJSON_IsNumber(timezone_offset)) {',1)
    ota=ota.replace('            tv.tv_sec = (time_t)(ts / 1000);',
        '#else\n            (void)timezone_offset;\n#endif\n            tv.tv_sec = (time_t)(ts / 1000);',1)
    ota=ota.replace('    cJSON_Delete(root);\n    return {};',
        '#ifndef CONFIG_OTA_ALLOW_FIRMWARE_UPGRADE\n    has_new_version_ = false;\n#endif\n    cJSON_Delete(root);\n    return {};',1)
    signature='bool Ota::Upgrade(const std::string& firmware_url, std::function<void(int progress, size_t speed)> callback) {'
    ota=ota.replace(signature,signature+'\n#ifndef CONFIG_OTA_ALLOW_FIRMWARE_UPGRADE\n    ESP_LOGW(TAG, "Network firmware upgrades disabled by build policy");\n    return false;\n#endif',1)
    write_generated(UPSTREAM/'main/ota.cc',ota)
    application_h=original('main/application.h').replace('    void ToggleChatState();',
        '    void ToggleChatState();\n    // Queue a complete conversation shutdown on the main event loop.\n    void CloseChat();',1)
    write_generated(UPSTREAM/'main/application.h',application_h)
    application=original('main/application.cc')
    close_chat="""
void Application::CloseChat() {
    Schedule([this]() {
        const auto state = GetDeviceState();
        if (state == kDeviceStateNotifying) {
            StopNotification();
            return;
        }
        if (state != kDeviceStateConnecting && state != kDeviceStateListening &&
            state != kDeviceStateSpeaking) {
            return;
        }
        if (state == kDeviceStateSpeaking) {
            AbortSpeaking(kAbortReasonNone);
        }
        if (protocol_ && protocol_->IsAudioChannelOpened()) {
            protocol_->CloseAudioChannel();
        }
        audio_service_.EnableVoiceProcessing(false);
        audio_service_.ResetDecoder();
        if (GetDeviceState() != kDeviceStateIdle) {
            SetDeviceState(kDeviceStateIdle);
        }
    });
}
"""
    marker='void Application::ToggleChatState() { xEventGroupSetBits(event_group_, MAIN_EVENT_TOGGLE_CHAT); }'
    if marker not in application:raise RuntimeError('小智对话入口已变化，请检查适配代码。')
    application=application.replace(marker,marker+'\n'+close_chat,1)
    write_generated(UPSTREAM/'main/application.cc',application)
    generator=REPO/'scripts/generate_orbit_ai_port.py'
    if generator.exists():run([sys.executable,generator])
    print(f'Prepared {BOARD}; upstream {revision}',flush=True)


def flash(args):
    images=validate(FIRMWARE)
    env=orbit_clock.dependencies();port=orbit_clock.find_port(args.port)
    tool=[sys.executable,'-m','esptool','--chip','esp32s3','--port',port,'--baud',str(args.baud),'--no-stub']
    if args.skip_backup:
        print('所选任务：直接烧录合并版，不创建原固件完整备份。',flush=True)
    else:
        backup=BASE/'work/board-backups'/(time.strftime('%Y%m%d-%H%M%S')+f'-{time.time_ns()%1000000000:09d}-before-orbit-ai.bin')
        backup.parent.mkdir(parents=True,exist_ok=True)
        print('先备份板上 16MB 固件；备份失败不会继续烧录。',flush=True)
        orbit_clock.backup_flash(tool,backup,env)
    print('使用 ROM 下载器写入合并版，保留 NVS 中的游戏和方向设置。',flush=True)
    arguments=[part for address,path in images for part in (address,str(path))]
    run(tool+['write-flash','--flash-mode','dio','--flash-freq','80m','--flash-size','16MB',*arguments],env=env)
    time.sleep(2);orbit_clock.sync_time(port)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=['prepare','build','test','package','flash','build-flash','sync'])
    parser.add_argument('--port',default='auto')
    parser.add_argument('--baud',type=int,choices=[115200,230400,460800],default=115200)
    parser.add_argument('--skip-backup',action='store_true',help='明确选择直接烧录，不创建原固件备份')
    args=parser.parse_args()
    with acquire_lock():
        if args.action=='sync':
            orbit_clock.dependencies();orbit_clock.sync_time(orbit_clock.find_port(args.port));return
        if args.action=='flash':flash(args);return
        prepare()
        if args.action=='prepare':return
        if args.action=='package':package(UPSTREAM/'build',FIRMWARE,REVISION);return
        env,python=environment()
        if args.action=='test':run([python,'-m','unittest','discover','-s','scripts/tests','-v'],cwd=UPSTREAM,env=env);return
        # The official build entry point generates SDK configuration from config.json.
        run([python,'scripts/build.py',BOARD,'--name',BOARD],cwd=UPSTREAM,env=env)
        package(UPSTREAM/'build',FIRMWARE,REVISION)
        if args.action=='build-flash':flash(args)


if __name__=='__main__':
    try:main()
    except (RuntimeError,OSError,subprocess.CalledProcessError) as error:
        print(f'Orbit AI 未完成：{error}',file=sys.stderr);sys.exit(1)
