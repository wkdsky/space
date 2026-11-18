"""Separate the existing cockpit and author articulated landing/coupling hardware in cm."""
import bpy, bmesh, math
from pathlib import Path
from mathutils import Vector, Matrix
BASE=Path('D:/projects/space/SourceArt/ShipRefit')
bpy.ops.wm.open_mainfile(filepath=str(BASE/'ShipRefit.blend'))
source=(BASE/'build_ship_refit.py').read_text(encoding='utf8').split('arm=[')[0]
source=source.replace("bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)","")
source=source.replace("OUT='D:/projects/space/SourceArt/ShipRefit'","OUT='D:/projects/space/SourceArt/ShipRefit/Mechanics'")
source=source.replace(" bpy.ops.export_scene.fbx("," o.data.transform(Matrix.Diagonal((-1,1,1,1)));bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.reverse_faces(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free()\n bpy.ops.export_scene.fbx(")
exec(compile(source,'refit_helpers','exec'),globals())

def split(source_name, cabin_name, frame_name):
 src=bpy.data.objects[source_name]
 for central,name in [(True,cabin_name),(False,frame_name)]:
  o=src.copy();o.data=src.data.copy();bpy.context.collection.objects.link(o)
  bm=bmesh.new();bm.from_mesh(o.data)
  remove=[]
  for v in bm.verts:
   is_cabin=abs(v.co.y)<240 and v.co.x>=-472
   if is_cabin!=central:remove.append(v)
  bmesh.ops.delete(bm,geom=remove,context='VERTS');bm.to_mesh(o.data);bm.free()
  global group
  group=[o];savepart(name)
split('SM_Refit_Armor','SM_Mech_CabinArmor','SM_Mech_ForkArmor')
split('SM_Refit_Frame','SM_Mech_CabinFrame','SM_Mech_ForkFrame')

# Hip-centred twin cheek frame. Knee is -70cm; overlapping piston keeps every joint connected.
box('Belly load plate',(0,0,3),(104,90,16),'Frame',5)
for x in [-38,38]:
 for y in [-28,28]:cyl('Structural fastener',(x,y,-7),5,5,'Metal')
cyl('Hip cross axle',(0,0,-13),20,92,'Metal','Y')
for y in [-32,32]:
 line('Load fork',(0,y,-13),(0,y,-68),19,'Panel')
 box('Ceramic cheek',(2,y,-39),(37,15,48),'Ceramic',4)
 cyl('Hip cap',(0,y*1.5,-13),15,5,'Orange','Y')
 line('Triangular tie',(-34,y,-11),(0,y,-68),9,'Metal')
box('Knee yoke',(0,0,-70),(51,77,20),'Frame',4)
cyl('Knee hinge',(0,0,-70),17,88,'Metal','Y')
for y in [-47,47]:cyl('Knee lock',(0,y,-70),10,5,'Orange','Y')
cyl('Hydraulic sleeve',(0,0,-40),17,58,'Frame')
savepart('SM_Mech_GearUpper')
# Lower piston origin is knee. The foot attaches at -40 and is never independently translated in XY.
cyl('Chromed ram',(0,0,-17),12,54,'Metal')
cyl('Dust seal',(0,0,-4),18,12,'Orange')
for y in [-21,21]:line('Anti torsion guide',(0,y,5),(0,y,-33),8,'Panel')
box('Ankle fork',(0,0,-36),(45,57,18),'Frame',4)
cyl('Ankle pin',(0,0,-40),12,70,'Metal','Y')
savepart('SM_Mech_GearShin')
# A broad chamfered sole with ribs and two independent load paths; sole plane z=-20.
box('Load spreader',(0,0,-8),(106,78,20),'Frame',8)
box('Ceramic toe',(27,0,1),(42,75,8),'Ceramic',3)
box('Ceramic heel',(-28,0,1),(30,75,8),'Ceramic',3)
cyl('Spherical ankle',(0,0,0),17,36,'Metal','Y')
for x in [-35,-18,18,35]:box('Sole traction rib',(x,0,-18),(9,70,4),'Metal',1)
for y in [-39,39]:
 box('Amber sole edge',(0,y,-5),(63,3,5),'Orange',1)
 line('Foot gusset',(-30,y*.6,-4),(0,y*.6,9),8,'Panel')
savepart('SM_Mech_GearFoot')

# Fixed axial main cannon: deep shielded energy magazine, flush dual aperture at the cabin nose.
loft('Armored power keel',[(-300,87,-74,0),(-230,109,-94,0),(130,111,-94,0),(258,90,-70,-3),(276,75,-60,-8)],0,'Frame',4)
for y in [-96,96]:
 box('Ceramic reactor flank',(-25,y,-43),(360,14,61),'Ceramic',7)
 box('Energy bus',(-50,y*1.08,-33),(280,4,10),'Teal',2)
 for x in [-170,-130,-90,-50,-10,30]:box('Cooling slot',(x,y*1.09,-64),(16,3,19),'Panel',2)
for x in [-205,-100,5,110]:
 box('Armored keel ring',(x,0,-89),(15,178,12),'Metal',3)
