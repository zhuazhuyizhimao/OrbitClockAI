#!/usr/bin/env python3
"""Build, flash and set Orbit Clock time from an ordinary local terminal."""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

REPO = Path(__file__).resolve().parents[1]
TASK = REPO.parent
TOOLCHAIN = TASK / "work/toolchain"
FIRMWARE = TASK / "outputs/OrbitClock-firmware"
SKETCH = REPO / "examples/arduino/08_OrbitClock"
FQBN = "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=default"
FLASH_BYTES = 16 * 1024 * 1024
BACKUP_CHUNK_BYTES = 512 * 1024
BACKUP_ATTEMPTS = 3


def run(command: list[str], *, env: dict[str, str] | None = None) -> None:
    subprocess.run(command, cwd=REPO, env=env, check=True)


def build() -> None:
    cli = TOOLCHAIN / "arduino-cli"
    command = [str(cli)] if cli.exists() else [shutil.which("arduino-cli") or "arduino-cli"]
    config = TOOLCHAIN / "arduino-cli.yaml"
    if cli.exists() and config.exists():
        command += ["--config-file", str(config)]
    run(command + ["compile", "--fqbn", FQBN, "--libraries",
                   str(REPO / "examples/arduino/libraries"), "--build-path",
                   str(TASK / "work/orbit-build"), "--output-dir", str(FIRMWARE), str(SKETCH)])


def dependencies() -> dict[str, str]:
    local = TOOLCHAIN / "python"
    env = os.environ.copy()
    if local.exists():
        sys.path.insert(0, str(local))
        env["PYTHONPATH"] = str(local) + os.pathsep + env.get("PYTHONPATH", "")
    return env


def find_port(requested: str) -> str:
    if requested != "auto":
        return requested
    from serial.tools import list_ports
    ports = [p.device for p in list_ports.comports() if p.vid == 0x303A and p.pid == 0x1001]
    if len(ports) == 1:
        return ports[0]
    if not ports:
        raise RuntimeError("没有找到 ESP32 USB 设备。请连接数据线后重试，或通过 --port 指定端口。")
    raise RuntimeError("发现多个 ESP32，请只保留目标开发板，或通过 --port 指定端口。")


def sync_time(port: str) -> None:
    import serial
    # DTR/RTS remain deasserted so opening the console does not intentionally reset.
    connection = serial.Serial(baudrate=115200, timeout=0.25)
    connection.dtr = False
    connection.rts = False
    connection.port = port
    connection.open()
    try:
        ack = False
        deadline = time.monotonic() + 12
        next_attempt = 0.0
        while time.monotonic() < deadline:
            if time.monotonic() >= next_attempt:
                connection.write(f"TIME {int(time.time())}\nSTATUS\n".encode())
                connection.flush()
                next_attempt = time.monotonic() + 2
            line = connection.readline().decode("utf-8", errors="replace").strip()
            if line:
                print(line, flush=True)
                ack |= "ORBIT TIME OK:" in line
                if "ORBIT STATUS:" in line and ack:
                    if "touch=FAILED" in line:
                        raise RuntimeError("固件已运行并校时，但触摸初始化失败。请保留日志用于排查。")
                    print("USB 校时成功，固件已响应状态查询。请在板上检查时间显示和触摸按钮。", flush=True)
                    return
        raise RuntimeError("未收到校时和状态确认。请确认时钟固件已启动，关闭其他串口监视器后重试。")
    finally:
        connection.close()


