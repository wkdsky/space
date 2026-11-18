"""Mars chapter production modules. Run in the installed Blender MCP session.
Metres, +X forward, +Z up, foot origin; named material slots survive FBX import.
"""
import bpy, math, random, json
from pathlib import Path
from mathutils import Vector

OUT=Path('D:/projects/space/SourceArt/MarsStation/Refined')
OUT.mkdir(parents=True,exist_ok=True)
for old in list(bpy.data.objects):bpy.data.objects.remove(old,do_unlink=True)
random.seed(781)
COLORS={'Ceramic':(.77,.80,.74,1),'Orange':(.90,.25,.045,1),'Graphite':(.035,.055,.060,1),
 'Metal':(.28,.35,.34,1),'Cell':(.018,.08,.17,1),'CellLine':(.18,.43,.52,1),
 'Green':(.13,.28,.11,1),'Leaf':(.29,.43,.17,1),'Soil':(.12,.07,.035,1),
 'Water':(.07,.30,.31,1),'Glass':(.48,.72,.68,.16),'Glow':(.24,.83,.59,1),
 'Amber':(1,.35,.055,1),'Red':(.8,.055,.022,1),'Bone':(.52,.44,.25,1),
 'Shell':(.21,.26,.15,1),'Belly':(.55,.27,.13,1),'Fungus':(.56,.53,.35,1)}
MATS={}
for n,c in COLORS.items():
 m=bpy.data.materials.new('MP_'+n);m.diffuse_color=c;m.use_nodes=True
 bs=next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED');bs.inputs['Base Color'].default_value=c;bs.inputs['Roughness'].default_value=.82
 bs.inputs['Metallic'].default_value=.3 if n in ('Metal','Cell','CellLine') else 0
 if n=='Glass':bs.inputs['Alpha'].default_value=.16
 if n in ('Glow','Amber','Red'):bs.inputs['Emission Color'].default_value=c;bs.inputs['Emission Strength'].default_value=2
 MATS[n]=m
parts=[];manifest={}
def add(o,mat):
 o.data.materials.append(MATS[mat]);parts.append(o);return o
def box(loc,size,mat='Ceramic',bevel=.02,rot=None):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.scale=size
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 if rot:o.rotation_euler=rot
 if bevel:
  mod=o.modifiers.new('Machined edges','BEVEL');mod.width=bevel;mod.segments=1
  bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=mod.name)
 return add(o,mat)
def uv(loc,size,mat='Ceramic',segments=12,rings=6):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=segments,ring_count=rings,radius=1,location=loc)
 o=bpy.context.object;o.scale=size;return add(o,mat)
def cyl(loc,r,depth,mat='Metal',vertices=12):
 bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=r,depth=depth,location=loc);return add(bpy.context.object,mat)
def beam(a,b,r=.04,mat='Metal',vertices=8):
 a,b=Vector(a),Vector(b);o=cyl((a+b)/2,r,(b-a).length,mat,vertices);o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler();return o
def ring(loc,r,tube=.05,mat='Metal',n=32):
 for i in range(n):
  a=i*math.tau/n;b=(i+1)*math.tau/n
  beam((loc[0]+r*math.cos(a),loc[1]+r*math.sin(a),loc[2]),(loc[0]+r*math.cos(b),loc[1]+r*math.sin(b),loc[2]),tube,mat)
def txt(s,loc,size=.14,mat='Ceramic',rotation=(math.pi/2,0,0)):
 bpy.ops.object.text_add(location=loc,rotation=rotation);o=bpy.context.object;o.data.body=s;o.data.size=size;o.data.extrude=.001;o.data.align_x='CENTER'
 bpy.ops.object.convert(target='MESH');return add(bpy.context.object,mat)
def mesh(name,vs,fs,mat):
 d=bpy.data.meshes.new(name);d.from_pydata(vs,[],fs);d.update();o=bpy.data.objects.new(name,d);bpy.context.collection.objects.link(o);return add(o,mat)
