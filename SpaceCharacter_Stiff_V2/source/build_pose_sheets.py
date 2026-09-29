"""Reproducible V2 design mannequin, not an Unreal bone animation export.
Canonical axes in cm: X forward, Y anatomical right, Z up.
All views, tables and GIFs use the same fixed-length limb coordinates.
"""
from pathlib import Path
import json, csv, math, io
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon, Circle, FancyBboxPatch
from PIL import Image
from reportlab.pdfgen import canvas
from reportlab.lib.utils import ImageReader

ROOT = Path(__file__).resolve().parents[1]
BLUE='#149BC1'; ORANGE='#E88632'; INK='#17304C'; GRAY='#AEB8C1'; BG='#F1F5F9'
COLORS={'L':BLUE,'R':ORANGE}
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':11,'axes.unicode_minus':False})

def unit(v):
    v=np.array(v,dtype=float)
    return v/np.linalg.norm(v)

def ik(a,b,l1,l2,pole):
    a=np.array(a,float); b=np.array(b,float)
    d=np.linalg.norm(b-a)
    assert abs(l1-l2)+1e-5<d<l1+l2-1e-5, (a,b,d)
    u=(b-a)/d
    q=np.array(pole,float); q=unit(q-np.dot(q,u)*u)
    x=(d*d+l1*l1-l2*l2)/(2*d)
    return a+u*x+q*np.sqrt(max(0,l1*l1-x*x))

def angle(a,b,c):
    u=unit(np.array(a)-b);v=unit(np.array(c)-b)
    return 180-math.degrees(math.acos(np.clip(np.dot(u,v),-1,1)))

def fk_arm(shoulder,side,pitch,abduction,bend):
    a=np.deg2rad(pitch); g=np.deg2rad(abduction)
    u=np.array([np.sin(a)*np.cos(g),side*np.sin(g),-np.cos(a)*np.cos(g)])
    v=unit(np.array([1.,0,0])-u*u[0])
    d=u*np.cos(np.deg2rad(bend))+v*np.sin(np.deg2rad(bend))
    e=shoulder+30*u; w=e+27*d
    return e,w

def punch_q(t):
    # Brief pose accents, continuous trajectories; no transform snapping.
    t=t%1
    if t<.04 or t>=.96:return 1.
    if .46<=t<.54:return 0.
    if t<.46:
        u=(t-.04)/.42
        return 1-(.7*u+.3*(3*u*u-2*u*u*u))
    u=(t-.54)/.42
    return .7*u+.3*(3*u*u-2*u*u*u)

def pose(mode,t):
    t=t%1
    hz={'idle':92.,'walk':92.,'run':84.,'punch':92.}[mode]
    j={'pelvis':np.array([0.,0,hz]),'chest':np.array([0.,0,hz+42]),
       'head':np.array([0.,0,hz+77])}
    contacts={}
    for side,s in [('L',-1),('R',1)]:
        h=np.array([0.,s*12.,hz]);j['hip_'+side]=h
        p=(t+(0 if side=='L' else .5))%1
        if mode in ['idle','punch']:
            f=np.array([0.,s*17.,6.]); contacts[side]=True
            pole=[.7,s*.4,0]
        elif mode=='walk':
            if p<.5:
                f=np.array([14-56*p,s*18.,6.]);contacts[side]=True
            else:
                u=(p-.5)*2
                f=np.array([-14+28*u,s*(18+2*np.sin(np.pi*u)),6+5.5*np.sin(np.pi*u)])
                contacts[side]=False
            pole=[1.,s*.42,0]
        else:
            if p<.5:
                f=np.array([18-72*p,s*23.,6.]);contacts[side]=True
            else:
                u=(p-.5)*2
                f=np.array([-18+36*u,s*(23-5*np.sin(np.pi*u)),6+24*np.sin(np.pi*u)])
                contacts[side]=False
            pole=[.45,s*1.,0]
        k=ik(h,f,44,44,pole)
        j['knee_'+side]=k;j['ankle_'+side]=f
        sh=np.array([0.,s*23.,hz+54]);j['shoulder_'+side]=sh
        if mode=='punch':
            q=punch_q(t) if side=='L' else 1-punch_q(t)
            w=np.array([33+23*q,s*(28-10*q),hz+54])
            e=ik(sh,w,30,27,[0,s,0])
        else:
            swing=(-1 if side=='L' else 1)*np.cos(2*np.pi*t)
            if mode=='idle': pitch,abd,bend=0,12,8
            elif mode=='walk':pitch,abd,bend=22*swing,14,8
            else:pitch,abd,bend=20*swing,26,95
            e,w=fk_arm(sh,s,pitch,abd,bend)
        j['elbow_'+side]=e;j['wrist_'+side]=w
        j['fist_'+side]=w+unit(w-e)*6
    for s in ['L','R']:
        for a,b,l in [('hip','knee',44),('knee','ankle',44),('shoulder','elbow',30),('elbow','wrist',27)]:
            assert abs(np.linalg.norm(j[a+'_'+s]-j[b+'_'+s])-l)<1e-7
    return j,contacts

