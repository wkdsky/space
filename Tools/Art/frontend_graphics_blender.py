"""Original vector UI, face atlas, adapted surface textures and synthesized audio.

Run in Blender MCP. Rasterization uses numpy and Blender's PNG writer; no stock icons.
"""
import bpy
import numpy as np
import math
import json
import wave
from pathlib import Path

OUT=Path('D:/projects/space/SourceArt/FrontEnd')
PRODUCTION_FILES={e['file'] for e in json.loads((OUT/'asset_manifest.json').read_text(encoding='utf-8'))['assets']}
ENTRIES=[]


def png(name, pixels, folder='UI', origin='original_blender_procedural'):
    if folder+'/'+name+'.png' not in PRODUCTION_FILES:return
    pixels=np.clip(pixels,0,1).astype(np.float32)
    h,w=pixels.shape[:2]
    if pixels.shape[2]==3:pixels=np.dstack([pixels,np.ones((h,w),dtype=np.float32)])
    image=bpy.data.images.new(name,width=w,height=h,alpha=True)
    image.colorspace_settings.name='sRGB'
    image.pixels.foreach_set(np.flipud(pixels).ravel())
    path=OUT/folder/(name+'.png');path.parent.mkdir(parents=True,exist_ok=True)
    image.filepath_raw=str(path);image.file_format='PNG';image.save()
    ENTRIES.append({'id':name,'source':origin,'file':path.relative_to(OUT).as_posix(),'size':[w,h]})
    bpy.data.images.remove(image)
    return path


def grid(n=128):
    return np.meshgrid((np.arange(n)+.5)/n,(np.arange(n)+.5)/n)


def segment(x,y,a,b,width=.035):
    dx,dy=b[0]-a[0],b[1]-a[1]
    t=np.clip(((x-a[0])*dx+(y-a[1])*dy)/max(dx*dx+dy*dy,1e-9),0,1)
    d=np.sqrt((x-a[0]-t*dx)**2+(y-a[1]-t*dy)**2)
    return np.clip((width-d)*x.shape[0]+.5,0,1)


def stroke(x,y,points,width=.035):
    mask=np.zeros_like(x)
    for a,b in zip(points,points[1:]):mask=np.maximum(mask,segment(x,y,a,b,width))
    return mask


def circle(x,y,c,r,fill=False,width=.035):
    distance=np.sqrt((x-c[0])**2+(y-c[1])**2)
    return np.clip(((r-distance) if fill else width-np.abs(distance-r))*x.shape[0]+.5,0,1)


def rect(x,y,a,b,width=.035,fill=False):
    if fill:return ((x>=a[0])&(x<=b[0])&(y>=a[1])&(y<=b[1])).astype(float)
    return stroke(x,y,[a,(b[0],a[1]),b,(a[0],b[1]),a],width)


def arc(x,y,c,rx,ry,start,end,width=.035):
    points=[(c[0]+rx*math.cos(t),c[1]+ry*math.sin(t)) for t in np.linspace(start,end,40)]
    return stroke(x,y,points,width)


def rgba(mask,color=(.94,.92,.84)):
    out=np.zeros((*mask.shape,4),dtype=np.float32);out[:,:,:3]=color;out[:,:,3]=mask
    return out


x,y=grid(256)
face=[]
for category in ['Eye','Mouth']:
    for choice in range(5):
        mask=np.zeros_like(x)
        if category=='Eye':
            for c in [.29,.71]:
                if choice==0:mask=np.maximum(mask,np.clip((1-np.sqrt(((x-c)/.075)**2+((y-.5)/.25)**2))*64+.5,0,1))
                elif choice==1:mask=np.maximum(mask,arc(x,y,(c,.64),.13,.25,math.pi,2*math.pi,.045))
                elif choice==2:
                    mask=np.maximum(mask,circle(x,y,(c,.49),.115,True)*(y>.49))
                    mask=np.maximum(mask,segment(x,y,(c-.14,.49),(c+.14,.49),.035))
                elif choice==3:mask=np.maximum(mask,segment(x,y,(c-.11,.35 if c<.5 else .6),(c+.11,.6 if c<.5 else .35),.055))
                else:
                    points=[(c+(.16 if i%2==0 else .065)*math.sin(i*math.pi/5),.5-(.29 if i%2==0 else .12)*math.cos(i*math.pi/5)) for i in range(11)]
                    mask=np.maximum(mask,stroke(x,y,points,.032))
        else:
            if choice==0:mask=segment(x,y,(.23,.50),(.77,.50),.043)
            elif choice==1:mask=arc(x,y,(.5,.29),.31,.36,0,math.pi,.042)
            elif choice==2:mask=arc(x,y,(.5,.50),.15,.29,0,math.tau,.04)
            elif choice==3:mask=arc(x,y,(.5,.75),.30,.34,math.pi,2*math.pi,.042)
            else:mask=stroke(x,y,[(.15,.4),(.28,.62),(.43,.45),(.57,.62),(.72,.45),(.85,.4)],.038)
        face.append(mask)
        png('T_FE_'+category+str(choice),rgba(mask))
