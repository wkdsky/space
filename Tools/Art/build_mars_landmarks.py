"""Chapter hero props and underground liners; independent of planet geometry."""
import bpy,ast,json,math,random
from pathlib import Path
from mathutils import Vector
script=Path('D:/projects/space/Tools/Art/build_mars_refinement.py').read_text(encoding='utf8')
functions=[n for n in ast.parse(script).body if isinstance(n,ast.FunctionDef)]
exec(compile(ast.Module(body=functions,type_ignores=[]),'<mars-helpers>','exec'))
OUT=Path('D:/projects/space/SourceArt/MarsStation/Refined')
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
manifest=json.loads((OUT/'manifest.json').read_text());MATS={n:bpy.data.materials.get('MP_'+n) for n in manifest['palette']};parts=[]
landmarks=('Vault_Door','Vault_Frame','Freight_Cage','Rock_Arch','Empty_Spherical_Liner','Ice_Drill','Dismantled_Habitat','Ice_Shards','Half_Sprout','Port_Tank_Farm','Core_Pod')
for old in list(bpy.data.objects):
 if any(old.name.startswith('SM_MP_'+name) for name in landmarks):bpy.data.objects.remove(old,do_unlink=True)
random.seed(29)
# Round seed door and a separate reader frame; the door itself disappears only
# when the existing authoritative three-badge latch is committed.
o=cyl((0,0,2),1.95,.32,'Graphite',32);o.rotation_euler.x=math.pi/2
o=cyl((0,-.19,2),1.72,.10,'Ceramic',32);o.rotation_euler.x=math.pi/2
for i in range(16):
 a=i*math.tau/16;x,z=1.82*math.cos(a),2+1.82*math.sin(a)
 box((x,-.25,z),(.12,.08,.12),'Orange',.02,(0,a,0))
beam((-.65,-.30,2),(.65,-.30,2),.06,'Metal');box((0,-.34,2),(.15,.1,.45),'Orange')
box((0,-.27,1.2),(2.7,.015,.11),'Orange',.005);txt('SEAL INTACT / SEED BANK',(0,-.29,1.27),.10,'Graphite')
finish('Vault_Door')
for x in (-2.35,2.35):box((x,0,2),(.5,.6,4.5),'Ceramic',.08)
box((0,0,4.2),(5.2,.6,.5),'Graphite');txt('SEED VAULT / 03 OF 04',(0,-.33,4.22),.20)
for i in range(3):
 x=-.4+i*.4;box((x,-.34,3.85),(.28,.10,.25),'Orange');box((x,-.40,3.85),(.18,.025,.04),'Graphite',0)
finish('Vault_Frame','complex')

box((0,0,-.12),(4.2,4.2,.24),'Graphite');box((0,0,4.2),(4.2,4.2,.18),'Metal')
for x in (-2,2):
 for y in (-2,2):box((x,y,2),(.14,.14,4.2),'Orange')
 for z in (.8,2.2,3.7):beam((x,-2,z),(x,2,z),.045,'Metal')
 for y in (-1.5,-1,-.5,0,.5,1,1.5):beam((x,y,.3),(x,y,4),.025,'Metal')
for y in (-2,2):
 beam((-2,y,3.7),(2,y,3.7),.045,'Metal')
 for x in (-1.75,-1.5,1.5,1.75):beam((x,y,.2),(x,y,3.7),.035,'Metal')
box((0,-2.08,3.9),(2,.1,.35),'Orange');txt('FREIGHT / CAPACITY 04',(0,-2.15,3.94),.12,'Graphite');finish('Freight_Cage','complex')

# One repeated rock arch, open below and on both ends. Long, clean facets
# differ strongly from the white manufactured station walls.
verts=[];faces=[]
for j in range(2):
 for i in range(9):
  a=-math.pi*.08+i*math.pi*1.16/8;r=2.5+(random.random()-.5)*.22
  verts.append((math.cos(a)*r,(-2 if j==0 else 2),math.sin(a)*r+1.9))
