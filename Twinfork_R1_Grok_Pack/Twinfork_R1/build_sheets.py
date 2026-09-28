from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon, Rectangle
from matplotlib.colors import to_rgb

OUT=Path(__file__).parent
BG='#f6f4ef'; INK='#26333b'; SAND='#b9af9b'; ORANGE='#bb6741'; DARK='#303b43'; GLASS='#354b5b'; AMBER='#e4a846'; CYAN='#63d7e6'
plt.rcParams.update({'font.family':'DejaVu Sans','text.color':INK,'axes.labelcolor':INK,'font.size':11})
parts=[]
def face(v,c=SAND,tag='hull'):
    parts.append((np.array(v,dtype=float),c,tag))
def box(lo,hi,c,tag='hull'):
    x,y,z=lo; X,Y,Z=hi
    v=np.array([[x,y,z],[X,y,z],[X,Y,z],[x,Y,z],[x,y,Z],[X,y,Z],[X,Y,Z],[x,Y,Z]])
    for ix in [[0,3,2,1],[4,5,6,7],[0,1,5,4],[1,2,6,5],[2,3,7,6],[3,0,4,7]]:face(v[ix],c,tag)
def ringx(x,y,z,wy,hz):
    return np.array([[x,y+wy*.72,z-hz],[x,y-wy*.72,z-hz],[x,y-wy,z-hz*.65],[x,y-wy,z+hz*.65],[x,y-wy*.72,z+hz],[x,y+wy*.72,z+hz],[x,y+wy,z+hz*.65],[x,y+wy,z-hz*.65]])
def loft(stations,y=0,c=SAND,tag='hull'):
    rings=[ringx(x,y,(z0+z1)/2,w,(z1-z0)/2) for x,w,z0,z1 in stations]
    face(rings[0][::-1],c,tag);face(rings[-1],c,tag)
    for a,b in zip(rings[:-1],rings[1:]):
        for i in range(8):face([a[i],a[(i+1)%8],b[(i+1)%8],b[i]],c,tag)
def octport(x,y,z,w,h,depth,glow,tag='port'):
    a=ringx(x-depth,y,z,w,h);b=ringx(x,y,z,w,h);inner=ringx(x+np.sign(depth)*.2,y,z,w*.70,h*.70)
    for i in range(8):
        face([a[i],a[(i+1)%8],b[(i+1)%8],b[i]],DARK,tag)
        face([b[i],b[(i+1)%8],inner[(i+1)%8],inner[i]],'#666961',tag)
    face(inner,glow,tag)

# All drawings are projections of this single parametric geometry. Unit: cm.
loft([(-580,430,-55,110),(-390,430,-55,110)],0,SAND)
box((-550,-395,111),(-415,395,118),ORANGE)
for sign in [-1,1]:
    y=sign*345
    loft([(-580,85,-50,95),(-460,105,-65,110),(380,105,-65,110),(650,80,-40,75)],y)
    # broad color patches on top, no arbitrary triangulation
    box((65,y-74,110),(285,y+74,112),ORANGE)
    box((-475,y-65,105),(-350,y+65,111),ORANGE)
    # front empty cartridge cap
    octport(650,y,20,60,40,7,DARK,'front_socket')
    # upper rail base
    box((80,y-50,112),(280,y+50,115),AMBER,'top_socket')
    box((88,y-43,115),(272,y+43,119),DARK,'top_socket')
    for dy in [-25,25]:box((115,y+dy-5,119),(245,y+dy+5,125),'#616765','top_socket')
    # outer panel, flush with boom outer face
    out=sign*450
    box((-285,min(out,out-sign*5),-16),(-155,max(out,out-sign*5),56),AMBER,'side_socket')
    box((-278,min(out+.3*sign,out-sign*3),-10),(-162,max(out+.3*sign,out-sign*3),50),DARK,'side_socket')
    # four landing nozzles; four separately positioned landing feet
    for x in [430,-330]:
        box((x-43,y-40,-69),(x+43,y+40,-65),DARK,'landing')
        box((x-27,y-25,-71),(x+27,y+25,-69),AMBER,'landing')
    for x in [250,-470]:
        yf=sign*380
        box((x-12,yf-12,-168),(x+12,yf+12,-57),DARK,'gear')
        box((x-50,yf-43,-190),(x+50,yf+43,-168),'#62655f','gear')