def finish(name,collision='simple'):
 global parts
 bpy.ops.object.select_all(action='DESELECT')
 for o in parts:o.select_set(True)
 bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();o=bpy.context.object;o.name='SM_MP_'+name
 bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
 bpy.ops.export_scene.fbx(filepath=str(OUT/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False)
 manifest[name]={'mesh':o.name,'triangles':sum(len(p.vertices)-2 for p in o.data.polygons),'collision':collision}
 o.hide_set(True);parts=[];return o
def bolt(loc):cyl(loc,.025,.018,'Metal',6)

# The old projected hex grid pinched into metre-long spikes. This is clipped,
# inset geodesic glazing with an intentional airlock and one eight-metre breach.
bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3,radius=1)
ico=bpy.context.object;vs=[v.co.copy() for v in ico.data.vertices];faces=[list(p.vertices) for p in ico.data.polygons];bpy.data.objects.remove(ico,do_unlink=True)
polys=[]
for f in faces:
 poly=[vs[i] for i in f];clipped=[]
 for a,b in zip(poly,poly[1:]+poly[:1]):
  if a.z>=-1e-6:clipped.append(a)
  if (a.z<0)<(b.z<0):clipped.append(a+(b-a)*(-a.z/(b.z-a.z)))
 if len(clipped)<3:continue
 p=[Vector((v.x*19,v.y*19,max(0,v.z)*16.5)) for v in clipped];c=sum(p,Vector())/len(p)
 if (c.y<-16.8 and abs(c.x)<2.7 and c.z<4.1) or (c.x>16 and abs(c.y)<4.2 and c.z<5.6):continue
 polys.append(p)
verts=[];polygons=[]
for p in polys:
 c=sum(p,Vector())/len(p);normal=Vector((c.x/19,c.y/19,c.z/16.5)).normalized();q=[c+(v-c)*.972 for v in p]
 start=len(verts);verts.extend(q);verts.append(c+normal*.10)
 for i in range(len(q)):polygons.append((start+i,start+(i+1)%len(q),start+len(q)))
mesh('Pillowed ETFE cushions',verts,polygons,'Glass');finish('Dome_Glazing','none')
edges=set();nodes=set()
for p in polys:
 for a,b in zip(p,p[1:]+p[:1]):
  ka=tuple(round(v,4) for v in a);kb=tuple(round(v,4) for v in b);key=tuple(sorted((ka,kb)))
  if key not in edges:beam(a,b,.065,'Ceramic');edges.add(key)
  nodes.add(ka)
for p in nodes:uv(p,(.12,.12,.12),'Metal',8,4)
finish('Dome_Ribs','complex')
# Segmented footing, inspection ring, airlock threshold and realistic rupture.
for i in range(72):
 a=(i+.5)*math.tau/72;x,y=18.95*math.cos(a),18.95*math.sin(a)
 if y<-18 and abs(x)<2.6:continue
 if x>17.8 and abs(y)<4.3:continue
 box((x,y,.18),(.40,1.67,.36),'Graphite',.025,(0,0,a))
 box((x*.986,y*.986,.42),(.16,1.63,.13),'Orange',.015,(0,0,a))
finish('Dome_Footing')

# A complete serviceable solar tracker: cells, bus bars, rails, pivot,
# bracing, cable junctions and bolt plates. Clear space remains underneath.
tilt=math.radians(8)
def panelpos(x,y,z=0):return (x,y*math.cos(tilt)-z*math.sin(tilt),2+y*math.sin(tilt)+z*math.cos(tilt))
box(panelpos(0,0), (5.5,11,.16),'Graphite',.035,(tilt,0,0))
for x in (-2.72,2.72):box(panelpos(x,0,.12),(.10,11,.12),'Metal',.01,(tilt,0,0))
for y in (-5.45,5.45):box(panelpos(0,y,.12),(5.5,.10,.12),'Ceramic',.01,(tilt,0,0))
for col in range(5):
 for row in range(10):
  x=-2.15+col*1.075;y=-4.88+row*1.085
  box(panelpos(x,y,.12),(1.01,1.02,.025),'Cell',.008,(tilt,0,0))
  for dx in (-.30,0,.30):box(panelpos(x+dx,y,.139),(.008,.98,.004),'CellLine',0,(tilt,0,0))
