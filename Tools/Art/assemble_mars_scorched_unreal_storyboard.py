"""Package the actual MRQ frames and current geometry/render/continuous-PIE reports."""
import json
import shutil
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageStat

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / 'SourceArt/MarsDestruction/ScorchedFractureV3'
manifest = json.loads((ART / 'manifest.json').read_text())
FRAMES = ROOT / manifest.get('cinematic_frames_directory', 'Saved/MarsDestruction/offline_r8')
revision = manifest['revision']
fps=manifest['video_fps']
canvas = Image.new('RGB', (1920, 1032), (10, 14, 22))
draw = ImageDraw.Draw(canvas)
font = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', 23)
small = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', 18)
draw.text((20, 15), f'MARS 修订 {revision} · 裂缝漏光 → 岩片塌落 → 断口撕开 → 缓慢外移',
          font=font, fill=(235, 235, 240))
draw.text((20, 51), f'Unreal 独立电影场景 · 原有星空 · 1920×1080 / 逐帧 {fps} fps / 40 秒',
          font=small, fill=(155, 170, 190))
shots = [(0, '完整地表'), (4, '裂沟加深、岩片向内塌落'), (6, '各地裂隙漏光增强'),
         (7, '继续蓄压，保留同一漏光效果'), (8, '内部光最强，所有主块同时释放'), (9, '同步外移，漏光开始衰减'),
         (13, '分层热流从内向外推进'), (20, '实心内部，持续减速外移'), (26, '慢速段连续逐帧运动'),
         (32, '外移持续到渐黑'), (33.5, '全黑期间替换模型与相机'), (37, '第二套探索残骸与星空')]
for i, (seconds, label) in enumerate(shots):
    frame=round(seconds*fps)
    with Image.open(FRAMES / f'Mars_{frame:04d}.png') as source:
        picture = source.convert('RGB')
    if seconds == 33.5:
        assert max(ImageStat.Stat(picture).mean) == 0
    else:
        assert sum(ImageStat.Stat(picture).mean) > 1
    x, y = i % 4 * 480, 88 + i // 4 * 312
    canvas.paste(picture.resize((480, 270), Image.Resampling.LANCZOS), (x, y))
    draw.text((x + 12, y + 276), f'{seconds:g}s  {label}', font=small, fill=(235, 175, 110))
canvas.save(ART / 'Unreal_Storyboard_V3.jpg', quality=94)
for source, name in [
    (ROOT / 'Saved/MarsDestruction/scorched_geometry_validation.json', 'validation_geometry.json'),
    (FRAMES / 'render_report.json', 'validation_unreal_render.json'),
    (ROOT / 'Saved/MarsDestruction/unreal_scorched_runtime/validation.json', 'validation_unreal_play.json')]:
    report = json.loads(source.read_text())
    expected_revision = manifest.get('geometry_revision', revision) if name == 'validation_geometry.json' else revision
    assert report['result'] == 'PASS' and report['revision'] == expected_revision, (source, report)
    shutil.copy2(source, ART / name)
print('UNREAL_STORYBOARD_OK revision', revision)
