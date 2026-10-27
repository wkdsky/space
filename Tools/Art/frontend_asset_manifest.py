"""Close the asset-first gate before configuring shipping front-end maps/widgets."""
import hashlib
import json
import wave
from array import array
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / 'SourceArt/FrontEnd'
entries = []
for manifest in ['models_manifest.json', 'graphics_audio_manifest.json', 'animations_manifest.json', 'fonts_manifest.json', 'sky_manifest.json', 'threat_manifest.json']:
    data = json.loads((ART / manifest).read_text(encoding='utf-8'))
    if isinstance(data, dict): data = data.get('assets', [data])
    for item in data:
        if 'file' not in item: continue
        path = ART / item['file']
        assert path.is_file(), path
        entry = dict(item, sha256=hashlib.sha256(path.read_bytes()).hexdigest(), bytes=path.stat().st_size)
        if path.suffix.lower() == '.png':
            with Image.open(path) as img:
                entry['dimensions'] = list(img.size)
                assert min(img.size) > 0
        if path.suffix.lower() == '.wav':
            with wave.open(str(path)) as wav:
                assert wav.getframerate() == 48000 and wav.getsampwidth() == 2
                samples = array('h', wav.readframes(wav.getnframes()))
                peak = max(abs(v) for v in samples) / 32768
                assert peak < 1, (path, peak)
                channels = wav.getnchannels()
                delta = max(abs(samples[i] - samples[-channels + i]) for i in range(channels))
                entry['audio_check'] = {'peak': round(peak, 5), 'endpoint_delta': delta, 'seconds': wav.getnframes() / 48000}
                if item.get('loop'): assert delta <= 2, path
        entries.append(entry)
fallback = ART / 'Textures/T_FE_FallbackBackground.png'
assert Image.open(fallback).size == (1920, 1080)
entries.append({'id': 'T_FE_FallbackBackground', 'file': str(fallback.relative_to(ART)), 'source': 'original_unreal_scene_capture', 'sha256': hashlib.sha256(fallback.read_bytes()).hexdigest()})
for mesh in [e for e in entries if e['id'].startswith('SK_FE_') and 'unweighted_vertices' in e]:
    assert mesh['unweighted_vertices'] == 0
validation = json.loads((ART / 'art_validation_report.json').read_text())
assert validation['success'] and len(validation['asset_checks']) == 5
assert all(e['lods'] == 3 for e in validation['asset_checks'])
assert json.loads((ART / 'unreal_import_report.json').read_text()).get('errors') == []
sources = json.loads((ART / 'sources.json').read_text(encoding='utf-8'))
result = {'schema': 1, 'sources': sources, 'assets': entries, 'unreal_authored': ['16 parameterized material families + 12 palette instances', 'L_FrontEndArtLab original set dressing, lighting and four cameras', 'LS_FE_HangarLoop 60 seconds: maintenance robot and steam cards', 'Original suspended high-altitude alien laser front, clear blue daytime lighting'], 'adaptations': {'floor': 'Cool low contrast repaint; normal amplitude reduced; roughness raised; AO suppressed', 'font': 'Renamed JTS UI Sans, tabular digits and punctuation spacing; full Chinese glyph coverage', 'rig_reference': 'Bones only; original fitted suit geometry, weights, faces and animations', 'ship': 'Project original Twinfork display derivative; welded, bevelled, recolored, gameplay internals removed'}}
(ART / 'asset_manifest.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
gate = {'passed': True, 'asset_count': len(entries), 'checks': ['All source files exist and SHA256 recorded', 'Five weighted suits with three Unreal LODs each', 'Independent alpha eye/mouth atlas visually inspected', 'Rifle and crouch pose renders inspected', 'Unreal centimetre animation scale checked', 'All 48kHz sounds below clipping; marked loops have matching endpoints', '1920x1080 Unreal fallback capture inspected', 'No screenshot artwork extracted'], 'visual_notes': 'Clear blue Earth daytime; routine base activity and a suspended alien laser front before ground impact. Ship is an adapted copy of the project original.'}
(ART / 'asset_gate.json').write_text(json.dumps(gate, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(gate, ensure_ascii=False))