for y in (-3.6,3.6):
 z=panelpos(0,y,-.18)[2]
 for x in (-1.7,1.7):
  box((x,y,.09),(.6,.6,.18),'Graphite');beam((x,y,.18),(x,y,z),.10,'Metal');beam((x,y,.35),(0,y,z),.07,'Metal')
  for dx,dy in ((-.2,-.2),(.2,.2)):bolt((x+dx,y+dy,.19))
 beam((-2.5,y,z),(2.5,y,z),.09,'Ceramic')
beam((0,-3.6,1.65),(0,3.6,2.67),.12,'Orange')
box((0,-3.6,1.2),(.35,.24,.45),'Graphite');beam((0,-3.6,.9),(0,-5.2,.14),.035,'Graphite')
finish('Solar_Tracker')

def terminal(name,kind='power'):
 box((0,0,.11),(1.5,1.1,.22),'Graphite',.06);box((0,.08,1.05),(1.15,.65,1.65),'Ceramic',.07)
 box((0,-.28,1.20),(.98,.06,1.1),'Orange',.045)
 box((0,-.32,1.42),(.67,.03,.26),'Graphite',.015)
 txt('RED PLAINS' if kind=='power' else 'ACCESS / LIFT',(0,-.345,1.46),.083)
 txt('PV / SERVICE' if kind=='power' else 'POWER REQUIRED',(0,-.345,1.34),.07)
 for x in (-.41,.41):
  for z in (.76,1.68):box((x,-.35,z),(.055,.07,.09),'Metal',.01)
 for x in (-.29,0,.29):
  cyl((x,0,.38),.055,.45,'Metal');box((x,-.34,1.06),(.16,.04,.16),'Graphite',.02)
 beam((-.25,-.4,.83),(.25,-.4,.83),.025,'Graphite');box((.25,-.40,.85),(.07,.06,.20),'Metal')
 for i in range(5):box((-.28+i*.14,-.342,.62),(.07,.02,.10),'Graphite',0)
 txt('LOCKOUT / 3 SEC',(0,-.345,.46),.068)
 finish(name)
terminal('Combiner');terminal('Lift_Console','lift')
box((0,0,0),(.63,.035,.15),'Glow',.02);finish('Status_Light','none')

# Walkable deck tiles and unmistakable human-scale doors.
box((0,0,-.10),(4,4,.20),'Graphite',.025)
for x in (-1.96,1.96):box((x,0,.012),(.05,4,.025),'Metal',0)
for y in (-1.96,1.96):box((0,y,.012),(4,.05,.025),'Metal',0)
for x in (-1.82,1.82):
 for y in (-1.82,1.82):bolt((x,y,.03))
for x in (-1.5,-1,-.5,0,.5,1,1.5):box((x,0,.006),(.012,3.4,.012),'Metal',0)
finish('Deck_Tile')
for x in (-2.15,2.15):
 box((x,0,2.25),(.30,.55,4.5),'Ceramic',.05);box((x,-.30,2.25),(.09,.06,3.8),'Orange')
box((0,0,4.45),(4.6,.55,.3),'Ceramic');box((0,-.30,4.45),(2.8,.04,.12),'Glow')
box((0,0,.06),(4.4,.8,.12),'Metal')
txt('RED PLAINS / AIRLOCK',(0,-.32,4.73),.22);finish('Door_Frame')

def plant(x,y,z,scale=1):
 beam((x,y,z),(x,y,z+.46*scale),.018*scale,'Green')
 for i in range(5):
  a=i*2.4;uv((x+math.cos(a)*.17*scale,y+math.sin(a)*.17*scale,z+(.14+i*.055)*scale),(.24*scale,.085*scale,.035*scale),'Leaf',8,4).rotation_euler.z=a
