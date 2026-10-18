"""Package rendered Blender frames as an MP4 and a labelled contact sheet."""
from pathlib import Path
import json
import shutil
import subprocess
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'SourceArt/MarsDestruction'
manifest = json.loads((OUT / 'prototype_manifest.json').read_text(encoding='utf-8'))
FRAMES = ROOT / manifest['render_frames_directory']
concat = ROOT / 'Saved/MarsDestruction/preview_concat.txt'
lines = []
for frame in range(0, manifest['frames'][1] + 1, manifest['render_step']):
    path = FRAMES / f'{frame:04d}.png'
    assert path.is_file(), path
    lines.extend(["file '" + path.as_posix() + "'", f"duration {manifest['render_step'] / manifest['fps']:.6f}"])
concat.write_text('\n'.join(lines) + '\n', encoding='utf-8')
ffmpeg = shutil.which('ffmpeg')
assert ffmpeg, 'ffmpeg is required on PATH'
subprocess.run([ffmpeg, '-y', '-hide_banner', '-loglevel', 'error', '-f', 'concat',
                '-safe', '0', '-i', str(concat), '-vf', 'fps=30,format=yuv420p',
                '-c:v', 'libx264', '-crf', '19', '-movflags', '+faststart',
                str(OUT / 'Mars_Explosion_Preview.mp4')], check=True)
font = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', 22)
small = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', 17)
canvas = Image.new('RGB', (1920, 848), (10, 14, 22))
draw = ImageDraw.Draw(canvas)
draw.text((24, 16), 'MARS 板块缓慢分离 · 大块慢、小块稍快，释放后持续减速', font=font, fill=(235, 235, 240))
draw.text((24, 49), '28 秒版本 · 末态保持第二个模型原始布局 · 无额外抛飞地壳、无冲击环 · 联机触发与探索碰撞待接入',
          font=small, fill=(150, 165, 184))
shots = [(0, '0 s  完整火星'), (90, '3 s  裂缝开始张开'), (150, '5 s  大板块逐渐显露'),
         (240, '8 s  板块缓慢外移'), (360, '12 s  表面交接完成'), (480, '16 s  继续减速漂移'),
         (660, '22 s  逐渐接近末态'), (840, '28 s  原始破碎布局')]
for i, (frame, label) in enumerate(shots):
    x, y = (i % 4) * 480, 88 + (i // 4) * 380
    picture = Image.open(FRAMES / f'{frame:04d}.png').convert('RGB').resize((480, 320), Image.Resampling.LANCZOS)
    canvas.paste(picture, (x, y))
    draw.text((x + 16, y + 329), label, font=font, fill=(235, 170, 110))
canvas.save(OUT / 'Storyboard.jpg', quality=92)
print('Mars preview packaged:', OUT)
