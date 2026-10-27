#!/usr/bin/env python3
"""Check release trees or ZIP members without printing sensitive contents."""
from pathlib import Path, PurePosixPath
import argparse
import io
import json
import re
import zipfile

PATTERNS = {
    'personal_absolute_path': re.compile(
        rb'(?:/(?:Users|home)/[A-Za-z0-9_.-]+|[A-Za-z]:[\\/]Users[\\/][A-Za-z0-9_.-]+'
        rb'|/(?:private/)?var/folders/[A-Za-z0-9_-]+/[A-Za-z0-9_-]+)', re.I),
    # TLS parsers contain PEM header strings. Require a body + matching footer
    # so a parser constant is not mistaken for an actual embedded private key.
    'private_key': re.compile(rb'-----BEGIN ((?:RSA |EC |OPENSSH |ENCRYPTED )?PRIVATE KEY)-----'
                              rb'[\r\n]+[A-Za-z0-9+/=\r\n]{64,}-----END \1-----'),
    'github_token': re.compile(rb'(?:github_pat_[A-Za-z0-9_]{30,}|gh[pousr]_[A-Za-z0-9]{30,})'),
}
FORBIDDEN_PARTS = {'.git', '__pycache__', 'managed_components', 'board-backups',
                   'toolchain', 'idf-component-cache', '.venv', 'node_modules', 'build', 'work'}
FORBIDDEN_NAMES = {'clock_secrets.h', '.env', 'id_rsa', 'id_ed25519', 'nvs.bin',
                   'nvs_keys.bin', 'sdkconfig', 'compile_commands.json'}
PLACEHOLDERS = {b'', b'YOUR_2_4_GHZ_WIFI', b'YOUR_WIFI_PASSWORD', b'xiaozhi-shared'}
WIFI_DEFINE = re.compile(rb'#define\s+CLOCK_WIFI_(?:SSID|PASSWORD)\s+"([^"\r\n]*)"')


def inspect_bytes(name, data):
    """Return rule IDs only. Never include matched credential/path values."""
    path = PurePosixPath(name)
    issues = []
    if path.is_absolute() or '..' in path.parts or '\\' in name:
        issues.append('unsafe_archive_path')
    if FORBIDDEN_PARTS.intersection(path.parts) or path.name.lower() in FORBIDDEN_NAMES:
        issues.append('private_or_generated_file')
    if path.suffix.lower() in ('.elf', '.map', '.partial', '.parts', '.pcap', '.pcapng'):
        issues.append('debug_or_backup_file')
    if any(term in path.name.lower() for term in ('before-orbit', 'flash-backup', 'full-flash', 'backup-flash')):
        issues.append('device_backup_file')
    if path.suffix.lower() == '.bin' and len(data) == 16*1024*1024:
        issues.append('full_device_dump')
    for rule, pattern in PATTERNS.items():
        if pattern.search(data):
            issues.append(rule)
    if path.suffix.lower() in ('.h', '.c', '.cc', '.cpp', '.ino', '.example'):
        if any(value not in PLACEHOLDERS for value in WIFI_DEFINE.findall(data)):
            issues.append('configured_wifi_literal')
    return sorted(set(issues))


def inspect_zip(data, prefix=''):
    issues = []
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        for item in archive.infolist():
            name = prefix+item.filename
            if item.is_dir():
                # Directory entries still must not smuggle a private location.
                problems = inspect_bytes(name.rstrip('/'), b'')
            else:
                content = archive.read(item)
                problems = inspect_bytes(name, content)
                if item.filename.endswith('.zip'):
                    issues.extend(inspect_zip(content, name+'!'))
            issues.extend({'file': name, 'rule': rule} for rule in problems)
    return issues


def inspect_tree(root):
    root = Path(root)
    issues = []
    count = 0
    for path in sorted(root.rglob('*')):
        name = path.relative_to(root).as_posix()
        if path.is_symlink():
            issues.append({'file': name, 'rule': 'symlink_in_release'})
        elif path.is_file():
            count += 1
            data = path.read_bytes()
            issues.extend({'file': name, 'rule': rule} for rule in inspect_bytes(name, data))
            if path.suffix == '.zip':
                issues.extend(inspect_zip(data, name+'!'))
    return {'files_scanned': count, 'findings': issues, 'passed': not issues}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('path', type=Path)
    args = parser.parse_args()
    if args.path.is_dir():
        result = inspect_tree(args.path)
    else:
        issues = inspect_zip(args.path.read_bytes())
        result = {'findings': issues, 'passed': not issues}
    print(json.dumps(result, ensure_ascii=False, indent=2))
    raise SystemExit(0 if result['passed'] else 1)


if __name__ == '__main__':
    main()
