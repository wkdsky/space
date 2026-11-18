import bpy,math
from mathutils import Vector
from pathlib import Path
P=Path('D:/projects/space/SourceArt/ShipRefit/Mechanics')
bpy.ops.wm.open_mainfile(filepath=str(P/'ShipMechanics.blend'))
sources={o.name:o for o in bpy.data.objects}
def empty(n,loc=(0,0,0),parent=None):
 o=bpy.data.objects.new(n,None);bpy.context.collection.objects.link(o);o.location=loc;o.parent=parent;return o
def part(n,loc=(0,0,0),yaw=0,parent=None):
 o=sources[n].copy();o.data=sources[n].data;bpy.context.collection.objects.link(o);o.name='Assembly_'+n
 o.hide_set(False);o.hide_render=False;o.parent=parent;o.location=loc;o.rotation_euler.z=math.radians(yaw);o.scale=(-1,1,1);return o
cab=empty('Detachable cockpit')
for n in ['SM_Mech_CabinArmor','SM_Mech_CabinFrame']:part(n,parent=cab)
for n in ['SM_Mech_ForkArmor','SM_Mech_ForkFrame']:part(n)
part('SM_Mech_MainCannon',(0,0,-65),parent=cab)
sliders=[];pins=[]
for sign in [-1,1]:
 yaw=180 if sign<0 else 0
 part('SM_Mech_CabinSocket',(0,sign*191,32),yaw,parent=cab)
 part('SM_Mech_CouplerHousing',(0,sign*402,32),yaw)
 s=part('SM_Mech_CouplerSlide',(0,sign*402,32),yaw);sliders.append((s,sign))
 # Separate pin object positions are keyed in world space to avoid mirrored parenting.
 p=part('SM_Mech_CouplerPins',(0,sign*209,64),yaw);pins.append((p,sign))
part('SM_Mech_RearCradle',(-510,0,35));rear=part('SM_Mech_RearPiston',(-560,0,35))
gears=[]
for x in [250,-470]:
 for y in [-555,555]:
  hip=empty('Gear hip',(x,y,-62));part('SM_Mech_GearUpper',parent=hip)
  shin=empty('Hydraulic knee',(0,0,-70),hip);part('SM_Mech_GearShin',parent=shin)
  foot=empty('Swivel foot',(0,0,-40),shin);part('SM_Mech_GearFoot',parent=foot)
  gears.append((hip,shin,foot))
def smooth(a,b,t):
 q=max(0,min(1,(t-a)/(b-a)));return q*q*(3-2*q)
for frame in range(1,361,3):
 bpy.context.scene.frame_set(frame)
 if frame<90:
  q=max(0,min(1,(frame-15)/75));side=1-smooth(.12,.38,q);lock=1-smooth(0,.12,q)
  travel=.12*smooth(.4,.66,q)+.88*smooth(.66,1,q);push=smooth(.4,.66,q)*(1-smooth(.72,.96,q))
 elif frame<135:side=lock=push=0;travel=1
 elif frame<241:
  q=(frame-135)/105;travel=1-.88*smooth(0,.48,q)-.12*smooth(.52,.72,q)
  side=smooth(.74,.94,q);lock=smooth(.94,1,q);push=smooth(.35,.48,q)*(1-smooth(.52,.72,q))
 else:side=lock=1;travel=push=0
 cab.location.x=travel*1000;cab.keyframe_insert('location')
 for s,sign in sliders:s.location.y=sign*(402+(1-side)*158);s.keyframe_insert('location')
 for p,sign in pins:p.location.y=sign*(209+(1-side)*158);p.location.z=64+(1-lock)*32;p.keyframe_insert('location')
 rear.location.x=-560+120*push;rear.keyframe_insert('location')
 fold=smooth(255,300,frame)*(1-smooth(315,355,frame))
 slide=smooth(0,.42,fold);hinge=smooth(.28,1,fold)
 for hip,shin,foot in gears:
  hip.rotation_euler.y=math.radians(-92)*hinge;hip.keyframe_insert('rotation_euler')
  shin.location.z=-70+34*slide;shin.keyframe_insert('location')
  foot.rotation_euler.y=math.radians(92)*slide;foot.keyframe_insert('rotation_euler')
# Add the existing replaceable dorsal weapons to the full review assembly.
with bpy.data.libraries.load('D:/projects/space/SourceArt/ShipRefit/ShipRefit.blend',link=False) as (a,b):
 b.objects=[n for n in ['SM_Laser_Mount','SM_Laser_Cradle','SM_Laser_Gun'] if n in a.objects]
weapons={o.name.split('.')[0]:o for o in b.objects if o}
for o in weapons.values():bpy.context.collection.objects.link(o);o.hide_render=True;o.hide_set(True)
for x,y,z,sc in [(648,-520,6,.95),(130,-520,130,1),(-320,-520,135,1),(648,520,6,.95),(130,520,130,1),(-320,520,135,1),(-120,0,238,1.2)]:
 for n,src in weapons.items():
  o=src.copy();o.data=src.data;bpy.context.collection.objects.link(o);o.hide_set(False);o.hide_render=False;o.location=(x,y,z+(48*sc if n.endswith('Gun') else 0));o.scale=(sc,)*3
  if y==0:o.parent=cab
bpy.ops.mesh.primitive_plane_add(size=20000,location=(0,0,-192));ground=bpy.context.object
m=bpy.data.materials.new('Review floor');m.use_nodes=True
next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED').inputs['Base Color'].default_value=(.12,.15,.18,1);ground.data.materials.append(m)
s=bpy.context.scene;s.world.use_nodes=True
next(n for n in s.world.node_tree.nodes if n.type=='BACKGROUND').inputs[0].default_value=(.08,.1,.13,1)
for loc,power,size in [((900,-1000,1700),32000000,1100),((-900,500,1000),22000000,900),((700,1200,700),16000000,800)]:
 bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.data.energy=power;o.data.shape='DISK';o.data.size=size;o.rotation_euler=(-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(1800,-1800,1000));cam=bpy.context.object
cam.rotation_euler=(Vector((0,0,5))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=1900;cam.data.clip_end=30000;s.camera=cam
try:s.render.engine='CYCLES';s.cycles.samples=16
except TypeError:pass
s.render.resolution_x=1500;s.render.resolution_y=1050;s.render.resolution_percentage=100
s.render.image_settings.file_format='PNG';s.frame_start=1;s.frame_end=360;s.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(P/'ShipMechanics_Assembly.blend'))
(P/'Previews').mkdir(parents=True,exist_ok=True)
s.render.filepath=str(P/'Previews/Mechanics_deployed.png');bpy.ops.render.render(write_still=True)
s.frame_set(300);s.render.filepath=str(P/'Previews/Mechanics_retracted.png');bpy.ops.render.render(write_still=True)
print('MECHANICS_PREVIEW_COMPLETE')
