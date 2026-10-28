"""Refresh the production source inventory without generating QA evidence."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / 'SourceArt/FrontEnd'
manifest_path = ART / 'asset_manifest.json'
manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
# The checked-in inventory is curated. New assets are admitted by adding their source file here.
metadata = {}
for name in ['models_manifest.json', 'graphics_audio_manifest.json', 'animations_manifest.json',
             'fonts_manifest.json', 'sky_manifest.json', 'threat_manifest.json']:
    data = json.loads((ART / name).read_text(encoding='utf-8'))
    for entry in data if isinstance(data, list) else data.get('assets', [data]):
        if 'file' in entry:
            metadata[entry['file']] = entry
entries = []
for entry in manifest['assets']:
    entry = dict(entry, **metadata.get(entry['file'], {}))
    path = ART / entry['file']
    assert path.is_file(), path
    entry.update(sha256=hashlib.sha256(path.read_bytes()).hexdigest(), bytes=path.stat().st_size)
    entries.append(entry)
manifest['assets'] = entries
manifest['sources'] = json.loads((ART / 'sources.json').read_text(encoding='utf-8'))
manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print('Updated', len(entries), 'production source records')