loft([(-390,125,-55,135),(-200,135,-55,230),(-80,135,-55,230),(210,110,-55,85),(270,80,-50,60)])
# front windscreen and roof/side glazing
face([(-77,-95,232),(-77,95,232),(170,79,106),(170,-79,106)],GLASS,'glass')
for s in [-1,1]:
    face([(-180,s*136.2,164),(-80,s*136.2,164),(-80,s*136.2,80),(-180,s*136.2,80)],GLASS,'glass')
    face([(-80,s*136.2,164),(140,s*117.24,79),(140,s*117.24,45),(-80,s*136.2,80)],GLASS,'glass')
# side door and stairs in left inner corridor, clear of nozzles
box((-330,-137,-48),(-215,-135,125),ORANGE,'door')
box((-323,-139,-40),(-222,-137,117),DARK,'door')
for i in range(4):
    box((-321,-151-i*20,-70-i*30),(-225,-131-i*20,-61-i*30),DARK,'stairs')
# central integral cannon and aft drive
loft([(135,47,-116,-30),(265,47,-116,-30),(300,37,-111,-37)],0,DARK,'main_gun')
octport(300,0,-74,35,35,14,'#101a20','main_gun')
octport(-650,0,15,105,92,-70,CYAN,'engine')
BASE=list(parts)

def weapons():
    global parts
    parts=[]
    for s in [-1,1]:
        y=s*345
        # front cartridge: replace cap visually with a shallow emitter
        octport(670,y,20,51,33,20,DARK,'weapon')
        box((670.5,y-9,-3),(671,y+9,43),CYAN,'weapon')
        # top railgun: base -> breech -> barrel, aiming +X
        box((95,y-34,125),(265,y+34,149),DARK,'weapon')
        loft([(90,31,148,207),(235,31,148,207),(280,22,155,195)],y,DARK,'weapon')
        box((235,y-16,161),(555,y+16,184),DARK,'weapon')
        box((290,y-17,169),(537,y-16,175),CYAN,'weapon')
        box((555,y-10,166),(556,y+10,180),CYAN,'weapon')
        # missile side pod, six forward cells (2 columns x 3 rows)
        yc=s*497.5
        box((-330,yc-47.5,-30),(-70,yc+47.5,70),DARK,'weapon')
        for dy in [-21,21]:
            for z in [-10,20,50]:box((-69.5,yc+dy-9,z-9),(-69,yc+dy+9,z+9),AMBER,'weapon')
    w=list(parts);parts=list(BASE);return w
WEAP=weapons()

def ortho_basis(view):
    return {'top':(np.array([1,0,0]),np.array([0,-1,0]),np.array([0,0,1])),
      'bottom':(np.array([1,0,0]),np.array([0,1,0]),np.array([0,0,-1])),
      'front':(np.array([0,-1,0]),np.array([0,0,1]),np.array([1,0,0])),
      'side':(np.array([1,0,0]),np.array([0,0,1]),np.array([0,1,0])),
      'rear':(np.array([0,1,0]),np.array([0,0,1]),np.array([-1,0,0]))}[view]
def perspective_basis(az=35,el=25):
    a,e=np.radians([az,el]);d=np.array([np.cos(e)*np.cos(a),np.cos(e)*np.sin(a),np.sin(e)])
    u=np.array([-np.sin(a),np.cos(a),0]);v=np.cross(d,u);return u,v,d
