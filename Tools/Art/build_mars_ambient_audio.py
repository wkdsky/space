"""Deterministic, original room ambience and short mechanical/ice effects."""
from array import array
import math
from pathlib import Path
import random
import wave

DEST = Path(__file__).resolve().parents[2] / 'SourceArt/MarsSystem/Audio'
RATE = 48000


def write(name, seconds, kind):
    rng = random.Random(921 + kind)
    data = array('h')
    smooth = 0.0
    for i in range(int(seconds * RATE)):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        smooth += (noise - smooth) * .018
        if kind == 0:
            v = .12 * math.sin(2*math.pi*65*t) + .05*math.sin(2*math.pi*130*t) + smooth*.22
        elif kind == 1:
            v = (.11*math.sin(2*math.pi*45*t) + smooth*.7) * (.8+.2*math.sin(2*math.pi*t/3))
        elif kind == 2:
            flutter = .5+.5*math.sin(2*math.pi*21*t + .4*math.sin(2*math.pi*3*t))
            v = (noise*.045 + smooth*.18)*flutter
        elif kind == 3:
            envelope = sum(math.exp(-(t-start)*rate) for start, rate in ((0,35),(.11,28),(.28,20)) if t>=start)
            v = (noise*.4 + math.sin(2*math.pi*(850*t-160*t*t))*.1)*envelope
        else:
            v = (.17*math.sin(2*math.pi*(1700*t-2000*t*t)) + noise*.04)*math.exp(-t*12)
        edge = min(1.0, t/.015, (seconds-t)/.025)
        data.append(int(max(-1, min(1, v * edge)) * 32767))
    with wave.open(str(DEST / (name+'.wav')), 'wb') as f:
        f.setnchannels(1); f.setsampwidth(2); f.setframerate(RATE); f.writeframes(data.tobytes())


if __name__ == '__main__':
    DEST.mkdir(parents=True, exist_ok=True)
    for name, seconds, kind in [('S_Mars_UndergroundHum',6,0),('S_Mars_Furnace',6,1),('S_Mars_MothFlutter',6,2),('S_Mars_IceCrack',1.1,3),('S_Mars_CameraOff',.35,4)]:
        write(name,seconds,kind)
        print(name)