def backup_flash(tool: list[str], backup: Path, env: dict[str, str]) -> None:
    """Read bounded segments, retaining successful segments within this attempt."""
    parts = backup.with_suffix(".parts")
    parts.mkdir(parents=True, exist_ok=False)
    count = (FLASH_BYTES + BACKUP_CHUNK_BYTES - 1) // BACKUP_CHUNK_BYTES
    for index, address in enumerate(range(0, FLASH_BYTES, BACKUP_CHUNK_BYTES)):
        size = min(BACKUP_CHUNK_BYTES, FLASH_BYTES - address)
        segment = parts / f"{address:08x}.bin"
        for attempt in range(1, BACKUP_ATTEMPTS + 1):
            print(f"备份分段 {index + 1}/{count}，尝试 {attempt}/{BACKUP_ATTEMPTS}。", flush=True)
            try:
                # Keep the original application stopped between segments, so it
                # cannot modify NVS or other flash contents during this backup.
                run(tool + ["--after", "no-reset", "read-flash", hex(address), hex(size), str(segment)], env=env)
                if not segment.is_file() or segment.stat().st_size != size:
                    raise RuntimeError("备份分段长度不完整")
                break
            except (subprocess.CalledProcessError, RuntimeError) as error:
                if attempt == BACKUP_ATTEMPTS:
                    raise RuntimeError(
                        f"第 {index + 1}/{count} 段备份仍失败，未开始烧录。"
                        "请换一根 USB 数据线或直连电脑 USB 后重试。"
                        "若板子停留在下载模式，可断开 USB 后重新连接。"
                    ) from error
                print("读取中断，稍后重试当前分段；已完成分段保留。", flush=True)
                time.sleep(attempt)
    temporary = backup.with_suffix(".partial")
    digest = hashlib.sha256()
    with temporary.open("xb") as output:
        for address in range(0, FLASH_BYTES, BACKUP_CHUNK_BYTES):
            data = (parts / f"{address:08x}.bin").read_bytes()
            output.write(data)
            digest.update(data)
        output.flush()
        os.fsync(output.fileno())
    if temporary.stat().st_size != FLASH_BYTES:
        raise RuntimeError("备份总长度不正确，未开始烧录。")
    temporary.rename(backup)
    backup.with_suffix(".sha256").write_text(digest.hexdigest() + "  " + backup.name + "\n")
    print("原固件备份完整，已保存 SHA-256 摘要。", flush=True)


def flash(port: str, env: dict[str, str], baud: int = 115200, *,
          skip_backup: bool = False, no_stub: bool = False) -> None:
    images = [("0x0", FIRMWARE / "08_OrbitClock.ino.bootloader.bin"),
              ("0x8000", FIRMWARE / "08_OrbitClock.ino.partitions.bin"),
              ("0xe000", TOOLCHAIN / "data/packages/esp32/hardware/esp32/3.3.11/tools/partitions/boot_app0.bin"),
              ("0x10000", FIRMWARE / "08_OrbitClock.ino.bin")]
    for _, image in images:
        if not image.is_file():
            raise RuntimeError(f"缺少烧录文件：{image.name}。请先运行编译任务。")
    tool = [sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", port, "--baud", str(baud)]
    if no_stub:
        tool += ["--no-stub"]
        print("使用 ESP32-S3 ROM 下载器，不上传 flasher stub。", flush=True)
    if skip_backup:
        print("所选任务：直接烧录，不创建原固件完整备份。", flush=True)
    else:
        backup = TASK / "work/board-backups" / (time.strftime("%Y%m%d-%H%M%S") + f"-{time.time_ns() % 1000000000:09d}-before-orbit.bin")
        backup.parent.mkdir(parents=True, exist_ok=True)
        print("备份板上原固件（16 MB）。备份失败时不会继续烧录。", flush=True)
        backup_flash(tool, backup, env)
    print("开始写入 Orbit Clock，完成后自动重启。", flush=True)
    arguments = [part for offset, image in images for part in (offset, str(image))]
    run(tool + ["write-flash", "--flash-mode", "dio", "--flash-freq", "80m",
                "--flash-size", "16MB", *arguments], env=env)
    time.sleep(2)
    sync_time(port)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["build", "flash", "build-flash", "sync"])
    parser.add_argument("--port", default="auto")
    parser.add_argument("--baud", type=int, choices=[115200, 230400, 460800, 921600], default=115200)
    parser.add_argument("--skip-backup", action="store_true", help="明确选择直接烧录，不创建原固件备份")
    parser.add_argument("--no-stub", action="store_true", help="直接使用 ROM 下载器，不上传 flasher stub")
    args = parser.parse_args()
    if args.action in ("build", "build-flash"):
        build()
    if args.action == "build":
        return
    env = dependencies()
    port = find_port(args.port)
    if args.action in ("flash", "build-flash"):
        flash(port, env, args.baud, skip_backup=args.skip_backup, no_stub=args.no_stub)
    else:
        sync_time(port)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        print(f"Orbit 任务未完成：{error}", file=sys.stderr)
        raise SystemExit(1)
