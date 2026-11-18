"""Author the wide TwinFork hull and separate replaceable equipment in centimetres."""
import bpy, math, os
from mathutils import Vector

OUT='D:/projects/space/SourceArt/ShipRefit'
os.makedirs(OUT,exist_ok=True)
os.makedirs(OUT+'/Previews',exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=.01
M={};parts={};group=[]
COLORS={'Ceramic':(.43,.48,.47),'Frame':(.022,.036,.046),'Metal':(.16,.22,.25),'Panel':(.085,.13,.15),'Teal':(.025,.29,.28),'Orange':(.9,.24,.045),'Glass':(.018,.075,.095),'Glow':(.035,.85,.68),'Amber':(1,.38,.045),'White':(.68,.85,.85),'Screen':(.003,.009,.018)}
for n,c in COLORS.items():
 m=bpy.data.materials.new('Refit_'+n);m.diffuse_color=(*c,1);m.use_nodes=True
 p=next(x for x in m.node_tree.nodes if x.type=='BSDF_PRINCIPLED')
 p.inputs['Base Color'].default_value=(*c,1);p.inputs['Metallic'].default_value=.65 if n in ['Metal','Frame','Panel'] else .3;p.inputs['Roughness'].default_value=.31
 if n in ['Glow','Amber','White']:p.inputs['Emission Color'].default_value=(*c,1);p.inputs['Emission Strength'].default_value=2 if n!='White' else .6
 M[n]=m
def finish(o,n,ma,bev=0):
 o.name=n;o.data.materials.append(M[ma]);group.append(o)
 if bev:
  b=o.modifiers.new('Edge radii','BEVEL');b.width=bev;b.segments=3
  bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=b.name)
  w=o.modifiers.new('Weighted normals','WEIGHTED_NORMAL')
  try:bpy.ops.object.modifier_apply(modifier=w.name)
  except:pass
 return o
def box(n,loc,dim,ma='Ceramic',bevel=1):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.dimensions=dim;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);return finish(o,n,ma,bevel)
def cyl(n,loc,r,d,ma='Metal',axis='Z',verts=32):
 bpy.ops.mesh.primitive_cylinder_add(vertices=verts,radius=r,depth=d,location=loc);o=bpy.context.object
 if axis=='X':o.rotation_euler[1]=math.pi/2
 if axis=='Y':o.rotation_euler[0]=math.pi/2
 bpy.ops.object.transform_apply(location=False,rotation=True,scale=True);return finish(o,n,ma,.5)
def line(n,a,b,w,ma='Metal'):
 v=Vector(b)-Vector(a);o=box(n,(Vector(a)+Vector(b))/2,(w,w,v.length),ma,.4);o.rotation_euler=v.to_track_quat('Z','Y').to_euler();return o
def text(n,s,loc,size,ma='White',rot=(0,0,0)):
 bpy.ops.object.text_add(location=loc,rotation=rot);o=bpy.context.object;o.data.body=s;o.data.size=size;o.data.align_x='CENTER';o.data.align_y='CENTER';o.data.extrude=.02
 bpy.ops.object.convert(target='MESH');return finish(o,n,ma)
def loft(n,rows,cy=0,ma='Ceramic',bevel=1):
 vs=[]
 for x,w,bot,top in rows:
  b=min(16,(top-bot)/5,w/4)
  vs.extend([(x,cy-w+b,bot),(x,cy+w-b,bot),(x,cy+w,bot+b),(x,cy+w,top-b),(x,cy+w-b,top),(x,cy-w+b,top),(x,cy-w,top-b),(x,cy-w,bot+b)])
 fs=[tuple(reversed(range(8))),tuple(range(len(vs)-8,len(vs)))]
 for i in range(len(rows)-1):
  for j in range(8):fs.append((i*8+j,i*8+(j+1)%8,(i+1)*8+(j+1)%8,(i+1)*8+j))
 me=bpy.data.meshes.new(n);me.from_pydata(vs,[],fs);me.update();o=bpy.data.objects.new(n,me);bpy.context.collection.objects.link(o);return finish(o,n,ma,bevel)
