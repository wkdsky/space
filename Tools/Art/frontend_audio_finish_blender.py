"""Blender MCP finishing pass: eliminate clicks at one-shot PCM boundaries."""
import json
import wave
import numpy as np
from pathlib import Path
ROOT=Path('D:/projects/space/SourceArt/FrontEnd')
entries=json.loads((ROOT/'graphics_audio_manifest.json').read_text())
count=0
for e in entries:
    if not e['file'].endswith('.wav') or e.get('loop'):continue
    p=ROOT/e['file']
    with wave.open(str(p)) as w:
        channels=w.getnchannels();rate=w.getframerate()
        data=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(float).reshape(-1,channels)
    fade=np.linspace(0,1,min(rate//200,len(data)//2))[:,None]
    data[:len(fade)]*=fade;data[-len(fade):]*=fade[::-1]
    with wave.open(str(p),'wb') as w:
        w.setnchannels(channels);w.setsampwidth(2);w.setframerate(rate);w.writeframes(data.astype('<i2').tobytes())
    count+=1
print('Finished one-shot endpoint fades:',count)