def planter():
 box((0,0,.45),(2.8,1.15,.6),'Ceramic',.08);box((0,0,.77),(2.58,.96,.08),'Soil',.02)
 for x in (-1.15,1.15):
  for y in (-.4,.4):beam((x,y,.05),(x,y,.4),.06)
 for x in (-.85,-.28,.28,.85):
  for y in (-.25,.25):plant(x,y,.81,random.uniform(.7,1.15))
 for y in (-.62,.62):beam((-1.4,y,.72),(1.4,y,.72),.035,'Water')
 box((1.44,0,.57),(.12,.36,.26),'Graphite');finish('Grow_Bed')
planter()
for x in (-1.45,1.45):
 for y in (-.7,.7):beam((x,y,.1),(x,y,2.8),.05,'Metal')
for z in (.85,1.8):
 box((0,0,z),(3,1.4,.16),'Ceramic');box((0,0,z+.1),(2.8,1.2,.06),'Soil')
 for x in (-1,-.5,0,.5,1):
  for y in (-.35,.35):plant(x,y,z+.14,.6)
for z in (1.7,2.7):box((0,0,z),(2.7,.09,.04),'Glow',.01)
txt('B2 / POLLINATION',(0,-.77,2.8),.13);finish('Nursery_Rack')

for x in (-.75,.75):
 cyl((x,0,.18),.52,.36,'Graphite');cyl((x,0,1.2),.48,1.7,'Ceramic');cyl((x,0,1.3),.49,.65,'Water');cyl((x,0,2.12),.53,.15,'Metal')
 beam((x,0,2.2),(x,0,2.65),.07);beam((x,0,2.65),(0,.5,2.65),.07)
box((0,-.55,1.5),(.55,.16,.45),'Graphite');box((0,-.65,1.55),(.37,.02,.2),'Glow',0)
txt('H2O / CLOSED LOOP',(0,-.66,1.75),.09);finish('Water_Recycler')

# Legible themed workstations rather than empty white rooms.
for x in (-1.15,1.15):box((x,0,.48),(.18,.8,.96),'Metal')
box((0,0,1.02),(2.7,1,.13),'Ceramic');box((.65,.1,1.4),(.75,.25,.6),'Graphite');box((.65,-.04,1.43),(.62,.025,.4),'CellLine',0)
for x in (-.9,-.5):cyl((x,-.15,1.22),.11,.27,'Fungus');cyl((x,-.15,1.36),.12,.04,'Orange')
box((-.7,.2,1.2),(.7,.3,.22),'Metal');txt('ECO / LAB 04',(.65,-.06,1.48),.07)
finish('Lab_Bench')
for z in (.25,1,1.75):
 box((0,0,z),(2.4,.8,.10),'Metal')
 for x in (-.8,0,.8):box((x,0,z+.28),(.62,.6,.45),'Ceramic',.035);box((x,-.32,z+.28),(.32,.02,.12),'Orange',0)
for x in (-1.2,1.2):beam((x,-.38,0),(x,-.38,2.4),.04);beam((x,.38,0),(x,.38,2.4),.04)
finish('Seed_Rack')
for x in (-.8,.8):
 cyl((x,0,.15),.58,.3,'Graphite');cyl((x,0,1.30),.55,2,'Ceramic');cyl((x,0,1.3),.56,.65,'Glass')
 ring((x,0,1),.57,.04,'Orange',16);ring((x,0,1.65),.57,.04,'Orange',16)
 for i in range(7):
  a=i*2.4;h=.85+(i%3)*.3;beam((x,0,.6),(x+math.cos(a)*.3,math.sin(a)*.3,h),.035,'Fungus');uv((x+math.cos(a)*.3,math.sin(a)*.3,h),(.18,.18,.08),'Fungus')
 beam((x,0,2.4),(x,.6,2.8),.07,'Water')
box((0,-.6,1.2),(.42,.18,.5),'Graphite');box((0,-.70,1.3),(.22,.025,.16),'Glow');finish('Mycelium_Vat')
box((0,0,.15),(3,2,.3),'Graphite');box((0,.35,.85),(2.8,1.1,1.2),'Ceramic',.10)
for x in (-1,0,1):
 cyl((x,.35,1.55),.28,.22,'Metal');box((x,-.35,.65),(.65,.25,.6),'Orange')