def project(p,view):
    x,y,z=p
    if view=='front':return np.array([-y,z])
    if view=='side':return np.array([x,z])
    return np.array([-y,x])

def hull(points):
    pts=sorted(set(tuple(p) for p in points))
    def cross(o,a,b):return (a[0]-o[0])*(b[1]-o[1])-(a[1]-o[1])*(b[0]-o[0])
    lo=[];hi=[]
    for p in pts:
        while len(lo)>1 and cross(lo[-2],lo[-1],p)<=0:lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(hi)>1 and cross(hi[-2],hi[-1],p)<=0:hi.pop()
        hi.append(p)
    return lo[:-1]+hi[:-1]

def draw(ax,j,c,view='front',upper=False,labels=False):
    hz=j['pelvis'][2]
    ax.set_aspect('equal');ax.axis('off')
    if view=='top':
        ax.set_xlim(-72,72);ax.set_ylim(-27,80)
        ax.add_patch(Polygon([[-20,-16],[20,-16],[20,5],[-20,5]],fc='#E0E6EC',ec=GRAY,lw=1.2))
        ax.add_patch(Circle(project(j['head'],'top'),10,fc='#D9DFE6',ec=INK,lw=1.4))
        ax.annotate('FORWARD +X',xy=(0,76),xytext=(0,67),ha='center',fontsize=9,color=INK,
                    arrowprops={'arrowstyle':'->','color':INK})
        ax.plot([-48,48],[64,64],ls=':',color='#C7D4DF',lw=1)
    else:
        ax.set_xlim((-71,71) if view=='front' else (-66,72))
        ax.set_ylim((75,187) if upper else (-8,188))
        if not upper:ax.plot([-65,65],[0,0],color='#BAC8D4',lw=1.2)
        # Torso is a neutral proxy, not the user's mesh or bind pose.
        pts=[np.array([0,-20,hz+53]),np.array([0,20,hz+53]),np.array([0,14,hz+4]),np.array([0,-14,hz+4])]
        if view=='side': pts=[[-9,0,hz+53],[10,0,hz+53],[8,0,hz+4],[-9,0,hz+4]]
        ax.add_patch(Polygon([project(p,view) for p in pts],fc='#E0E6EC',ec=GRAY,lw=1.3,zorder=1))
        hc=project(j['head'],view)
        ax.add_patch(Circle(hc,10,fc='#D9DFE6',ec=INK,lw=1.5,zorder=4))
        if view=='side':
            ax.plot([hc[0]+7,hc[0]+12],[hc[1],hc[1]-1],color=INK,lw=1.5,zorder=5)
        else:
            ax.plot([hc[0]-4,hc[0]+4],[hc[1]+1,hc[1]+1],ls='',marker='.',color=INK,ms=3,zorder=5)
    order=['L','R'] if view!='side' else ['L','R']
    for s in order:
        color=COLORS[s]
        chains=[('shoulder','elbow','wrist')]
        if view!='top' and not upper: chains.insert(0,('hip','knee','ankle'))
        for chain in chains:
            p=np.array([project(j[n+'_'+s],view) for n in chain])
            ax.plot(p[:,0],p[:,1],color='white',lw=10,solid_capstyle='round',zorder=2)
            ax.plot(p[:,0],p[:,1],color=color,lw=7,solid_capstyle='round',zorder=3,alpha=.92)
            ax.scatter(p[:,0],p[:,1],s=20,c='white',edgecolors=color,linewidths=1.3,zorder=4)
        wrist=project(j['wrist_'+s],view);fist=project(j['fist_'+s],view)
        ax.plot([wrist[0],fist[0]],[wrist[1],fist[1]],color=color,lw=5,zorder=4)
        ax.add_patch(Circle(fist,3.8,fc=color,ec=INK,lw=.8,zorder=5))
        if view!='top' and not upper:
            f=j['ankle_'+s];sig=-1 if s=='L' else 1
            yaw=np.deg2rad(sig*10);fw=np.array([np.cos(yaw),np.sin(yaw),0]);rt=np.array([-np.sin(yaw),np.cos(yaw),0])
            corners=[]
            for x in [-6,15]:
                for y in [-4.5,4.5]:
                    for z in [-6,0]:corners.append(project(f+fw*x+rt*y+[0,0,z],view))
            ax.add_patch(Polygon(hull(corners),fc=color,ec=INK,lw=.9,zorder=4))
            fp=project(f,view)
            if c[s]:
                ax.plot([fp[0]-9,fp[0]+9],[-2,-2],color=color,lw=3,zorder=5)
        if labels:
            anchor=project(j['shoulder_'+s],view)
            if view=='front':ax.text(anchor[0]+(-10 if s=='R' else 10),anchor[1]+6,s,ha='center',color=color,fontweight='bold',fontsize=11)
            if view=='top':ax.text(fist[0]+(-7 if s=='R' else 7),fist[1]+3,s,ha='center',color=color,fontweight='bold',fontsize=10)