def savepart(n):
 global group
 bpy.ops.object.select_all(action='DESELECT')
 for o in group:o.select_set(True)
 bpy.context.view_layer.objects.active=group[0];bpy.ops.object.join();o=bpy.context.object;o.name=n
 bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
 bpy.ops.export_scene.fbx(filepath=OUT+'/'+n+'.fbx',use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False)
 parts[n]=o;group=[];o.hide_set(True);o.hide_render=True;return o

arm=[(-650,72,-55,75),(-540,105,-66,122),(-320,122,-68,128),(-20,125,-68,128),(250,118,-66,122),(520,105,-58,90),(650,75,-45,45)]
cabin=[(-470,150,-55,150),(-360,185,-62,222),(-200,190,-65,240),(80,180,-65,220),(260,150,-65,130),(300,140,-50,40)]
# Closed physical hulls are exported separately, so the widened walking aisles stay empty.
for sign,side in [(-1,'Left'),(1,'Right')]:loft('collision',arm,sign*520,'Frame',0);savepart('SM_Refit_Col_'+side)
loft('collision',cabin,0,'Frame',0);savepart('SM_Refit_Col_Cabin')
box('Rear bridge',(-548,0,25),(175,1180,166),'Frame',16);savepart('SM_Refit_Col_Bridge')

# Dark understructure, an exposed aft bridge and recessed power spine.
for sign in [-1,1]:loft('Arm pressure body',arm,sign*520,'Frame',2)
loft('Cabin pressure body',cabin,0,'Frame',2)
box('Aft bridge',(-548,0,25),(175,1180,166),'Frame',16)
for y in range(-420,421,70):box('Bridge heat exchanger',(-550,y,116),(130,35,12),'Metal',2)
for y in [-290,290]:
 box('Aft brace',(-530,y,20),(210,58,190),'Panel',8)
 for x in [-595,-475]:cyl('Bridge fastener',(x,y,125),8,10,'Metal')
savepart('SM_Refit_Frame')

# Segmented armor instead of a scaled smooth slab. Each module seat remains visually distinct.
for sign in [-1,1]:
 cy=sign*520
 for j in range(len(arm)-1):
  a=list(arm[j]);b=list(arm[j+1]);a[0]+=3;b[0]-=3
  a[1]+=3;b[1]+=3;a[2]+=4;b[2]+=4;a[3]+=4;b[3]+=4
  loft('Arm armor segment '+str(j),[a,b],cy,'Ceramic',1)
 # Teal shoulder band and orange service inset, mirrored port/starboard.
 for x,z,w in [(-390,135,175),(-35,135,210),(300,118,180)]:
  box('Raised dorsal spine',(x,cy,z),(w,130,8),'Panel',4)
  box('Teal dorsal stripe',(x,cy-40,z+5),(w-12,16,2),'Teal',1)
  for k in range(6):box('Louver',(x-w/2+19+k*23,cy+30,z+5),(11,38,4),'Metal',1)
 for x in [-455,-130,180,440]:
  box('Outer side armor',(x,cy+sign*115,17),(195,8,106),'Ceramic',5)
  box('Maintenance inset',(x,cy+sign*120,27),(126,2,20),'Panel',1)
  box('Service amber index',(x-58,cy+sign*122,27),(9,2,16),'Orange',.5)
  for xx in [-83,83]:cyl('Armor lock',(x+xx,cy+sign*122,-15),3,3,'Metal','Y',12)
 # The right interior station gets a purpose-built dark bay, not paint on a blank arm.
 for x in [-380,-40,250]:box('Inner service rail',(x,cy-sign*119,0),(180,6,26),'Panel',2)
 box('Nose armor',(642,cy,4),(18,128,72),'Ceramic',7)
 for yy in [-51,51]:box('Nose navigation marker',(653,cy+yy,4),(3,7,23),'Glow',1)
 for yy in [-44,44]:
  cyl('Aft engine shroud',(-650,cy+yy,3),34,35,'Panel','X')
  cyl('Aft engine rim',(-670,cy+yy,3),27,8,'Metal','X')
  cyl('Aft energized core',(-675,cy+yy,3),21,2,'Glow','X')
 text('Arm identifier','L / 01' if sign<0 else 'R / 02',(-470,cy,145),19,'White')