for y in (-.8,.8):beam((-1.5,y,1.35),(1.5,y,1.35),.06)
txt('B4 / REGOLITH PROCESSOR',(0,-.52,1.05),.13);finish('Soil_Processor')

# Exterior equipment with the same coherent family of frames, panels and fasteners.
box((0,0,.60),(3.2,1.8,.55),'Graphite',.15);box((0,0,1.0),(2.4,1.6,.6),'Ceramic',.12)
box((-.7,0,1.65),(1.2,1.4,1.0),'Orange',.12);box((-.7,-.73,1.7),(.88,.04,.55),'Cell')
for x in (-1.25,0,1.25):
 for y in (-1,1):
  o=cyl((x,y,.5),.48,.24,'Graphite',16);o.rotation_euler.x=math.pi/2
  o=cyl((x,y*1.14,.5),.25,.03,'Metal',12);o.rotation_euler.x=math.pi/2
  beam((x,y,.58),(x*.75,0,.80),.07,'Metal')
beam((.8,0,1.3),(1.2,0,2.3),.06);box((1.2,0,2.4),(.4,.55,.28),'Ceramic');box((1.42,-.1,2.4),(.03,.16,.13),'Cell')
finish('Survey_Rover')
for x,y in ((-1,-.65),(1,-.65),(0,1)):
 box((x,y,.06),(.55,.55,.12),'Graphite');beam((x,y,.1),(0,0,3.8),.08)
beam((0,0,0),(0,0,6),.08,'Ceramic');beam((-1.8,0,4.6),(1.8,0,4.6),.06,'Orange')
for x in (-1.5,1.5):uv((x,0,4.9),(.25,.25,.25),'Ceramic');beam((x,0,4.6),(x,0,5.2),.04)
box((0,0,2.3),(.6,.4,.8),'Ceramic');box((0,-.22,2.4),(.3,.03,.25),'Cell');cyl((0,0,6.1),.18,.3,'Amber');finish('Weather_Mast')
for x,y in ((-1,-1),(1,-1),(1,1),(-1,1)):beam((x,y,0),(0,0,5),.07,'Metal')
beam((0,0,4),(0,0,7),.07,'Ceramic')
for z in (5.5,6,6.5):beam((-1.5,0,z),(1.5,0,z),.025,'Ceramic')
box((0,0,1.1),(1,.8,1.5),'Orange');box((0,-.43,1.3),(.7,.04,.4),'Graphite');txt('SOS / 121.5',(0,-.46,1.35),.09);finish('Rescue_Antenna')

# Access buildings: a vented lift enclosure, not a featureless cube.
for x in (-3,3):
 box((x,0,2.5),(.22,6,5),'Ceramic',.04)
 for y in (-2.9,0,2.9):box((x,y,2.5),(.4,.18,5),'Metal')
box((0,3,2.5),(6,.25,5),'Ceramic');box((0,0,5.05),(6.3,6.3,.30),'Graphite')
for x in (-2.7,2.7):box((x,-3,2.5),(.6,.4,5),'Ceramic')
box((0,-3,4.65),(5.4,.4,.7),'Orange');txt('RED PLAINS / LIFT 01',(0,-3.23,4.68),.23)
for i in range(12):box((-2.5+i*.45,3.16,3.5),(.25,.08,.65),'Graphite',.01)
for x in (-3.15,3.15):
 for y in (-2.9,2.9):box((x,y,.16),(.65,.65,.32),'Graphite')
finish('Lift_Housing','complex')