def basepage(num,title,sub):
    fig=plt.figure(figsize=(18,12),facecolor='white')
    fig.text(.035,.955,f'{num:02d}  {title}',size=25,weight='bold',color=INK)
    fig.text(.036,.922,sub,size=11.5,color='#526478')
    fig.text(.965,.951,'STIFF / V2',ha='right',size=13,color=BLUE,weight='bold')
    fig.text(.04,.028,'Canonical: X forward / Y anatomical right / Z up | Units: cm | 180 cm proxy | NOT a UE bone export',size=10,color='#657589')
    fig.text(.96,.028,f'{num:02d} / 04',ha='right',size=10,color='#657589')
    return fig

def finish(fig,name,notes):
    fig.add_artist(FancyBboxPatch((.035,.065),.93,.088,boxstyle='round,pad=0.007',transform=fig.transFigure,fc=BG,ec='none'))
    for i,line in enumerate(notes):fig.text(.052,.13-i*.023,line,size=11,color=INK)
    path=ROOT/'designs'/name
    fig.savefig(path.with_suffix('.png'),dpi=160,facecolor='white')
    fig.savefig(path.with_suffix('.svg'),facecolor='white')
    plt.close(fig)
    return path.with_suffix('.png')

def locomotion_sheet(mode,num,title):
    fig=basepage(num,title,'BLUE = anatomical L | ORANGE = anatomical R | Same 3D pose in both views | Solid bar = support foot')
    phases=[0,.25,.5,.75]
    front_text=['L contact','R folded','R contact','L folded'] if mode=='run' else ['L contact','R passing','R contact','L passing']
    for row,view in enumerate(['front','side']):
        y=.535 if row==0 else .18
        fig.text(.038,y+.318,'FRONT: facing viewer' if view=='front' else 'RIGHT SIDE: forward ->',size=11,color=INK,weight='bold')
        for col,t in enumerate(phases):
            ax=fig.add_axes([.045+.24*col,y,.21,.29])
            j,c=pose(mode,t);draw(ax,j,c,view,labels=(col==0))
            ax.set_title(f'{mode[0].upper()}{col}  |  {int(t*100):02d}%  |  {front_text[col]}',fontsize=11,pad=9,color=INK)
    notes=(['Rigid swing: elbow 8 deg, wrist fixed. Legs stay in separate lanes; no crossover or limp wrist.',
            'Walk keys: 0 / 25 / 50 / 75%. Foot lift peaks at 5.5 cm above contact; mirror half a cycle.',
            'Preview cycle 0.90 s only. In-game phase and stride must match actual movement speed.'] if mode=='walk' else
           ['User Fig. 6 direction: one grounded leg, opposite knee OUT and shin folded IN; no crossed feet.',
            'Run uses a low, steady pelvis. Elbows stay at 95 deg; arms swing as rigid angled units.',
            'Preview cycle 0.56 s only. This is a grounded cartoon run; existing jump remains unchanged.'])
    return finish(fig,f'{num:02d}_{mode}_technical',notes)

