"""Fetch pinned, licensed source archives. Never downloads executable code."""
import concurrent.futures
import hashlib
import json
from pathlib import Path
import shutil
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'SourceArt/FrontEnd'
EXT = OUT / 'External'
EXT.mkdir(parents=True, exist_ok=True)


def download(url, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists():
        request = urllib.request.Request(url, headers={'User-Agent': 'JumpToSpace-AssetAuthoring/1.0'})
        with urllib.request.urlopen(request, timeout=180) as response:
            path.write_bytes(response.read())
    return {'file': path.relative_to(OUT).as_posix(), 'url': url,
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'bytes': path.stat().st_size}


def fetch():
    api = 'https://api.polyhaven.com/files/hangar_concrete_floor'
    data = json.load(urllib.request.urlopen(urllib.request.Request(api, headers={'User-Agent': 'JTS'})))
    (EXT / 'polyhaven_files.json').write_text(json.dumps(data, indent=2), encoding='utf-8')
    jobs = [(data[key]['2k']['jpg']['url'], EXT / ('hangar_' + name + '.jpg'))
            for key, name in [('Diffuse', 'diffuse'), ('nor_dx', 'normal_dx'), ('arm', 'arm')]]
    jobs += [
        ('https://github.com/adobe-fonts/source-han-sans/releases/download/2.005R/09_SourceHanSansSC.zip', EXT / '09_SourceHanSansSC_2.005R.zip'),
        ('https://raw.githubusercontent.com/adobe-fonts/source-han-sans/2.005R/LICENSE.txt', EXT / 'OFL_SourceHanSans.txt'),
        ('https://polyhaven.com/license', EXT / 'PolyHaven_license.html'),
    ]
    with concurrent.futures.ThreadPoolExecutor(max_workers=5) as executor:
        sources = list(executor.map(lambda job: download(*job), jobs))
    with zipfile.ZipFile(EXT / '09_SourceHanSansSC_2.005R.zip') as archive:
        for name in archive.namelist():
            if name.endswith(('SourceHanSansSC-Regular.otf', 'SourceHanSansSC-Bold.otf')):
                (EXT / Path(name).name).write_bytes(archive.read(name))
    for name in ['Casual_2_reference.fbx', 'LICENSE_Quaternius.txt']:
        destination = EXT / name
        shutil.copy2(ROOT / 'SourceArt/SkeletonAstronaut' / name, destination)
        sources.append({'file': destination.relative_to(OUT).as_posix(),
                        'url': 'https://quaternius.com/packs/ultimatemodularcharacters.html',
                        'sha256': hashlib.sha256(destination.read_bytes()).hexdigest(),
                        'usage': 'Skeleton compatibility reference only; avatar geometry is original.'})
    (OUT / 'sources.json').write_text(json.dumps({
        'sources': sources,
        'licenses': {'floor': 'CC0-1.0', 'rig_reference': 'CC0-1.0', 'font': 'OFL-1.1'},
        'ship_source': 'SourceArt/TwinforkR1; Tools/Art/generate_twinfork_meshes.py (project original)',
        'reference_images': 'User-provided visual direction only; no screenshot pixels, logo or models extracted.',
    }, indent=2), encoding='utf-8')
    print('Fetched', len(sources), 'source files; licenses and SHA-256 recorded.')


if __name__ == '__main__':
    fetch()