# Reusable ribs, pipework, wall equipment and hazard dressing.
for x in (-1.95,1.95):box((x,0,2),(.12,.25,4),'Metal')
box((0,0,3.94),(4,.25,.12),'Metal');box((0,0,.04),(4,.25,.08),'Metal');finish('Corridor_Rib')
for y in (-.17,.17):beam((-2,y,0),(2,y,0),.055,'Water');
for x in (-1.9,1.9):box((x,0,0),(.12,.55,.16),'Metal')
finish('Pipe_Run','none')
box((0,0,0),(1.2,.3,.15),'Graphite');box((0,-.17,0),(1,.04,.08),'Glow',.01);finish('Wall_Light','none')
box((0,0,.25),(1.8,.65,.5),'Graphite');box((0,.2,.85),(1.8,.2,.8),'Ceramic');box((-.85,0,.65),(.12,.65,.4),'Orange');box((.85,0,.65),(.12,.65,.4),'Orange');finish('Bench')
box((0,0,.10),(.9,.7,.2),'Graphite');box((0,0,.55),(.8,.6,.7),'Orange',.06)
for x in (-.28,.28):box((x,-.32,.55),(.10,.04,.6),'Metal')
box((0,-.34,.65),(.3,.025,.15),'Ceramic');finish('Cargo_Crate')
for i in range(7):
 x=random.uniform(-1.2,1.2);y=random.uniform(-1,1);z=random.uniform(.1,.3)
 box((x,y,z),(random.uniform(.3,.8),.25,.2),'Metal',.03,(.2,i*.4,i))
 box((x,y,z+.1),(.5,.4,.04),'Ceramic',.01,(i*.4,.3,i))
finish('Wreck_Salvage')

# Exactly five camera actors will receive this recognizable articulated housing.
box((0,0,0),(.35,.12,.3),'Metal');beam((0,0,0),(0,-.35,-.15),.04)
box((0,-.55,-.2),(.42,.65,.30),'Ceramic',.05)
o=cyl((0,-.90,-.2),.115,.08,'Graphite');o.rotation_euler.x=math.pi/2
o=cyl((0,-.95,-.2),.07,.02,'CellLine');o.rotation_euler.x=math.pi/2
box((.14,-.91,-.10),(.035,.02,.035),'Red',0);finish('Security_Camera')

# Objects the player actually picks up/read: departments are authored as text.
box((0,0,.02),(.32,.22,.04),'Graphite');box((0,0,.045),(.28,.18,.01),'Ceramic',.01)
box((0,.04,.053),(.18,.06,.005),'Orange',0);box((-.07,-.04,.055),(.055,.055,.008),'Cell',0)
txt('RED PLAINS',(.035,-.03,.057),.018,rotation=(0,0,0));txt('ECO STATION',(.025,-.07,.057),.016,rotation=(0,0,0));finish('Supervisor_Badge')
box((0,0,.025),(.36,.26,.05),'Orange',.02);box((0,0,.056),(.31,.21,.02),'Graphite',.01)
for i in range(6):box((-.11+i*.042,0,.07),(.026,.15,.005),'CellLine',0)
finish('Data_Pad')
ring((0,0,.08),.28,.045,'Orange',24);box((.28,0,.1),(.18,.22,.15),'Graphite');box((.37,0,.1),(.025,.1,.07),'Glow');finish('Tracking_Collar')
box((0,0,.03),(.30,.24,.06),'Graphite');box((0,0,.067),(.20,.14,.015),'Metal',.01)
for x in (-.16,.16):
 for i in range(6):box((x,-.09+i*.035,.03),(.05,.018,.012),'Amber',0)
txt('LOGISTICS',(0,-.025,.08),.027,'Graphite',rotation=(0,0,0));finish('Logistics_Chip')

# Noctis: human heavy engineering, welded robot fabrication, not alien ruins.
for x in (-5.5,5.5):
 for y in (-5.5,5.5):
  box((x,y,.25),(1.8,1.8,.5),'Graphite');box((x,y,9),(.45,.45,18),'Orange')
 for z in (4,9,14):
  beam((x,-5.5,z),(x,5.5,z),.13,'Metal');beam((x,-5.5,z-4),(x,5.5,z),.10,'Metal');beam((x,5.5,z-4),(x,-5.5,z),.10,'Metal')