for j in range(len(cabin)-1):
 a=list(cabin[j]);b=list(cabin[j+1]);a[0]+=3;b[0]-=3;a[1]+=3;b[1]+=3;a[3]+=3;b[3]+=3
 loft('Cabin armor '+str(j),[a,b],0,'Ceramic',1)
# Raised cockpit glazing follows the cabin's sloping nose.
canopy=loft('Armored canopy',[(74,142,221,236),(120,134,200,217),(227,110,146,164)],0,'Glass',2)
for y in [-140,0,140]:line('Canopy frame',(78,y,239),(230,y*.77,167),5,'Panel')
for x,y,z in [(-330,-130,234),(-330,130,234),(-170,-155,246),(-170,155,246)]:box('Cabin ridge',(x,y,z),(110,16,5),'Teal',2)
for sign in [-1,1]:
 for x in [-340,-275,-210,-145]:box('Cabin side gill',(x,sign*192,85),(30,5,65),'Panel',2)
 box('Cabin handrail',(-120,sign*192,4),(215,5,7),'Metal',1)
 for x in [-230,-10]:box('Handrail mount',(x,sign*190,4),(14,14,22),'Panel',2)
box('Belly forward armor',(210,0,-63),(150,230,18),'Panel',4)
text('Ship designation','TWINFORK  /  R2',(-270,0,249),23,'Panel')
for y in [-95,95]:box('Aft ceramic beam',(-550,y,126),(148,160,10),'Ceramic',5)
savepart('SM_Refit_Armor')

# Reusable laser plugin: circular rail, yaw cradle, recoil-sliding dual emitter.
cyl('Isolated mounting flange',(0,0,6),42,12,'Frame')
cyl('Quick release ring',(0,0,14),37,6,'Metal')
for i in range(8):
 a=i*math.tau/8;cyl('Retention bolt',(32*math.cos(a),32*math.sin(a),18),3,4,'Orange',verts=12)
cyl('Power collector',(0,0,21),30,7,'Teal');savepart('SM_Laser_Mount')
cyl('Yaw race',(0,0,27),31,9,'Panel')
for y in [-27,27]:
 box('Elevator cheek',(-4,y,43),(58,14,42),'Ceramic',5)
 cyl('Pitch bearing',(0,y,48),14,18,'Metal','Y')
 cyl('Bearing cap',(0,y+(9 if y>0 else -9),48),8,2,'Orange','Y')
box('Rear power block',(-23,0,38),(39,38,30),'Panel',4);savepart('SM_Laser_Cradle')
# Gun origin is its pitch bearing at local zero; blueprint offsets that pivot to z=48.
box('Emitter body',(15,0,0),(96,36,29),'Frame',4)
for y in [-14,14]:
 cyl('Accelerator barrel',(61,y,0),10,98,'Metal','X')
 for x in [26,43,60,77]:cyl('Cooling fin',(x,y,0),14,5,'Panel','X')
 cyl('Energy collar',(86,y,0),12,7,'Teal','X')
 cyl('Ceramic muzzle',(104,y,0),13,24,'Ceramic','X')
 cyl('Muzzle throat',(117,y,0),8.5,2,'Frame','X')
 cyl('Laser lens',(118.5,y,0),5.8,1,'Glow','X')
box('Upper receiver',(5,0,19),(67,33,12),'Ceramic',3)
box('Charge spine',(5,0,26),(51,8,3),'Glow',1)
for x in [-21,-9,3,15]:box('Heatsink',(x,0,-20),(5,39,10),'Metal',1)
text('Gun ID','LZR-08',(1,0,27.8),8,'Panel')
savepart('SM_Laser_Gun')

# The exchange terminal faces local +Y; its lever is exported at its hinge origin.
box('Exchange mounting pad',(0,-8,0),(173,18,84),'Frame',7)
box('Isolator chassis',(0,3,-3),(166,32,80),'Metal',7)
box('Ceramic upper hood',(-15,20,30),(139,40,12),'Ceramic',4)
box('Screen bezel',(-17,25,5),(127,12,44),'Frame',4)
box('Screen glass',(-17,32,5),(115,2,32),'Screen',2)
box('Upper signal line',(-17,34,28),(105,2,2),'Glow',.5)
box('Lower storage shelf',(-17,30,-25),(136,51,15),'Panel',3)
for x in [-62,-25,12,49]:box('Shared bin seal',(x,57,-25),(27,2,6),'Teal',1)
box('Lever mechanism',(67,19,-6),(26,38,90),'Orange',4)
cyl('Lever pivot bearing',(67,41,-19),14,17,'Frame','Y')
cyl('Lever pivot cap',(67,51,-19),8,4,'Metal','Y')
for x in [-73,73]:
 for z in [-35,35]:cyl('Mounting stud',(x,26,z),3,5,'Metal','Y',12)
