#!/usr/bin/env python3
"""Package and validate the five separate images without touching the NVS range."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import shutil
import struct
import zipfile

LAYOUT={0x0:('bootloader.bin',0x8000),0x8000:('partition-table.bin',0x1000),
        0xe000:('ota_data_initial.bin',0x2000),0x20000:('orbit-clock-ai.bin',0x3f0000),
        0x800000:('assets.bin',0x800000)}
NVS=(0x9000,0xe000)


def partition_entries(data:bytes)->dict[str,tuple[int,int]]:
    result={}
    for offset in range(0,len(data)-31,32):
        magic,kind,subtype,address,size,label,flags=struct.unpack_from('<HBBII16sI',data,offset)
        if magic!=0x50aa:break
        name=label.split(b'\0')[0].decode('ascii');result[name]=(address,size)
    return result


def validate(directory:Path)->list[tuple[str,Path]]:
    manifest=json.loads((directory/'flash_manifest.json').read_text())
    if manifest.get('board')!='orbit-clock-ai' or manifest.get('format')!=1:
        raise RuntimeError('不是 Orbit Clock AI 固件清单。')
    images=[];seen=set()
    for item in manifest['files']:
        address=int(item['offset'],0)
        if address not in LAYOUT or address in seen:raise RuntimeError('固件烧录地址不正确。')
        name,limit=LAYOUT[address]
        if item['name']!=name:raise RuntimeError('固件文件名不正确。')
        path=directory/name;data=path.read_bytes()
        if not data or len(data)>limit or len(data)!=item['size']:raise RuntimeError(f'固件长度不正确：{name}')
        if hashlib.sha256(data).hexdigest()!=item['sha256']:raise RuntimeError(f'固件校验失败：{name}')
        if address<NVS[1] and address+len(data)>NVS[0]:raise RuntimeError('固件写入范围覆盖现有设置。')
        seen.add(address);images.append((hex(address),path))
    if seen!=set(LAYOUT):raise RuntimeError('缺少合并版必需的固件文件。')
    partitions=partition_entries((directory/'partition-table.bin').read_bytes())
    required={'nvs':(0x9000,0x5000),'otadata':(0xe000,0x2000),'ota_0':(0x20000,0x3f0000),
              'ota_1':(0x410000,0x3f0000),'assets':(0x800000,0x800000)}
    if any(partitions.get(name)!=value for name,value in required.items()):raise RuntimeError('合并版分区表不匹配。')
    return sorted(images,key=lambda item:int(item[0],0))


def package(build:Path,destination:Path,revision:str)->None:
    args=json.loads((build/'flasher_args.json').read_text())
    actual={int(address,0):path for address,path in args['flash_files'].items()}
    if set(actual)!=set(LAYOUT):raise RuntimeError('编译输出地址与预期分区不一致。')
    destination.mkdir(parents=True,exist_ok=True);files=[]
    for address,(name,limit) in LAYOUT.items():
        path=build/actual[address];data=path.read_bytes()
        if not data or len(data)>limit:raise RuntimeError(f'编译输出大小错误：{name}')
        shutil.copy2(path,destination/name)
        files.append({'offset':hex(address),'name':name,'size':len(data),'sha256':hashlib.sha256(data).hexdigest()})
    manifest={'format':1,'board':'orbit-clock-ai','xiaozhi_revision':revision,'files':files}
    (destination/'flash_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    validate(destination)
    (destination/'SHA256SUMS').write_text(''.join(f"{item['sha256']}  {item['name']}\n" for item in files))
    guide=Path(__file__).resolve().parents[1]/'examples/esp-idf/06_OrbitClockAI/README_ZH.md'
    if guide.exists():shutil.copy2(guide,destination/'README_ZH.md')
    archive=destination.with_suffix('.zip')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as output:
        for path in destination.iterdir():
            if path.is_file():output.write(path,destination.name+'/'+path.name)
    print(f'已生成并校验：{destination}（5 个分区镜像，不写入 NVS）',flush=True)