box((0,0,17.8),(12,12,.5),'Ceramic');box((0,0,18.4),(7,5,1),'Orange')
for y in (-3,3):beam((-5,y,17),(5,y,17),.15,'Metal')
txt('NOCTIS / FREIGHT 07',(0,-6.05,16.4),.65);finish('Noctis_Headframe','complex')
for i in range(6):
 a=i*math.tau/6;x,y=2.4*math.cos(a),2.4*math.sin(a)
 box((x,y,.2),(1.5,1.5,.4),'Graphite',.05,(0,0,a))
 beam((x,y,.3),(x,y,1.1),.10,'Orange');beam((x,y,1.1),(x*.5,y*.5,1.7),.10,'Metal')
 uv((x*.5,y*.5,1.7),(.2,.2,.2),'Ceramic');beam((x*.5,y*.5,1.7),(x*.3,y*.3,1),.07,'Graphite')
cyl((0,0,.25),1.6,.5,'Metal',12);cyl((0,0,.52),1.2,.08,'Cell');txt('HUMAN INDUSTRIAL / UNIT 001',(0,-2.7,.8),.12)
finish('Robot_Nest')
cyl((0,0,.3),1.8,.6,'Graphite',16);cyl((0,0,.7),1.5,.2,'Orange',16)
for i in range(8):
 a=i*math.tau/8;beam((1.4*math.cos(a),1.4*math.sin(a),.8),(.8*math.cos(a),.8*math.sin(a),2.2),.10,'Metal')
cyl((0,0,1.35),.7,1.1,'Ceramic');cyl((0,0,2.1),.5,.25,'Glow');finish('Assembly_Point')
box((0,0,.15),(5,2,.3),'Graphite');box((0,0,1.6),(4.6,1.6,2.8),'Ceramic')
for i in range(5):box((-1.7+i*.85,-.83,1.5),(.55,.08,1.8),'Graphite');box((-1.7+i*.85,-.89,1.5),(.28,.03,1.6),'Amber')
for x in (-2.2,2.2):beam((x,0,3),(x,0,4.5),.15,'Metal')
txt('THERMAL RECOVERY / 04',(0,-.9,2.8),.16);finish('Furnace_Bank')

# Small articulated creatures, individually recognizable at gameplay distance.
uv((0,0,.35),(.65,.36,.26),'Belly')
for i in range(7):
 x=-.56+i*.18;uv((x,0,.49),(.15,.41,.25),'Shell',12,6)
 for side in (-1,1):
  beam((x,side*.25,.3),(x+.07,side*.49,.10),.035,'Bone');beam((x+.07,side*.49,.10),(x+.14,side*.54,.035),.025,'Bone')
uv((.67,0,.35),(.20,.26,.18),'Shell');
for y in (-.13,.13):uv((.79,y,.40),(.045,.045,.045),'Graphite');beam((.74,y,.46),(1,y*2,.64),.017,'Bone')
finish('Woodlouse','none')
uv((0,0,.10),(.21,.055,.06),'Fungus')
for side in (-1,1):
 mesh('Veined wing',[(.12,0,.1),(.05,side*.38,.16),(-.20,side*.27,.10),(-.17,0,.08)],[(0,1,2),(0,2,3)],'Fungus')
 beam((.10,0,.11),(.05,side*.36,.165),.005,'Shell');beam((.05,side*.36,.165),(-.17,0,.09),.004,'Shell')
finish('Dust_Moth','none')
uv((0,0,.35),(.53,.25,.28),'Water')
for side in (-1,1):
 for x in (-.25,.23):
  beam((x,side*.17,.27),(x+.1,side*.43,.03),.055,'Metal');uv((x+.16,side*.45,.04),(.15,.09,.035),'Water')
 for i in range(3):box((.28-i*.10,side*.245,.42),(.035,.02,.23),'CellLine',.008,(0,-.2,0))
mesh('Tail fin',[(-.45,0,.35),(-.80,0,.65),(-.72,0,.08)],[(0,1,2)],'Water')
for y in (-.13,.13):uv((.46,y,.47),(.055,.05,.05),'Bone')
finish('Icegill','none')
for i in range(8):
 z=.2+i*.35;x=.10*math.sin(i*.4);cyl((x,0,z),.40-i*.013,.32,'Belly',12);ring((x,0,z+.14),.40-i*.013,.04,'Shell',12)