def draw(ax,geometry,basis,limits=None,edge=.42):
    # Orthographic triangle z-buffer: avoids painter-order errors on long hull faces.
    u,v,d=basis
    allv=np.concatenate([g[0] for g in geometry]);xp=allv@u;yp=allv@v
    if limits is None:
        dx=max(np.ptp(xp),20);dy=max(np.ptp(yp),20)
        limits=((xp.min()-.09*dx,xp.max()+.09*dx),(yp.min()-.09*dy,yp.max()+.09*dy))
    (xmin,xmax),(ymin,ymax)=limits
    W=1600;H=max(250,round(W*(ymax-ymin)/(xmax-xmin)))
    H=min(H,1800)
    rgb=np.empty((H,W,3),dtype=np.uint8);rgb[:]=np.array(to_rgb(BG))*255
    dep=np.full((H,W),-np.inf)
    for verts,c,tag in geometry:
        screen=np.column_stack(((verts@u-xmin)/(xmax-xmin)*(W-1),(verts@v-ymin)/(ymax-ymin)*(H-1),verts@d))
        n=np.cross(verts[1]-verts[0],verts[2]-verts[0]);ln=np.linalg.norm(n)
        shade=.80+.20*abs(n@np.array([.3,-.4,.866])/ln) if ln else 1
        color=(np.clip(np.array(to_rgb(c))*shade,0,1)*255).astype(np.uint8)
        for j in range(1,len(screen)-1):
            tri=screen[[0,j,j+1]];a,b,cc=tri
            x0=max(0,int(np.floor(tri[:,0].min())));x1=min(W-1,int(np.ceil(tri[:,0].max())))
            y0=max(0,int(np.floor(tri[:,1].min())));y1=min(H-1,int(np.ceil(tri[:,1].max())))
            if x0>x1 or y0>y1:continue
            den=(b[1]-cc[1])*(a[0]-cc[0])+(cc[0]-b[0])*(a[1]-cc[1])
            if abs(den)<1e-7:continue
            xx,yy=np.meshgrid(np.arange(x0,x1+1),np.arange(y0,y1+1))
            w0=((b[1]-cc[1])*(xx-cc[0])+(cc[0]-b[0])*(yy-cc[1]))/den
            w1=((cc[1]-a[1])*(xx-cc[0])+(a[0]-cc[0])*(yy-cc[1]))/den
            w2=1-w0-w1;zz=w0*a[2]+w1*b[2]+w2*cc[2]
            sub=dep[y0:y1+1,x0:x1+1]
            mask=(w0>=-1e-6)&(w1>=-1e-6)&(w2>=-1e-6)&(zz>=sub-1e-5)
            sub[mask]=zz[mask];rgb[y0:y1+1,x0:x1+1][mask]=color
    ax.imshow(rgb,origin='lower',extent=(xmin,xmax,ymin,ymax),interpolation='bilinear')
    ax.set_aspect('equal');ax.axis('off');ax.set_facecolor(BG)
    ax.set_xlim(limits[0]);ax.set_ylim(limits[1])
def title(fig,num,sub):
    fig.text(.045,.957,'TWINFORK / R1',fontsize=27,weight='bold')
    fig.text(.045,.92,sub,fontsize=12)
    fig.text(.955,.956,num,fontsize=22,ha='right',color=ORANGE,weight='bold')
def label(ax,txt,p,q,color=INK):
    ax.annotate(txt,xy=p,xytext=q,color=color,fontsize=10,weight='bold',va='center',ha='center',arrowprops={'arrowstyle':'-','color':color,'lw':.9},bbox={'facecolor':BG,'edgecolor':'none','pad':1.6})
def dim(ax,p,q,text,offset=0):
    ax.annotate('',p,q,arrowprops={'arrowstyle':'|-|','color':'#617078','lw':.8})
    mid=(np.array(p)+np.array(q))/2
    ax.text(mid[0],mid[1]+offset,text,fontsize=10,ha='center',va='bottom',bbox={'facecolor':BG,'edgecolor':'none','pad':1})
def save(fig,name):
    fig.savefig(OUT/name,dpi=190,facecolor=BG);plt.close(fig)

# Sheet 01: exact projections, four coordinated views, six ports + main gun.
fig=plt.figure(figsize=(16,12),facecolor=BG)
title(fig,'01','ORTHOGRAPHIC GEOMETRY | Default hull, external weapons absent | Dimensions in cm')
ax=fig.add_axes([.05,.48,.44,.39]);draw(ax,BASE,ortho_basis('top'),((-780,820),(-580,570)))
ax.set_title('TOP  /  +X FORWARD  →',loc='left',fontsize=13,weight='bold')
dim(ax,(-650,-520),(650,-520),'1300',12);dim(ax,(-720,-450),(-720,450),'900',0)
for s,side in [(-1,'L'),(1,'R')]:
    label(ax,side+'1',(650,-s*345),(744,-s*370))
    label(ax,side+'2',(180,-s*345),(180,-s*520))
    label(ax,side+'3',(-220,-s*450),(-220,-s*530))
