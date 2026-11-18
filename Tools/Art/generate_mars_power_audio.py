import math,wave,struct
from pathlib import Path
rate=48000;length=2.2;buf=[]
for i in range(int(rate*length)):
 t=i/rate;s=0
 # Contactors, a short electrical rise, then a restrained three-note confirmation.
 for start,freq in ((.05,110),(.28,165),(.55,220)):
  q=t-start
  if 0<q<.12:s+=.22*math.exp(-q*40)*math.sin(2*math.pi*freq*q)
 for start,freq in ((.8,440),(1.04,554.37),(1.28,659.25)):
  q=t-start
  if 0<q<.65:s+=.12*min(1,q/.018)*math.exp(-q*6)*math.sin(2*math.pi*freq*q)
 if .3<t<.8:s+=.025*math.sin(2*math.pi*(80+90*t)*t)
 buf.append(struct.pack('<h',int(max(-1,min(1,s))*32767)))
p=Path('SourceArt/MarsSystem/Audio/S_Mars_LiftPowerOn.wav')
with wave.open(str(p),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(rate);w.writeframes(b''.join(buf))
print(p)
