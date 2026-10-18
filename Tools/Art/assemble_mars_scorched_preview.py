"""Package V3 with its actual blackout timing; raw render frames stay untouched."""
import json
import shutil
import subprocess
from pathlib import Path
from PIL import Image, ImageDraw, ImageEnhance, ImageFont

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / 'SourceArt/MarsDestruction/ScorchedFractureV3'
data = json.loads((ART / 'manifest.json').read_text())
source = ROOT / data['render_frames_directory']
processed = ROOT / 'Saved/MarsDestruction/frames_scorched_blackout'
processed.mkdir(parents=True, exist_ok=True)
lines = []
for frame in range(0, data['frames'][1] + 1, data['render_step']):
    picture = Image.open(source / f'{frame:04d}.png').convert('RGB')
    t = frame / data['fps']
    factor = 1. if t < 32 else max(0., 33-t) if t < 34 else min(1., (t-34)/1.5)
    if factor < 1:
        picture = ImageEnhance.Brightness(picture).enhance(factor)
    path = processed / f'{frame:04d}.png'
    picture.save(path)
    lines.extend(["file '" + path.as_posix() + "'", f'duration {data["render_step"] / data["fps"]:.9f}'])
concat = processed / 'concat.txt'
concat.write_text('\n'.join(lines)+'\n', encoding='utf-8')
subprocess.run([shutil.which('ffmpeg'), '-y', '-hide_banner', '-loglevel', 'error', '-f', 'concat',
                '-safe', '0', '-i', str(concat), '-vf', 'fps=30,format=yuv420p', '-c:v', 'libx264',
                '-crf', '18', '-movflags', '+faststart', str(ART / 'Mars_Scorched_Fracture_V3.mp4')], check=True)
font = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', 23)
small = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', 18)
canvas = Image.new('RGB', (1920, 1032), (10, 14, 22))
draw = ImageDraw.Draw(canvas)
draw.text((20, 15), f'MARS V3 修订 {data["revision"]} · 沟壑渐深 → 掀裂 → 释放 / 分层热流、稀疏余热', font=font, fill=(235,235,240))
draw.text((20, 51), 'Blender 预览 · 光源固定于球心、由岩体遮挡 · 黑屏前持续外移 · 游戏内效果见 Unreal 实拍', font=small, fill=(155,170,190))
shots = [(0,'完整火星'), (90,'地表先出现浅沟壑'), (144,'沟壑加深，主体仍未平移'),
         (180,'局部掀裂，内部透光'), (225,'交错撕扯，压力上升'), (270,'主破裂释放，开始明显外移'),
         (330,'亮黄热核与橙红推进前缘'), (390,'分叉热流从内部涌向地表'), (570,'辉光熄灭，热流继续扩散'),
         (960,'炭化地表、稀疏余热，仍在外移'),
         (1005,'完全黑屏期间替换'), (1110,'返回远处视角：第二套残骸')]
for i,(frame,label) in enumerate(shots):
    x,y=i%4*480,88+i//4*312
    picture=Image.open(processed/f'{frame:04d}.png').convert('RGB').resize((480,270),Image.Resampling.LANCZOS)
    canvas.paste(picture,(x,y))
    draw.text((x+12,y+276),f'{frame/30:g}s  {label}',font=small,fill=(235,175,110))
canvas.save(ART/'Storyboard_V3.jpg',quality=94)
print('MARS_SCORCHED_PREVIEW',ART)