def upper_sheet():
    fig=basepage(3,'UNARMED UPPER BODY','Three exact pose samples | Same joint coordinates in front and side | No wrist lag / no shoulder sway')
    states=[('idle',0,'U0  IDLE'),('walk',0,'U1  WALK / L contact'),('run',.25,'U2  RUN / R folded')]
    for row,view in enumerate(['front','side']):
        y=.555 if row==0 else .19
        fig.text(.04,y+.3,'FRONT' if view=='front' else 'RIGHT SIDE: forward ->',color=INK,weight='bold',size=11)
        for col,(mode,t,title) in enumerate(states):
            ax=fig.add_axes([.065+.31*col,y,.265,.27])
            j,c=pose(mode,t);draw(ax,j,c,view,upper=True,labels=True)
            ax.set_title(title,color=INK,size=13,pad=13)
    return finish(fig,'03_upper_technical',[
        'U0: arms off torso 12 deg; elbow 8 deg. Hands hang separately. Do not hold a permanent boxing guard.',
        'U1: upper-arm swing +/-22 deg; elbow fixed 8 deg. U2: swing +/-20 deg; elbow fixed 95 deg.',
        'These upper-body poses apply ONLY when unarmed. Preserve current equipped poses and grip constraints.'])

def punch_sheet():
    fig=basepage(4,'ALTERNATING PISTON PUNCH','User Fig. 5 interpreted as TOP VIEW | Forward punch, not overhead reach | Fixed torso; separate fist lanes')
    titles=['B0  00% / L EXTEND','B1  25% / CHANGE','B2  50% / R EXTEND','B3  75% / CHANGE']
    for col,t in enumerate([0,.25,.5,.75]):
        j,c=pose('punch',t)
        ax=fig.add_axes([.045+.24*col,.57,.21,.28]);draw(ax,j,c,'top',labels=True)
        ax.set_title(titles[col],size=11,color=INK,pad=8)
        ax2=fig.add_axes([.045+.24*col,.2,.21,.29]);draw(ax2,j,c,'front',upper=True,labels=(col==0))
        ax2.set_title('FRONT / fists at chest height',size=10,color=INK,pad=5)
    return finish(fig,'04_punch_technical',[
        'One arm extends as the other retracts. Elbows remain out; wrists fixed; no torso twist or boxing guard.',
        'B0 and B2 are extension endpoints, not the entire damage window. Sweep the APPROACH path.',
        'Preview loop 0.40 s = 2 strokes. Runtime timing and damage count follow existing legal attack cadence.'])