# 2048 square, 4x4 cells, 32 pixels transparent padding around every original shape.
atlas=np.zeros((2048,2048,4),dtype=np.float32)
for i,mask in enumerate(face):
    tile=np.repeat(np.repeat(rgba(mask), 448//256+1,axis=0),448//256+1,axis=1)
    # Resample to 448 pixels by deterministic nearest indices (edges already antialiased).
    idx=np.minimum(255,(np.arange(448)*256/448).astype(int));tile=rgba(mask)[idx[:,None],idx[None,:]]
    row,col=divmod(i,4);atlas[row*512+32:row*512+480,col*512+32:col*512+480]=tile
png('T_FE_FaceAtlas',atlas,'Textures')

ICON_NAMES=['back','left','right','up','down','confirm','close','plus','minus','settings','astronaut','ship','save','delete','refresh','search','copy','lock','unlock','host','ready','unready','mic','micmute','speaker','network','warning','info','eye','mouth','random','reset']
x,y=grid(128)
for name in ICON_NAMES:
    m=np.zeros_like(x)
    def add(mask):
        global m
        m=np.maximum(m,mask)
    if name in ['left','back','right','up','down']:
        points=[(.62,.22),(.34,.5),(.62,.78)]
        if name=='right':points=[(1-a,b) for a,b in points]
        if name=='up':points=[(b,a) for a,b in points]
        if name=='down':points=[(b,1-a) for a,b in points]
        add(stroke(x,y,points))
        if name=='back':add(segment(x,y,(.35,.5),(.82,.5)))
    elif name in ['confirm','ready']:add(stroke(x,y,[(.2,.5),(.41,.72),(.81,.28)],.045))
    elif name in ['close','unready']:
        add(segment(x,y,(.25,.25),(.75,.75)));add(segment(x,y,(.25,.75),(.75,.25)))
    elif name in ['plus','minus']:
        add(segment(x,y,(.23,.5),(.77,.5)))
        if name=='plus':add(segment(x,y,(.5,.23),(.5,.77)))
    elif name=='settings':
        add(circle(x,y,(.5,.5),.24));add(circle(x,y,(.5,.5),.08))
        for a in np.linspace(0,math.tau,8,endpoint=False):add(segment(x,y,(.5+.25*math.cos(a),.5+.25*math.sin(a)),(.5+.36*math.cos(a),.5+.36*math.sin(a)),.045))
    elif name=='astronaut':
        add(arc(x,y,(.5,.49),.29,.32,0,math.tau));add(rect(x,y,(.27,.34),(.73,.56),.025));add(segment(x,y,(.32,.79),(.68,.79)))
    elif name=='ship':
        add(stroke(x,y,[(.5,.13),(.63,.36),(.84,.74),(.61,.63),(.5,.84),(.39,.63),(.16,.74),(.37,.36),(.5,.13)],.028));add(circle(x,y,(.5,.42),.07))
    elif name=='save':
        add(rect(x,y,(.22,.18),(.78,.82)));add(rect(x,y,(.34,.19),(.65,.39),.025));add(rect(x,y,(.32,.55),(.68,.82),.025))
    elif name=='delete':
        add(stroke(x,y,[(.29,.35),(.34,.81),(.66,.81),(.71,.35)]));add(segment(x,y,(.2,.27),(.8,.27)));add(stroke(x,y,[(.4,.24),(.4,.17),(.6,.17),(.6,.24)]))
        for a in [.44,.56]:add(segment(x,y,(a,.43),(a,.7),.02))
    elif name in ['refresh','reset']:
        add(arc(x,y,(.5,.5),.28,.28,-.3,5.1));add(stroke(x,y,[(.62,.18),(.77,.24),(.79,.08)],.03))
    elif name=='search':add(circle(x,y,(.43,.43),.22));add(segment(x,y,(.59,.59),(.81,.81),.045))
    elif name=='copy':add(rect(x,y,(.36,.31),(.77,.81)));add(stroke(x,y,[(.63,.21),(.22,.21),(.22,.69)],.028))
    elif name in ['lock','unlock']:
        add(rect(x,y,(.26,.45),(.74,.82)));add(arc(x,y,(.5,.43),.18,.23,math.pi,2*math.pi if name=='lock' else 1.85*math.pi));add(segment(x,y,(.5,.58),(.5,.68)))
    elif name=='host':add(stroke(x,y,[(.2,.33),(.34,.5),(.5,.23),(.66,.5),(.8,.33),(.73,.74),(.27,.74),(.2,.33)],.028))
    elif name in ['mic','micmute']:
        add(arc(x,y,(.5,.45),.12,.25,0,math.tau));add(arc(x,y,(.5,.46),.24,.26,0,math.pi));add(segment(x,y,(.5,.73),(.5,.84)));add(segment(x,y,(.36,.84),(.64,.84)))
        if name=='micmute':add(segment(x,y,(.2,.2),(.8,.8),.045))
    elif name=='speaker':add(stroke(x,y,[(.2,.41),(.35,.41),(.56,.23),(.56,.77),(.35,.59),(.2,.59),(.2,.41)],.025));add(arc(x,y,(.54,.5),.26,.28,-.9,.9))
    elif name=='network':
        for r in [.18,.30,.42]:add(arc(x,y,(.5,.80),r,r,math.pi*1.2,math.pi*1.8,.028))
        add(circle(x,y,(.5,.79),.04,True))
    elif name=='warning':add(stroke(x,y,[(.5,.15),(.86,.79),(.14,.79),(.5,.15)],.03));add(segment(x,y,(.5,.37),(.5,.56)));add(circle(x,y,(.5,.67),.032,True))
    elif name=='info':add(circle(x,y,(.5,.5),.32));add(segment(x,y,(.5,.46),(.5,.69)));add(circle(x,y,(.5,.32),.032,True))
    elif name=='eye':add(arc(x,y,(.5,.5),.34,.23,0,math.tau));add(circle(x,y,(.5,.5),.085,True))
    elif name=='mouth':add(arc(x,y,(.5,.35),.30,.30,0,math.pi));add(segment(x,y,(.2,.35),(.8,.35)))
    elif name=='random':
        add(rect(x,y,(.2,.2),(.8,.8),.03))
        for p in [(.34,.34),(.66,.34),(.5,.5),(.34,.66),(.66,.66)]:add(circle(x,y,p,.045,True))
    png('T_Icon_'+name,rgba(m))

# Native-resolution 9-slice component families, all six interaction states.
x,y=grid(96)
states={'Normal':(.17,.20,.23),'Hover':(.25,.28,.30),'Pressed':(.10,.12,.14),'Selected':(.91,.31,.055),'Disabled':(.11,.12,.13),'Focus':(.94,.58,.15)}
for family in ['Panel','Subpanel','Card','Button','Tab','Scrollbar','Slider','Toggle','Radio','Modal']:
    for state,color in states.items():
        inset=.045 if family!='Modal' else .025
        mask=rect(x,y,(inset,inset),(1-inset,1-inset),fill=True)
        image=rgba(mask,color)
        border=rect(x,y,(inset,inset),(1-inset,1-inset),width=.012)
        image[:,:,:3]+=border[:,:,None]*.08
        if state=='Focus':image[:,:,:3]=np.where(border[:,:,None]>.1,(1,.64,.19),image[:,:,:3])
        png('T_UI_'+family+'_'+state,image)

# Custom geometric lettering, preserved as editable stroke data.
LETTERS={
 'J':[[(.15,0),(.9,0),(.9,.78),(.7,1),(.25,1),(0,.8)]],
 'U':[[(0,0),(0,.8),(.2,1),(.8,1),(1,.8),(1,0)]],
 'M':[[(0,1),(0,0),(.5,.5),(1,0),(1,1)]],
 'P':[[(0,1),(0,0),(.8,0),(1,.2),(1,.43),(.8,.6),(0,.6)]],
 'T':[[(0,0),(1,0)],[(.5,0),(.5,1)]],
 'O':[[(.2,0),(.8,0),(1,.2),(1,.8),(.8,1),(.2,1),(0,.8),(0,.2),(.2,0)]],
 'S':[[(1,.08),(.8,0),(.2,0),(0,.2),(0,.35),(.2,.5),(.8,.5),(1,.65),(1,.8),(.8,1),(.2,1),(0,.9)]],
 'A':[[(0,1),(0,.3),(.5,0),(1,.3),(1,1)],[(0,.6),(1,.6)]],
 'C':[[(1,.12),(.8,0),(.2,0),(0,.2),(0,.8),(.2,1),(.8,1),(1,.88)]],
 'E':[[(1,0),(0,0),(0,1),(1,1)],[(0,.5),(.8,.5)]],
}
x,y=np.meshgrid((np.arange(1024)+.5)/1024,(np.arange(512)+.5)/512)
logo=np.zeros_like(x)
for text,top,left,width,height in [('JUMP',.08,.06,.62,.33),('TO SPACE',.58,.06,.88,.31)]:
    step=width/len(text)
    for i,char in enumerate(text):
        for path in LETTERS.get(char,[]):
            points=[(left+i*step+a*step*.70,top+b*height) for a,b in path]
            logo=np.maximum(logo,stroke(x,y,points,.012))
png('T_Logo_Full',rgba(logo))
png('T_Logo_Mono',rgba(logo,(1,1,1)))
x,y=grid(512);symbol=stroke(x,y,[(.5,.12),(.67,.5),(.79,.74),(.57,.64),(.5,.86),(.43,.64),(.21,.74),(.33,.5),(.5,.12)],.035)
symbol=np.maximum(symbol,circle(x,y,(.5,.42),.07))
png('T_Logo_Compact',rgba(symbol,(.96,.43,.10)))

# Five original technical line illustrations composed from our own drawings.
for name in ['EmptySave','NoCrews','NetworkError','ExpeditionCard','LoadingShip']:
    x,y=grid(512);m=np.zeros_like(x)
    if name in ['EmptySave','ExpeditionCard']:
        m=rect(x,y,(.2,.2),(.8,.8),.008)
        m=np.maximum(m,stroke(x,y,[(.25,.68),(.42,.46),(.52,.58),(.64,.37),(.76,.68)],.009))
        m=np.maximum(m,circle(x,y,(.34,.33),.04))
    elif name=='NoCrews':
        for c in [.3,.5,.7]:
            m=np.maximum(m,circle(x,y,(c,.41),.085));m=np.maximum(m,arc(x,y,(c,.72),.13,.16,math.pi,2*math.pi,.018))
    elif name=='NetworkError':
        m=stroke(x,y,[(.5,.17),(.85,.78),(.15,.78),(.5,.17)],.016)
        m=np.maximum(m,segment(x,y,(.5,.39),(.5,.56),.025));m=np.maximum(m,circle(x,y,(.5,.67),.025,True))
    else:
        m=stroke(x,y,[(.5,.14),(.62,.38),(.82,.72),(.60,.62),(.5,.83),(.4,.62),(.18,.72),(.38,.38),(.5,.14)],.014)
        for a in [.3,.7]:m=np.maximum(m,segment(x,y,(a,.78),(a,.9),.008))
    png('T_Illus_'+name,rgba(m,(.91,.48,.17)))

# Adapt the CC0 photographic ground into a restrained, cool, low-frequency surface.
def read(name):
    image=bpy.data.images.load(str(OUT/'External'/name),check_existing=False)
    w,h=image.size;arr=np.empty(w*h*4,dtype=np.float32);image.pixels.foreach_get(arr)
    arr=np.flipud(arr.reshape(h,w,4));bpy.data.images.remove(image);return arr
diff=read('hangar_diffuse.jpg');luma=diff[:,:,:3].mean(axis=2,keepdims=True)
diff[:,:,:3]=np.clip(.24+(luma-.4)*.20, .12,.40)*np.array([.90,.98,1.06])
png('T_FE_Concrete_BaseColor',diff,'Textures','polyhaven_cc0_adapted')
normal=read('hangar_normal_dx.jpg');normal[:,:,:2]=.5+(normal[:,:,:2]-.5)*.22;normal[:,:,2]=np.sqrt(np.maximum(0,1-np.sum((normal[:,:,:2]*2-1)**2,axis=2)))*.5+.5
png('T_FE_Concrete_Normal',normal,'Textures','polyhaven_cc0_adapted')
orm=read('hangar_arm.jpg');orm[:,:,0]=.88+orm[:,:,0]*.12;orm[:,:,1]=.76+orm[:,:,1]*.18;orm[:,:,2]=0
png('T_FE_Concrete_ORM',orm,'Textures','polyhaven_cc0_adapted')
rng=np.random.default_rng(961026)
x,y=grid(2048)
for style in range(5):
    seams=(np.sin(x*math.tau*(4+style))**24+np.sin(y*math.tau*(6+style))**32)*.012
    noise=rng.normal(0,.0007,x.shape)
    height=seams+noise;dy,dx=np.gradient(height)
    n=np.dstack([.5-dx*25,.5+dy*25,np.ones_like(x)])
    png('T_FE_Suit'+str(style)+'_Normal',n,'Textures')
    rough=np.dstack([np.ones_like(x),np.clip(.66+seams*2+noise*8,.5,.8),np.zeros_like(x)])
    png('T_FE_Suit'+str(style)+'_ORM',rough,'Textures')
for name in ['Metal','Rubber','Paint']:
    grain=rng.random(x.shape)*.025
    png('T_FE_'+name+'_ORM',np.dstack([np.ones_like(x),(.42 if name=='Metal' else .78 if name=='Rubber' else .56)+grain,np.full_like(x,.75 if name=='Metal' else .02)]),'Textures')
# Original crater map; wrap X, cosine-latitude scaling for equirectangular projection.
height=rng.normal(0,.004,x.shape)
for i in range(100):
    cx,cy=rng.random(2);radius=rng.uniform(.005,.065)
    d=np.sqrt((np.minimum(abs(x-cx),1-abs(x-cx))*np.maximum(.2,np.sin(cy*math.pi)))**2+(y-cy)**2)/radius
    height+=np.exp(-((d-1)/.13)**2)*.05-np.exp(-(d/.7)**4)*.035
png('T_FE_Moon_BaseColor',np.dstack([np.clip(.45+height,.2,.8)]*3),'Textures')
dy,dx=np.gradient(height);png('T_FE_Moon_Normal',np.dstack([.5-dx*6,.5+dy*6,np.ones_like(x)]),'Textures')
x,y=grid(512)
noise=rng.random(x.shape)
for name,p in [('SoftCircle',np.clip(1-np.sqrt((x-.5)**2+(y-.5)**2)*2,0,1)**2),('Noise',noise),('Stars',(noise>.998).astype(float))]:png('T_FX_'+name,rgba(p),'Textures')
png('T_UI_Grain',rgba(noise*.035),'UI')
stripe=((x+y)*8%1<.5).astype(float)
png('T_UI_WarningStripe',rgba(np.ones_like(x),(.96,.38,.07))*np.dstack([.12+.88*stripe]*3+[np.ones_like(x)]),'UI')
mark=np.maximum(stroke(x,y,[(.1,.1),(.1,.9),(.9,.9)],.018),stroke(x,y,[(.3,.5),(.7,.5),(.55,.35)],.022))
png('T_FE_MarkingAtlas',rgba(mark,(.97,.60,.22)),'Textures')

# Original PCM synthesis. All time-domain sources are saved; deterministic seed above.
RATE=48000
def wav(name, signal, loop=False, recipe=''):
    if 'Audio/'+name+'.wav' not in PRODUCTION_FILES:return
    signal=np.asarray(signal,dtype=np.float64)
    if loop:
        # Smoothly bridge the endpoints with a half-second zero-slope correction.
        # Preserve duration and beat positions; the actual PCM endpoints match.
        count=RATE//2
        blend=(.5-.5*np.cos(np.linspace(0,math.pi,count)))
        if signal.ndim==2:blend=blend[:,None]
        target=(signal[0]+signal[-1])*.5
        signal[:count]+=(target-signal[0])*(1-blend)
        signal[-count:]+=(target-signal[-1])*blend
    else:
        fade=np.linspace(0,1,min(RATE//200,len(signal)//2))
        if signal.ndim==2:fade=fade[:,None]
        signal[:len(fade)]*=fade
        signal[-len(fade):]*=fade[::-1]
    peak=max(float(np.abs(signal).max()),1e-7)
    signal=signal*(.72/max(1,peak))
    path=OUT/'Audio'/(name+'.wav')
    with wave.open(str(path),'wb') as f:
        f.setnchannels(1 if signal.ndim==1 else signal.shape[1]);f.setsampwidth(2);f.setframerate(RATE)
        f.writeframes((np.clip(signal,-1,1)*32767).astype('<i2').tobytes())
    ENTRIES.append({'id':name,'source':'original_blender_pcm_synthesis','file':'Audio/'+path.name,'seconds':len(signal)/RATE,'sample_rate':RATE,'loop':loop,'recipe':recipe})
def tone(frequency,duration,attack=.005):
    t=np.arange(int(RATE*duration))/RATE
    return (np.sin(math.tau*frequency*t)+.16*np.sin(math.tau*frequency*2*t))*np.minimum(1,t/attack)*np.exp(-t/(duration*.28))
SOUNDS=['Focus','Confirm','Back','Tab','Apply','Random','Ready','Unready','Join','Leave','Error','Transition']
for i,name in enumerate(SOUNDS):
    duration=.12 if i<4 else .32
    a=tone([620,740,370,550,880,660,784,440,587,392,185,330][i],duration)
    if i>=4:
        t=np.arange(len(a))/RATE;a+=tone([930,830,1046,330,880,294,174,660][i-4],duration)*np.minimum(1,t/.05)*.35
    wav('S_UI_'+name,a,recipe='Sine + second harmonic; exponential decay; '+name)
for name,seconds in [('Wind',12),('Hangar',10),('ShipCooling',8),('Steam',1.4),('Motor',2.2)]:
    t=np.arange(int(RATE*seconds))/RATE
    n=rng.normal(0,1,len(t));smooth=np.convolve(n,np.ones(32)/32,mode='same')
    phase=math.tau*t/seconds
    if name=='Wind':s=.09*smooth*(.7+.3*np.cos(phase*3))
    elif name=='Hangar':s=.026*np.sin(math.tau*60*t)+.012*np.sin(math.tau*120*t)+smooth*.025
    elif name=='ShipCooling':s=.03*np.sin(math.tau*96*t)*(.8+.2*np.cos(phase*4))+smooth*.04
    elif name=='Steam':s=smooth*.34*np.sin(math.pi*t/seconds)**2
    else:s=.10*np.sin(math.tau*(80*t+45*t*t/seconds))*np.sin(math.pi*t/seconds)**2
    if name in ['Wind','Hangar','ShipCooling']:
        # Remove endpoint mismatch in a short crossfade without a silent seam.
        count=RATE//5;fade=np.linspace(0,1,count);blend=s[:count]*fade+s[-count:]*(1-fade);s[:count]=blend;s[-count:]=blend
    wav('S_Amb_'+name,s,name in ['Wind','Hangar','ShipCooling'],'Filtered noise and harmonic oscillator; original '+name)
duration=80;music=np.zeros((duration*RATE,2),dtype=np.float64)
beat=60/96;bar=beat*4
chords=[(48,55,60,64),(45,52,57,60),(41,48,53,57),(43,50,55,59)]
for b in range(32):
    chord=chords[(b//2)%4];start=int(b*bar*RATE);length=min(int(bar*RATE),len(music)-start)
    t=np.arange(length)/RATE;env=np.sin(math.pi*np.minimum(1,t/bar))**.7
    pad=sum(np.sin(math.tau*440*2**((note-69)/12)*t+.15*j) for j,note in enumerate(chord))*.023*env
    music[start:start+length,0]+=pad;music[start:start+length,1]+=pad*.92
    for step in range(8):
        at=int((b*bar+step*beat*.5)*RATE)
        note=chord[(step+b)%4]+12
        pluck=tone(440*2**((note-69)/12),.34)*.07
        length=min(len(pluck),len(music)-at)
        music[at:at+length,step%2]+=pluck[:length]
    bass=tone(440*2**((chord[0]-12-69)/12),bar)*.07
    length=min(len(bass),len(music)-start);music[start:start+length,:]+=bass[:length,None]
    for beat_index in range(4):
        at=int((b*bar+beat_index*beat)*RATE);hit=rng.normal(0,.009,int(RATE*.035))*np.exp(-np.arange(int(RATE*.035))/(RATE*.008))
        length=min(len(hit),len(music)-at);music[at:at+length,:]+=hit[:length,None]
# Soft tail wrap gives a continuous 32-bar loop.
count=RATE//2;fade=np.linspace(0,1,count)[:,None];blend=music[:count]*fade+music[-count:]*(1-fade);music[:count]=blend;music[-count:]=blend
wav('S_Music_LaunchShift',music,True,'Original 96 BPM, 4/4, 32 bars (80 seconds); C/Am/F/G pad, alternating pluck, sine bass and filtered-noise rhythm')
(OUT/'graphics_audio_manifest.json').write_text(json.dumps(ENTRIES,indent=2),encoding='utf-8')
result={'assets':len(ENTRIES),'icons':len(ICON_NAMES),'face_shapes':len(face),'audio':18,'music_seconds':80}