for i in range(8):faces.append((i,i+1,i+10,i+9))
mesh('Excavated basalt arch',verts,faces,'Soil');finish('Rock_Arch','none')
# Smooth spherical exception in the ice branch, intentionally almost empty.
verts=[];faces=[]
for row in range(1,12):
 th=row*math.pi/12
 for col in range(32):
  a=col*math.tau/32;verts.append((4*math.sin(th)*math.cos(a),4*math.sin(th)*math.sin(a),4+4*math.cos(th)))
for r in range(10):
 for c in range(32):
  ids=(r*32+c,r*32+(c+1)%32,(r+1)*32+(c+1)%32,(r+1)*32+c);center=sum((Vector(verts[i]) for i in ids),Vector())/4
  if center.y<-2.6 and abs(center.x)<1.8 and center.z<4.5:continue
  faces.append(ids)
mesh('Unnaturally smooth chamber',verts,faces,'Ceramic');finish('Empty_Spherical_Liner','none')

# Drill, dismantled habitation, frost shards, port tanks and motif.
box((0,0,.5),(3.5,2,.8),'Graphite',.12);box((-.6,0,1.4),(2.1,1.8,1.4),'Ceramic',.12)
for y in (-1.1,1.1):
 for x in (-1.2,0,1.2):o=cyl((x,y,.5),.5,.25,'Metal');o.rotation_euler.x=math.pi/2
beam((0,0,1.4),(2,0,3.5),.2,'Orange');beam((2,0,3.5),(3.6,0,1),.18,'Metal')
for i in range(7):
 cyl((3.6,0,.5+i*.11),.25+i*.025,.10,'Metal',8)
finish('Ice_Drill')
for x in (-3.6,-2.4,-1.2,0,1.2,2.4,3.6):
 for i in range(15):
  a=i*math.tau/16;b=(i+1)*math.tau/16
  beam((x,1.8*math.cos(a),2+1.8*math.sin(a)),(x,1.8*math.cos(b),2+1.8*math.sin(b)),.06,'Metal')
for i in range(16):
 a=i*math.tau/16
 if i in (4,5,6):continue
 box((0,1.85*math.cos(a),2+1.85*math.sin(a)),(7.5,.70,.09),'Ceramic',.02,(a,0,0))
for x in (-3.6,3.6):box((x,0,.12),(1,3,.24),'Orange')
finish('Dismantled_Habitat','complex')
for i in range(6):
 x=random.uniform(-.6,.6);y=random.uniform(-.6,.6);h=random.uniform(.7,2)
 bpy.ops.mesh.primitive_cone_add(vertices=5,radius1=.25,radius2=0,depth=h,location=(x,y,h/2));add(bpy.context.object,'Water')
finish('Ice_Shards','none')
for side in (-1,1):
 mesh('Sprout leaf',[(0,0,.02),(side*4,2,.02),(side*2.5,5,.02),(.2*side,2,.02)],[(0,1,2),(0,2,3)],'Ceramic')
box((0,-1,.02),(.5,5,.04),'Ceramic',0);finish('Half_Sprout','none')
for i in range(3):
 x=-2.4+i*2.4;cyl((x,0,3),1,5.8,'Ceramic',16);ring((x,0,1),1.02,.11,'Orange',24);ring((x,0,4.8),1.02,.11,'Orange',24);beam((x,0,5.9),(x,1.4,6.4),.10,'Metal');beam((x,1.4,6.4),(0,1.4,6.4),.10,'Metal')
box((0,-1.6,.8),(3,1.2,1.6),'Graphite');box((0,-2.22,1.2),(2.4,.04,.5),'Orange');txt('PHOBOS / FUEL RESERVE',(0,-2.25,1.3),.16)
finish('Port_Tank_Farm')

uv((0,0,.55),(.55,.55,.55),'Ceramic',16,8)
for z in (.25,.55,.85):ring((0,0,z),.45,.035,'Amber',16)
box((0,-.53,.55),(.45,.06,.20),'Metal');txt('UNIT 001',(0,-.575,.56),.055,'Graphite');beam((0,0,0),(.4,.3,-.2),.045,'Graphite');finish('Core_Pod')
manifest['palette']=manifest.get('palette',{})
# finish() reused the manifest, so all pre-existing assets retain their records.
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Mars_Landmarks.blend'))
print('Landmark modules exported')