savepart('SM_StellarDock_Body')
line('Lever steel shaft',(0,0,0),(0,0,40),6,'Metal')
cyl('Mechanical pivot',(0,0,0),9,14,'Metal','X')
cyl('Grip',(0,0,43),10,27,'Orange','X')
for x in [-8,0,8]:cyl('Grip knurl',(x,0,43),10.7,2,'Frame','X')
savepart('SM_StellarDock_Lever')
bpy.ops.mesh.primitive_cylinder_add(vertices=16,radius=.5,depth=1);finish(bpy.context.object,'Unit laser','Glow');savepart('SM_Laser_BeamUnit')
bpy.ops.mesh.primitive_uv_sphere_add(segments=12,ring_count=8,radius=.5);finish(bpy.context.object,'Unit impact','Glow');savepart('SM_Laser_ImpactUnit')

# Arrange a reviewable assembly; exports above remain independent replaceable pieces.
for n in ['SM_Refit_Frame','SM_Refit_Armor']:parts[n].hide_set(False);parts[n].hide_render=False
for cy in [-520,520]:
 for x,z,scale in [(650,5,.95),(130,130,1),(-320,135,1)]:
  for n in ['SM_Laser_Mount','SM_Laser_Cradle','SM_Laser_Gun']:
   o=parts[n].copy();o.data=parts[n].data;bpy.context.collection.objects.link(o);o.hide_set(False);o.hide_render=False;o.location=(x,cy,z+(48*scale if n.endswith('Gun') else 0));o.scale=(scale,)*3
for x,z,scale in [(270,-65,1.4),(-120,238,1.2)]:
 for n in ['SM_Laser_Mount','SM_Laser_Cradle','SM_Laser_Gun']:
  o=parts[n].copy();o.data=parts[n].data;bpy.context.collection.objects.link(o);o.hide_set(False);o.hide_render=False;o.location=(x,0,z+((-48 if z<0 else 48)*scale if n.endswith('Gun') else 0));o.scale=(scale,)*3
  if z<0:o.rotation_euler.x=math.pi
o=parts['SM_StellarDock_Body'];o.hide_set(False);o.hide_render=False;o.location=(360,394,-5);o.rotation_euler.z=math.pi
o=parts['SM_StellarDock_Lever'];o.hide_set(False);o.hide_render=False;o.location=(293,343,-24);o.rotation_euler.z=math.pi

# Studio render without adding lights or cameras to the exported gameplay meshes.
world=bpy.context.scene.world;world.use_nodes=True;next(n for n in world.node_tree.nodes if n.type=='BACKGROUND').inputs[0].default_value=(.055,.075,.09,1)
for loc,power,size in [((600,-800,1300),22000000,900),((-700,600,950),18000000,750),((800,900,500),9000000,600)]:
 bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.data.energy=power;o.data.shape='DISK';o.data.size=size;o.rotation_euler=(Vector((0,0,0))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(1900,-1800,1500));cam=bpy.context.object;cam.rotation_euler=(Vector((0,0,30))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=2050;cam.data.clip_end=20000
s=bpy.context.scene;s.camera=cam
try:s.render.engine='CYCLES';s.cycles.samples=24
except:pass
s.render.resolution_x=1500;s.render.resolution_y=1100;s.render.resolution_percentage=100;s.render.filepath=OUT+'/Previews/Twinfork_R2_review.png';s.render.image_settings.file_format='PNG'
bpy.ops.wm.save_as_mainfile(filepath=OUT+'/ShipRefit.blend')
bpy.ops.render.render(write_still=True)
print('SHIP_REFIT_ART_COMPLETE')
