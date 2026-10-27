#!/usr/bin/env python3
"""Package the maintained additions, without credentials, caches or board backups."""
from pathlib import Path
import zipfile

REPO=Path(__file__).resolve().parents[1]
OUTPUT=REPO.parent/'outputs'
PROJECT=REPO/'examples/esp-idf/06_OrbitClockAI'
APP=REPO/'examples/arduino/08_OrbitClock'


def main():
    files=[p for folder in (PROJECT,APP) for p in folder.rglob('*') if p.is_file()
           and p.name!='clock_secrets.h' and not {'__pycache__','build','managed_components','.git'}.intersection(p.parts)]
    files += [REPO/'scripts'/name for name in ('orbit_ai.py','orbit_ai_artifacts.py','orbit_clock.py',
        'generate_orbit_ai_port.py','generate_orbit_ai_font.py','generate_orbit_ai_assets.py','generate_orbit_fonts.py',
        'check_orbit_ai_ui.py','check_orbit_ai_emotion.py','check_release_privacy.py','package_orbit_ai_release.py',
        'package_orbit_ai_source.py','generate_retro_assets.py','generate_pikachu_audio.py')]
    files += sorted((REPO/'tests').glob('test_*clock*.py'))
    files += [p for p in (REPO/'tests').glob('test_*.py') if any(x in p.stem for x in ('orbit_ai','dice','sand','player'))]
    files.append(REPO/'tests/test_release_privacy.py')
    files=sorted(set(files))
    destination=OUTPUT/'OrbitClockAI-source.zip'
    with zipfile.ZipFile(destination,'w',zipfile.ZIP_DEFLATED) as archive:
        for path in files:archive.write(path,Path(REPO.name)/path.relative_to(REPO))
        for name in ('OrbitClockAI.code-workspace','OrbitClockAI-验证说明.md'):
            path=OUTPUT/name
            if path.exists():archive.write(path,Path('outputs')/name)
        archive.writestr('README.txt','Custom Orbit Clock AI additions. Apply over the Waveshare Git checkout.\n'
            'See examples/esp-idf/06_OrbitClockAI/README_ZH.md in the repository directory.\n'
            'The official XiaoZhi checkout, ESP-IDF, bundled Waveshare libraries, caches and firmware are not included.\n')
    with zipfile.ZipFile(destination) as archive:
        assert archive.testzip() is None
        assert not any('clock_secrets.h' in p or '/build/' in p or 'board-backups' in p for p in archive.namelist())
        for path in files:
            assert archive.read(str(Path(REPO.name)/path.relative_to(REPO)))==path.read_bytes()
    print(f'源码包已校验：{destination} ({destination.stat().st_size} bytes)')

if __name__=='__main__':main()