for i in range(10):
 a=i*math.tau/10;beam((.18+.30*math.cos(a),.30*math.sin(a),2.93),(.18+.12*math.cos(a),.12*math.sin(a),2.65),.045,'Bone')
finish('Burrow_Worm','none')
def miner(name,scale=1,fast=False):
 box((0,0,.9),(.95,.72,.55),'Graphite',.09);box((0,0,1.25),(.9,.8,.3),'Ceramic',.08)
 for x in (-.35,.35):
  for y in (-.45,.45):
   uv((x,y,.9),(.12,.12,.12),'Orange');beam((x,y,.85),(x*1.8,y*1.7,.45),.07,'Metal');beam((x*1.8,y*1.7,.45),(x*2,y*1.8,.06),.06,'Graphite');box((x*2,y*1.8,.05),(.22,.22,.10),'Metal')
 box((.52,0,1.25),(.18,.6,.35),'Orange');box((.62,0,1.28),(.025,.35,.10),'Glow')
 for y in (-.45,.45):
  beam((.15,y,1.1),(.75,y*1.5,.7),.09,'Metal');beam((.75,y*1.5,.7),(1.1,y*1.5,.3),.08,'Graphite')
  box((1.1,y*1.5,.2),(.4,.35,.35),'Orange',.04)
 txt('UNIT 001',(0,-.42,1.3),.08)
 for o in parts:o.location*=scale;o.scale*=scale
 finish(name,'none')
miner('Heavy_Miner',1.6);miner('Fast_Miner',.7,True);miner('Feign_Miner',1.1)

# Satellite survival corner and fuel/telescope modules.
for x in (-.75,.75):cyl((x,0,1.3),.55,2.6,'Ceramic');ring((x,0,.3),.56,.05,'Orange',16);ring((x,0,2.3),.56,.05,'Orange',16)
box((0,-.6,1.3),(.5,.25,.65),'Orange');box((0,-.75,1.4),(.32,.03,.25),'Cell');finish('Fuel_Depot')
for x,y in ((-1,-.6),(1,-.6),(0,1)):beam((x,y,0),(0,0,1.8),.07,'Metal')
beam((0,0,1.8),(0,0,2.6),.08,'Orange');o=cyl((0,0,2.8),.38,1.8,'Ceramic');o.rotation_euler.y=math.pi/3
o=cyl((.80,0,3.25),.3,.05,'Cell');o.rotation_euler.y=math.pi/3;finish('Telescope')
box((0,0,.25),(2,.85,.5),'Graphite',.08)
for x in (-.55,.55):box((x,0,.59),(.85,.65,.22),'Ceramic',.08)
box((0,.3,1.05),(2,.22,.9),'Orange',.08)
for x in (-1,1):box((x,0,.7),(.22,.9,.7),'Orange',.08)
finish('Jummt_Sofa')
box((0,0,.04),(1.4,.65,.08),'Graphite');beam((0,0,0),(0,0,.6),.08);box((0,0,1.1),(1.4,.18,.9),'Ceramic');box((0,-.1,1.1),(1.22,.025,.7),'Cell');txt('EARTH / 21:04',(0,-.12,1.2),.09);finish('Jummt_TV')
uv((0,0,1.12),(.36,.24,.47),'Ceramic');uv((0,0,1.85),(.29,.27,.30),'Orange');uv((.18,-.1,1.89),(.20,.23,.15),'Graphite')
for x in (-.18,.18):
 beam((x,0,.83),(x,0,.22),.12,'Ceramic');box((x,-.1,.1),(.28,.45,.2),'Graphite');beam((x*2,0,1.35),(x*2.5,-.15,.88),.11,'Ceramic');box((x*2.6,-.15,.82),(.14,.18,.18),'Graphite')
box((0,.27,1.2),(.45,.23,.55),'Graphite');box((-.20,-.23,1.2),(.15,.03,.18),'Orange');finish('Jummt_Suit','none')

manifest['palette']=COLORS
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Mars_Chapter_Refined.blend'))
print(json.dumps({k:v for k,v in manifest.items() if k!='palette'}))