ax.plot([-650,700],[0,0],ls='--',c='#768589',lw=.8)
label(ax,'M0 / BELOW CABIN',(300,0),(510,105))
ax=fig.add_axes([.54,.48,.41,.39]);draw(ax,BASE,ortho_basis('bottom'),((-780,820),(-580,570)))
ax.set_title('BOTTOM  /  4 LANDING JETS',loc='left',fontsize=13,weight='bold')
for s in [-1,1]:
    for i,x in enumerate([430,-330]):label(ax,'V'+str((0 if s<0 else 2)+i+1),(x,s*345),(x,s*530),ORANGE)
label(ax,'M0: +X',(300,0),(510,90));label(ax,'T0: -X',(-650,0),(-680,190))
ax=fig.add_axes([.05,.15,.44,.25]);draw(ax,BASE,ortho_basis('side'),((-780,820),(-270,350)))
ax.set_title('RIGHT SIDE  /  +X FORWARD  →',loc='left',fontsize=13,weight='bold')
dim(ax,(-720,-190),(-720,230),'420*',0)
ax.axhline(-190,c='#9eaaa8',lw=.8)
label(ax,'R3',(-220,30),(-180,290));label(ax,'R2',(180,125),(180,290));label(ax,'R1',(650,20),(740,155))
ax.annotate('',(620,-74),(310,-74),arrowprops={'arrowstyle':'->','color':ORANGE,'lw':1.6})
ax.text(380,-125,'M0 fixed firing axis',fontsize=9,color=ORANGE)
ax=fig.add_axes([.54,.15,.41,.25]);draw(ax,BASE,ortho_basis('front'),((-660,660),(-270,350)))
ax.set_title('FRONT  /  LOOKING TOWARD -X',loc='left',fontsize=13,weight='bold')
label(ax,'L1',(345,20),(380,285));label(ax,'R1',(-345,20),(-380,285));label(ax,'M0',(0,-74),(180,-160))
dim(ax,(-450,-240),(450,-240),'900',0)
fig.text(.045,.068,'Frame: +X forward / +Y ship right / +Z up. Origin: mid-length centerline at Z=0.  Left = -Y.',fontsize=11)
fig.text(.045,.042,'*420 cm nominal landed height (-190 to +230); surface trim +2 cm. This sheet governs dimensions and hardpoint positions.',fontsize=10)
save(fig,'01_Orthographic.png')

# Sheet 02: real shared geometry assembly comparison + numbered coordinates.
fig=plt.figure(figsize=(16,11),facecolor=BG)
title(fig,'02','ASSEMBLY & SOCKET MAP | Amber = mount/landing jet; cyan = powered emitter | Six external slots only')
for rect,geo,t in [([.04,.51,.44,.36],BASE,'DEFAULT / 6 EMPTY PORTS + M0'),([.52,.51,.44,.36],BASE+WEAP,'EXAMPLE / 6 OPTIONAL MODULES + M0')]:
    ax=fig.add_axes(rect);draw(ax,geo,perspective_basis(38,29));ax.set_title(t,fontsize=12,weight='bold',loc='left')
fig.text(.05,.46,'TYPE 1 / FRONT',fontsize=12,weight='bold');fig.text(.37,.46,'TYPE 2 / TOP',fontsize=12,weight='bold');fig.text(.69,.46,'TYPE 3 / OUTER SIDE',fontsize=12,weight='bold')
samples=[('front_socket',(650,345,20),(.05,.29,.27,.14)),('top_socket',(180,345,115),(.37,.29,.27,.14)),('side_socket',(-220,450,20),(.69,.29,.27,.14))]
for tag,center,rect in samples:
    center=np.array(center);g=[(v-center,c,t) for v,c,t in BASE if t==tag and np.mean(v[:,1])>0]
    ax=fig.add_axes(rect);draw(ax,g,perspective_basis(35,28))