def dump_data():
    rows=[]; samples={}
    for mode,n in [('idle',1),('walk',36),('run',28),('punch',20)]:
        samples[mode]=[]
        for i in range(n):
            t=i/n;j,c=pose(mode,t)
            samples[mode].append({'phase':t,'contacts':c,'joints':{k:[round(float(v),6) for v in a] for k,a in j.items()}})
        for t in [0,.25,.5,.75]:
            j,c=pose(mode,t)
            row={'mode':mode,'phase':t}
            for s in ['L','R']:
                row['knee_'+s+'_flex_deg']=round(angle(j['hip_'+s],j['knee_'+s],j['ankle_'+s]),2)
                row['elbow_'+s+'_flex_deg']=round(angle(j['shoulder_'+s],j['elbow_'+s],j['wrist_'+s]),2)
                row['foot_'+s+'_lift_cm']=round(j['ankle_'+s][2]-6,2)
                row['support_'+s]=c[s]
            rows.append(row)
    data={'schema':'pose-design-reference-v2','units':'cm','height_cm':180,
          'axes':{'X':'forward','Y':'anatomical right','Z':'up'},
          'warning':'Proxy reference only. Not Unreal bone transforms. Solve on measured real rig.',
          'limb_lengths':{'thigh':44,'shin':44,'upper_arm':30,'forearm':27},'samples':samples}
    (ROOT/'data/pose_samples.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
    with (ROOT/'data/key_pose_metrics.csv').open('w',newline='',encoding='utf-8-sig') as f:
        writer=csv.DictWriter(f,fieldnames=rows[0].keys());writer.writeheader();writer.writerows(rows)
    return rows

def gifs():
    for mode,n,ms in [('walk',36,25),('run',28,20),('punch',20,20)]:
        frames=[]
        for i in range(n):
            t=i/n;j,c=pose(mode,t)
            fig=plt.figure(figsize=(8,5),facecolor='white')
            fig.text(.045,.936,f'V2 / {mode.upper()} / phase {t:.2f}',size=18,weight='bold',color=INK)
            views=['top','front'] if mode=='punch' else ['front','side']
            for col,view in enumerate(views):
                ax=fig.add_axes([.06+col*.47,.11,.40,.73]);draw(ax,j,c,view,upper=(mode=='punch' and view=='front'),labels=True)
                ax.set_title(view.upper(),color=INK,size=11)
            fig.text(.045,.04,'BLUE=L   ORANGE=R | Design proxy only; preserve the existing jump.',size=10,color='#526478')
            buf=io.BytesIO();fig.savefig(buf,format='png',dpi=105);plt.close(fig);buf.seek(0)
            frames.append(Image.open(buf).convert('RGB').copy())
        durations=([20,30]*(n//2)) if mode=='walk' else ms
        frames[0].save(ROOT/'previews'/f'{mode}_loop.gif',save_all=True,append_images=frames[1:],duration=durations,loop=0,optimize=False)

def main():
    for n in ['designs','previews','data']:(ROOT/n).mkdir(exist_ok=True)
    images=[locomotion_sheet('walk',1,'RIGID WALK'),locomotion_sheet('run',2,'SPLAYED FOLD RUN'),upper_sheet(),punch_sheet()]
    dump_data();gifs()
    pdf=canvas.Canvas(str(ROOT/'Pose_Atlas_V2.pdf'),pagesize=(1080,720))
    pdf.setTitle('Stiff Character Pose Atlas V2')
    pdf.setAuthor('Character animation design')
    for p in images:
        pdf.drawImage(ImageReader(str(p)),0,0,width=1080,height=720);pdf.showPage()
    pdf.save()
    print(json.dumps({'pdf_pages':4,'technical_sheets':4,'loop_previews':3,'coordinate_samples':85,'limb_length_validation':'passed'}))

if __name__=='__main__':main()
