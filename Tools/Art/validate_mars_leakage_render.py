"""Measure warm leaked light outside the intact planet in the final MRQ frames.

The fixed annulus avoids the rock surface during the measured 4..11s interval.
R-B suppresses the neutral starfield; this is an image-space art check, not a
physical luminance measurement. Continuous PIE separately checks source curves.
"""
import json
from pathlib import Path
from PIL import Image,ImageChops,ImageDraw,ImageStat

ROOT=Path(__file__).resolve().parents[2]
ART=ROOT/'SourceArt/MarsDestruction/ScorchedFractureV3'
data=json.loads((ART/'manifest.json').read_text())
folder=ROOT/data['cinematic_frames_directory']
render=json.loads((folder/'render_report.json').read_text())
assert render['result']=='PASS' and render['revision']==data['revision']
mask=Image.new('L',(1920,1080),0)
draw=ImageDraw.Draw(mask)
draw.ellipse((470,50,1450,1030),fill=255)
draw.ellipse((620,200,1300,880),fill=0)
samples=[]
for t in (4.,5.,6.,7.,7.5,8.,8.5,9.,10.,11.):
    with Image.open(folder/f'Mars_{round(t*data["video_fps"]):04d}.png') as picture:
        r,g,b=picture.convert('RGB').split()
    warm=ImageChops.subtract(r,b)
    quadrants=[]
    for box in ((0,0,960,540),(960,0,1920,540),(0,540,960,1080),(960,540,1920,1080)):
        quadrants.append(ImageStat.Stat(warm.crop(box),mask.crop(box)).mean[0])
    samples.append(dict(seconds=t,mean_warm_contrast=ImageStat.Stat(warm,mask).mean[0],quadrants=quadrants))
values=[s['mean_warm_contrast'] for s in samples]
assert all(b>a for a,b in zip(values[:5],values[1:6])),samples
# Aperture growth can briefly compensate declining radiance, but must not
# produce the previous broad increase through 9..11s after release.
assert values[6]<=values[5]*1.03,samples
assert values[6]>values[7]>values[8]>values[9],samples
assert all(q>0 for q in samples[3]['quadrants']),samples[3]
report=dict(revision=data['revision'],result='PASS',metric='mean R-minus-B outside rock, fixed annulus 340..490 px',
            increasing_until_release=True,post_release_light_declines=True,
            samples=samples)
(ART/'validation_leakage_render.json').write_text(json.dumps(report,indent=2)+'\n')
print('LEAKAGE_RENDER_PASS',json.dumps(report))