fig.text(.05,.25,'120 x 80 face; recessed cap\nInsert: short pulse emitter\nBlanking cap hidden when equipped',fontsize=10,linespacing=1.5)
fig.text(.37,.25,'200 x 100 base; twin short rails\nInsert: 470 cm railgun envelope\nBreech behind muzzle; fires +X',fontsize=10,linespacing=1.5)
fig.text(.69,.25,'130 x 72 panel; keyed side bracket\nInsert: 260 x 95 x 100 missile pod\nSix cells (2 x 3) face +X',fontsize=10,linespacing=1.5)
fig.text(.05,.183,'SOCKET ORIGINS (cm)  /  ROTATION: all weapon local +X = ship +X',fontsize=11,weight='bold')
rows=[['L1 / R1','(650, -345 / +345, 20)','front cartridge'],['L2 / R2','(180, -345 / +345, 115)','top rail'],['L3 / R3','(-220, -450 / +450, 20)','outer side panel'],['M0','(300, 0, -74)','integral main-gun muzzle; +X']]
ax=fig.add_axes([.05,.04,.90,.125]);ax.axis('off')
t=ax.table(cellText=rows,colWidths=[.14,.43,.43],cellLoc='left',bbox=[0,0,1,1]);t.auto_set_font_size(False);t.set_fontsize(10)
for (r,c),cell in t.get_celld().items():cell.set_facecolor('#ede9df' if r%2==0 else BG);cell.set_edgecolor('#d5d9d5');cell.PAD=.10
save(fig,'02_Hardpoints_Assembly.png')

# Sheet 03: isolated main gun, rear engine and landing clearances.
fig=plt.figure(figsize=(16,10),facecolor=BG)
title(fig,'03','PROPULSION & MAIN GUN | Built-in M0 remains present in default configuration')
ax=fig.add_axes([.04,.49,.43,.36]);g=[p for p in BASE if p[2]=='main_gun'];draw(ax,g,perspective_basis(35,18));ax.set_title('M0 / FIXED UNDER-CABIN GUN',loc='left',fontsize=13,weight='bold')
fig.text(.05,.44,'Octagonal dark muzzle, 70 x 70 opening surround.\nShort housing under cabin nose; no turret joint.\nMuzzle at (300, 0, -74); projectile axis = +X.',fontsize=11,linespacing=1.5)
ax=fig.add_axes([.53,.49,.43,.36]);draw(ax,BASE,ortho_basis('rear'),((-570,570),(-270,310)));ax.set_title('REAR / ONE CENTRAL DRIVE',loc='left',fontsize=13,weight='bold')
label(ax,'T0',(0,15),(0,270),ORANGE)
fig.text(.54,.44,'One octagonal engine: outer face 210 x 184.\nExit center (-650, 0, 15); exhaust axis = -X.\nCyan glow is the rear drive, not a weapon socket.',fontsize=11,linespacing=1.5)
ax=fig.add_axes([.04,.08,.43,.28]);draw(ax,BASE,perspective_basis(140,-34));ax.set_title('UNDERSIDE / 4 VERTICAL JETS',loc='left',fontsize=13,weight='bold')
fig.text(.54,.32,'LANDING SYSTEM',fontsize=13,weight='bold')
fig.text(.54,.265,'V1 / V3: (430, -345 / +345, -71)\nV2 / V4: (-330, -345 / +345, -71)\nEach outlet 86 x 80; exhaust -Z, thrust +Z.',fontsize=11,linespacing=1.5)
fig.text(.54,.145,'Ground plane Z=-190; feet are separate from nozzles.\nJet clearance: 119 cm. Main-gun clearance: 74 cm.\nSide door opens into the inner corridor, clear of weapons.\nLanding legs/stairs shown deployed; make separate parts.',fontsize=11,linespacing=1.5)
save(fig,'03_MainGun_Propulsion.png')
print('Created three sheets from one geometry:',len(BASE),'faces')