for y in [-38,38]:
 cyl('Power cell',(-105,y,-45),28,260,'Panel','X')
 for x in [-180,-125,-70,-15]:cyl('Containment collar',(x,y,-45),31,10,'Metal','X')
 cyl('Emitter socket',(260,y,-37),29,27,'Metal','X')
 cyl('Recessed throat',(275,y,-37),22,3,'Screen','X')
 cyl('Focusing ring',(277,y,-37),16,2,'Teal','X')
 cyl('Axial lens',(278,y,-37),11,1,'Glow','X')
box('Upper flange',(-10,0,0),(500,220,15),'Panel',5)
savepart('SM_Mech_MainCannon')

# Each side has a fixed outer sleeve and nested inner carriage, with a matching cabin receptacle.
# All local +Y axes run from cabin toward fork; mirror using the component yaw.
box('Outer saddle',(0,0,0),(122,32,102),'Frame',7)
box('Ceramic shoulder',(0,-7,4),(116,27,94),'Ceramic',6)
box('Telescopic outer housing',(0,-52,0),(96,104,68),'Panel',6)
for x in [-40,40]:box('Guide rail',(x,-54,0),(10,96,43),'Metal',2)
for z in [-37,37]:box('Reinforced sleeve',(0,-56,z),(107,101,10),'Frame',3)
box('Status strip',(0,-51,43),(76,65,3),'Teal',1)
savepart('SM_Mech_CouplerHousing')
box('Inner sliding beam',(0,-118,0),(62,156,46),'Metal',4)
for x in [-34,34]:box('Wear rail',(x,-116,0),(5,150,29),'Panel',1)
box('Capture head',(0,-193,0),(98,26,79),'Frame',5)
for x in [-30,30]:
 cyl('Guide dowel',(x,-212,0),8,27,'Metal','Y')
box('Lock status',(0,-210,23),(43,3,7),'Glow',1)
savepart('SM_Mech_CouplerSlide')
box('Cockpit docking shoe',(0,0,0),(126,22,104),'Frame',5)
for x in [-46,46]:box('Ceramic interface ear',(x,10,0),(27,24,91),'Ceramic',4)
box('Capture recess',(0,13,0),(66,5,55),'Panel',3)
for x in [-30,30]:cyl('Dowel receiver',(x,17,0),11,8,'Frame','Y')
box('Dock power socket',(0,18,27),(41,6,10),'Teal',2)
savepart('SM_Mech_CabinSocket')
for x in [-43,43]:
 cyl('Mechanical lock pin',(x,0,0),7,32,'Metal')
 box('Pin actuator',(x,0,20),(19,31,18),'Orange',3)
savepart('SM_Mech_CouplerPins')

# Rear central cradle bolts to the existing transverse bridge. Two rails eject the cabin forward.
box('Rear capture cradle',(0,0,0),(40,292,188),'Frame',9)
for y in [-104,104]:
 cyl('Ejection sleeve',(22,y,0),24,84,'Panel','X')
 cyl('Collar',(64,y,0),29,12,'Metal','X')
 for z in [-65,65]:box('Rear capture jaw',(34,y,z),(62,37,27),'Ceramic',4)
box('Magnetic capture bar',(30,0,0),(29,140,104),'Panel',7)
for y in [-55,55]:box('Rear capture indicator',(47,y,0),(3,7,67),'Glow',1)
savepart('SM_Mech_RearCradle')
for y in [-104,104]:cyl('Ejection ram',(60,y,0),15,96,'Metal','X')
box('Ejection contact plate',(111,0,0),(18,250,115),'Frame',6)
for y in [-104,104]:box('Magnetic pads',(122,y,0),(7,41,70),'Teal',3)
savepart('SM_Mech_RearPiston')

# The old workstation bay used a large black face over the intact white hull.
# Retain its buried guide hardware, removing that redundant exterior backing on both stations.
with bpy.data.libraries.load('D:/projects/space/SourceArt/ShipDocks/ShipDocks.blend',link=False) as (a,b):
 b.objects=['SM_Dock_Material_Mount','SM_Dock_Weapon_Mount']
for kind,o in zip(['Material','Weapon'],b.objects):
 bpy.context.collection.objects.link(o);o.data=o.data.copy();o.location=(0,0,0);o.rotation_euler=(0,0,0);o.scale=(1,1,1)
 bm=bmesh.new();bm.from_mesh(o.data)
 bmesh.ops.delete(bm,geom=[v for v in bm.verts if v.co.y>-5],context='VERTS');bm.to_mesh(o.data);bm.free()
 group=[o];savepart('SM_Mech_Dock'+kind+'Mount')

# Export only the independent parts, preserving the original refit .blend intact.
for o in list(bpy.data.objects):
 if o not in parts.values():bpy.data.objects.remove(o,do_unlink=True)
for o in parts.values():o.hide_set(True);o.hide_render=True
bpy.ops.wm.save_as_mainfile(filepath=OUT+'/ShipMechanics.blend')
print('SHIP_MECHANICS_ART_COMPLETE',len(parts))
