"""Authored silhouettes for Mars travel, breakable seams and local hazards."""
import bpy,ast,json,math,random
from pathlib import Path
from mathutils import Vector
script=Path('D:/projects/space/Tools/Art/build_mars_refinement.py').read_text(encoding='utf8')
exec(compile(ast.Module(body=[n for n in ast.parse(script).body if isinstance(n,ast.FunctionDef)],type_ignores=[]),'<mars-helpers>','exec'))
OUT=Path('D:/projects/space/SourceArt/MarsStation/Refined')
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
manifest=json.loads((OUT/'manifest.json').read_text());MATS={n:bpy.data.materials.get('MP_'+n) for n in manifest['palette']};parts=[]
names=('Asteroid_Shard','Asteroid_Porous','Asteroid_Crescent','Light_Sail_Ray','Fractured_Seam','Dust_Plume','Spore_Cloud','Empty_Suit','Log_Pedestal','Interior_Panel')
for old in list(bpy.data.objects):
 if any(old.name.startswith('SM_MP_'+n) for n in names):bpy.data.objects.remove(old,do_unlink=True)
random.seed(719)
for i,n in enumerate(names[:3]):
 bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3,radius=1)
 o=bpy.context.object
 for v in o.data.vertices:
  q=v.co.normalized();r=1+random.uniform(-.15,.15)
  if i==1:r*=.68 if q.z>.45 or q.y<-.6 else 1
  if i==2:r*=.42 if q.x>.15 and q.z>.1 else 1
  v.co=Vector((q.x*(12 if i==0 else 8)*r,q.y*(5 if i==0 else 9)*r,q.z*(5 if i==0 else 7)*r))
 add(o,'Graphite' if i==0 else 'Soil');o.data.materials.append(MATS['Metal']);o.data.materials.append(MATS['Bone'])
 for p in o.data.polygons:
  if random.random()<.12:p.material_index=random.choice((1,2))
 finish(n)
# Human-surveyed light sail organism: broad veined membranes, reinforced root,
# narrow central body, paired sensory vanes and trailing forked tail.
uv((0,0,0),(2.2,.36,.30),'Graphite',16,8)
for side in (-1,1):
 vs=[(1.7,0,.05),(2.8,side*2.3,.1),(1.4,side*5.3,.0),(-1,side*7.2,-.4),(-2.3,side*4.2,-.65),(-1.6,side*.5,-.05),(0,side*2.8,.28)]
 mesh('Solar membrane',vs,[(0,1,6),(1,2,6),(2,3,6),(3,4,6),(4,5,6),(5,0,6)],'Bone')
 for a,b in ((0,1),(1,2),(2,3),(3,4),(4,5),(0,6),(2,6),(3,6),(4,6),(5,6)):
  beam(vs[a],vs[b],.028,'Metal')
 beam((-1.7,0,0),(-4,side*.7,-.15),.06,'Shell')
 uv((1.6,side*.24,.2),(.35,.09,.09),'Amber',10,6)
finish('Light_Sail_Ray','complex')
# Independent six-metre seam: readable orange mineral veins and an irregular
# broken outline. Its destruction removes this actor, never the planet mesh.
for row in range(4):
 for col in range(5):
  x=-2.4+col*1.2+random.uniform(-.15,.15);z=.55+row*1.1
  box((x,random.uniform(-.16,.16),z),(1.12,random.uniform(.65,1),1.05),'Soil',.15,(0,random.uniform(-.08,.08),random.uniform(-.04,.04)))
for a,b in [((-1.3,-.57,.2),(-.3,-.6,1.7)),((-.3,-.6,1.7),(-.8,-.58,2.7)),((-.8,-.58,2.7),(.5,-.59,4.2)),((-.3,-.6,1.7),(1.4,-.58,2.5))]:beam(a,b,.055,'Orange')
finish('Fractured_Seam')
# Layered translucent ribbons rather than a scaled opaque engine sphere.
for i in range(7):
 z=i*.85;r=.4+i*.15;verts=[]
 for j in range(14):
  a=j*math.tau/14+i*.5
  verts.extend(((r*math.cos(a),r*math.sin(a),z),((r+.26)*math.cos(a+.18),(r+.26)*math.sin(a+.18),z+1.2)))
 mesh('Drifting dust ribbon',verts,[(j*2,(j*2+2)%28,(j*2+3)%28,j*2+1) for j in range(14)],'Bone')
finish('Dust_Plume','none')
for i in range(25):
 uv((random.uniform(-2.5,2.5),random.uniform(-2.5,2.5),random.uniform(.2,3.3)),(.15,.15,.15),'Fungus',8,4)
finish('Spore_Cloud','none')
# Empty pressure suit for the otherwise deliberately clean spherical chamber.
uv((0,0,.3),(.5,.3,.3),'Ceramic');uv((.65,0,.27),(.24,.25,.26),'Ceramic');uv((.82,0,.28),(.035,.21,.16),'Graphite')
for y in (-.18,.18):
 beam((-.3,y,.25),(-.85,y*1.3,.15),.12,'Ceramic');box((-.95,y*1.3,.12),(.3,.18,.2),'Graphite')
 beam((.35,y,.25),(.12,y*2.4,.1),.095,'Ceramic')
box((0,0,.6),(.5,.36,.25),'Orange');finish('Empty_Suit','none')
box((0,0,.04),(.65,.65,.08),'Graphite');beam((0,0,.06),(0,0,1.1),.075,'Metal')
box((0,0,1.10),(.62,.5,.06),'Orange');box((0,.21,1.18),(.62,.05,.14),'Ceramic');finish('Log_Pedestal')
# Four-metre manufactured wall bay. Narrow seams, removable service covers,
# low vents, structural mullions and a coloured utility rail.
box((0,0,3),(3.96,.10,6),'Graphite',.02)
for z,h in ((.45,.7),(2.2,2.6),(4.8,2.3)):
 box((0,-.07,z),(3.6,.09,h),'Metal' if z<1 else 'Ceramic',.05)
for x in (-1.92,1.92):box((x,-.13,3),(.12,.16,6),'Metal',.01)
box((0,-.15,1),(3.7,.10,.11),'Orange',.01)
for i in range(13):box((-1.5+i*.25,-.14,.45),(.13,.025,.28),'Graphite',.01)
for x in (-1.55,1.55):
 for z in (1.25,3.15,4.0,5.6):cyl((x,-.17,z),.025,.025,'Graphite',6).rotation_euler.x=math.pi/2
finish('Interior_Panel','none')
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2))
print('Mars encounters exported')
