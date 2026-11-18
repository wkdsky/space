"""Station access assembly and the engineer's physical clipboard. Preserve the existing scene."""
import ast, bpy, json, math
from pathlib import Path
from mathutils import Vector

OUT=Path('D:/projects/space/SourceArt/MarsStation/Refined')
functions=[n for n in ast.parse(Path('D:/projects/space/Tools/Art/build_mars_refinement.py').read_text(encoding='utf8')).body if isinstance(n,ast.FunctionDef)]
exec(compile(ast.Module(body=functions,type_ignores=[]),'<mars-prop-helpers>','exec'))
manifest=json.loads((OUT/'manifest.json').read_text(encoding='utf8'))
MATS={};parts=[]
for n,color in manifest['palette'].items():
 m=bpy.data.materials.get('MP_'+n) or bpy.data.materials.new('MP_'+n)
 m.use_nodes=True
 bs=next(node for node in m.node_tree.nodes if node.type=='BSDF_PRINCIPLED')
 bs.inputs['Base Color'].default_value=color;bs.inputs['Roughness'].default_value=.82
 MATS[n]=m
names=('Access_Frame','Access_Leaf','Access_Reader','Access_Status','Incident_Clipboard')
for old in list(bpy.data.objects):
 if any(old.name=='SM_MP_'+n for n in names):bpy.data.objects.remove(old,do_unlink=True)

# Local X is the passage direction; the two leaf pockets fill the lift-house facade.
for side in (-1,1):
 y=side*2.2
 box((0,y,2.12),(.68,1.6,4.24),'Ceramic',.065)
 box((.37,y,2.12),(.10,1.38,3.85),'Graphite',.025)
 for z in (.45,3.55):box((.44,y,z),(.04,1.3,.11),'Orange',.01)
 for dy in (-.50,.50):
  for z in (.25,4):
   o=cyl((.44,y+dy,z),.035,.04,'Metal',6);o.rotation_euler.y=math.pi/2
 box((-.39,y,2.12),(.08,1.3,3.8),'Graphite',.025)
box((0,0,4.25),(.8,6.35,.75),'Graphite',.06)
box((.44,0,4.25),(.10,6.1,.48),'Orange',.035)
txt('RED PLAINS  /  CONTROLLED ACCESS',(.505,0,4.31),.14,'Graphite',(math.pi/2,0,math.pi/2))
for y in (-1.38,1.38):box((.42,y,1.85),(.08,.065,3.62),'Metal',.008)
box((0,0,.035),(.85,2.85,.07),'Metal',.015)
for i in range(8):box((.46,-1.19+i*.34,.08),(.20,.15,.025),'Orange',.002)
finish('Access_Frame','complex')

box((0,0,1.9),(.21,1.43,3.72),'Graphite',.028)
for x in (-.13,.13):
 box((x,0,1.9),(.07,1.28,3.46),'Ceramic',.025)
 box((x*1.37,0,2.25),(.04,1.12,.42),'Metal',.012)
 box((x*1.5,0,.62),(.035,1.09,.12),'Orange',.008)
 for y in (-.53,.53):box((x*1.5,y,1.25),(.035,.045,.45),'Graphite',.004)
 for y in (-.48,0,.48):box((x*1.5,y,2.25),(.035,.025,.32),'Graphite',0)
 for z in (.35,3.44):box((x*1.5,0,z),(.035,.62,.025),'Metal',0)
finish('Access_Leaf')

box((0,0,1.5),(.16,.36,.57),'Graphite',.035)
box((.095,0,1.52),(.05,.30,.46),'Metal',.02)
box((.13,0,1.60),(.025,.24,.19),'Cell',.008)
for i in range(3):box((.146,-.065+i*.065,1.60),(.015,.035,.012),'Ceramic',0)
box((.13,0,1.39),(.025,.24,.095),'Graphite',.008)
for z in (1.37,1.40,1.43):box((.148,-.055,z),(.012,.12,.008),'Ceramic',0)
txt('PERMIT',(.151,0,1.77),.037,'Ceramic',(math.pi/2,0,math.pi/2))
finish('Access_Reader')
box((0,0,0),(.035,.22,.045),'Amber',.009);finish('Access_Status','none')

# A worn backed folder, layered paper, corner pockets and a real spring clip.
box((0,0,.014),(.34,.46,.028),'Graphite',.015)
box((0,0,.031),(.315,.432,.012),'Orange',.009)
for i in range(4):box((.001*i,-.001*i,.041+i*.0015),(.291,.389,.0016),'Ceramic',.001)
for y in (-.193,.193):box((0,y,.049),(.30,.009,.002),'Metal',.001)
box((0,.190,.057),(.13,.044,.018),'Metal',.005)
box((0,.208,.068),(.09,.022,.012),'Graphite',.004)
beam((-.05,.17,.072),(-.05,.211,.078),.003,'Metal')
beam((.05,.17,.072),(.05,.211,.078),.003,'Metal')
beam((-.05,.17,.072),(.05,.17,.072),.003,'Metal')
txt('RED PLAINS STATION',(0,.138,.051),.014,'Graphite',(0,0,0))
txt('ENGINEERING / HANDOVER  17',(0,.111,.051),.010,'Graphite',(0,0,0))
box((0,.087,.051),(.247,.002,.001),'Graphite',0)
for i in range(10):
 y=.060-i*.018
 box((-.028+(i%3)*.008,y,.051),(.19-(i%3)*.016,.0016,.001),'Metal',0)
for y in (.055,-.017,-.089):
 box((-.119,y,.051),(.009,.009,.001),'Graphite',0)
 box((-.119,y,.052),(.006,.006,.001),'Ceramic',0)
box((.074,-.125,.052),(.10,.042,.001),'Orange',.002)
txt('HOLD',(.074,-.135,.054),.019,'Graphite',(0,0,0))
txt('L. CHEN / A. ZHOU',(-.037,-.166,.052),.009,'Graphite',(0,0,0))
box((.167,-.06,.016),(.019,.085,.012),'Orange',.003)
finish('Incident_Clipboard')

(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
exec(compile(Path('D:/projects/space/Tools/Art/finalize_mars_palette.py').read_text(encoding='utf8'),'<finalize-palette>','exec'))
print('Access props ready: '+', '.join(names))
