"""Original short mining foley and a small contact scar mask."""
from pathlib import Path
import math
import random
import struct
import wave
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[2] / 'SourceAssets' / 'Mining'
RATE = 48000


def sound(name, kind, seed):
    rng = random.Random(seed)
    breaking = kind.endswith('break')
    duration = .17 if kind == 'swing' else .64 if breaking else .28
    samples, low, mid = [], 0., 0.
    taps = [(i*.045+rng.random()*.015, 1-i*.085) for i in range(8)] if breaking else [(0., 1.)]
    for i in range(int(RATE*duration)):
        t = i/RATE
        white = rng.uniform(-1, 1)
        low += .035*(white-low)
        mid += .35*(white-mid)
        if kind == 'swing':
            envelope = math.sin(math.pi*t/duration)**2
            value = (mid-low)*envelope*.65
        else:
            value = 0.
            for at, gain in taps:
                age = t-at
                if age < 0: continue
                grit = (mid-low)*math.exp(-age*23)
                crack = white*math.exp(-age*180)
                body = math.sin(2*math.pi*(105*age-90*age*age))*math.exp(-age*34)
                ring = (math.sin(2*math.pi*1387*age)+.35*math.sin(2*math.pi*2411*age))*math.exp(-age*57)
                if kind.startswith('soil'):
                    value += gain*(low*2.8*math.exp(-age*14)+body*.35+crack*.13+grit*.25)
                else:
                    value += gain*(crack*.45+body*.36+grit*.80+ring*.10)
        samples.append(value * min(1., max(0., (duration-t)/.025)))
    peak = max(abs(v) for v in samples)
    level = .48 if kind == 'swing' else .82
    pcm = b''.join(struct.pack('<h',int(max(-1,min(1,v*level/max(.001,peak)))*32767)) for v in samples)
    with wave.open(str(ROOT/(name+'.wav')),'wb') as out:
        out.setnchannels(1); out.setsampwidth(2); out.setframerate(RATE); out.writeframes(pcm)


def build():
    ROOT.mkdir(parents=True, exist_ok=True)
    for name,kind,seed in [('S_PickaxeWhoosh','swing',811),('S_StoneChip_A','stone',901),('S_StoneChip_B','stone',902),
            ('S_SoilStrike_A','soil',903),('S_SoilStrike_B','soil',904),('S_RockBreak','break',905),('S_SoilCollapse','soil_break',906)]:
        sound(name,kind,seed)
    mask=Image.new('L',(512,512),0); draw=ImageDraw.Draw(mask); rng=random.Random(731)
    draw.polygon([(232,249),(252,228),(279,249),(269,280),(245,278)],fill=210)
    for branch in range(7):
        angle=branch*math.tau/7+rng.uniform(-.3,.3); point=(256.,256.)
        for step in range(5):
            length=rng.uniform(18,31); angle+=rng.uniform(-.45,.45)
            end=(point[0]+math.cos(angle)*length,point[1]+math.sin(angle)*length)
            draw.line([point,end],fill=max(0,210-step*33),width=max(1,7-step))
            if step==2:
                draw.line([end,(end[0]+math.cos(angle+.8)*30,end[1]+math.sin(angle+.8)*30)],fill=110,width=2)
            point=end
    mask=mask.filter(ImageFilter.GaussianBlur(.6))
    image=Image.new('RGBA',(512,512),(24,21,18,0)); image.putalpha(mask)
    image.save(ROOT/'T_MiningContactScar.png')
    print('Generated mining foley and contact mask in', ROOT)


if __name__ == '__main__': build()
