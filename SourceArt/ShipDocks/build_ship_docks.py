import bpy, math, os
from mathutils import Vector
OUT='D:/projects/space/SourceArt/ShipDocks'
os.makedirs(OUT,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=0.01
M={}
def mat(n,c,metal=0,rough=.4,em=0):
 m=bpy.data.materials.new(n);m.diffuse_color=(*c,1);m.use_nodes=True
 p=next(x for x in m.node_tree.nodes if x.type=='BSDF_PRINCIPLED');p.inputs['Base Color'].default_value=(*c,1);p.inputs['Metallic'].default_value=metal;p.inputs['Roughness'].default_value=rough
 if em:p.inputs['Emission Color'].default_value=(*c,1);p.inputs['Emission Strength'].default_value=em
 M[n]=m
mat('Dock_Ivory',(.55,.58,.56),.4);mat('Dock_Graphite',(.022,.035,.046),.65);mat('Dock_Steel',(.19,.24,.27),.85,.23);mat('Dock_Screen',(.002,.012,.023),.2,.25)
mat('Dock_Teal',(.015,.5,.43),.45);mat('Dock_Orange',(.85,.26,.045),.45);mat('Dock_GlowTeal',(.025,.8,.61),.1,.3,2);mat('Dock_GlowOrange',(1,.32,.035),.1,.3,2);mat('Dock_Ink',(.14,.25,.28),.3);mat('Dock_White',(.65,.85,.87),.1,.4,1)
parts={};group=[]
def finish(o,name,ma,bev=0):
 o.name=name;o.data.materials.append(M[ma]);group.append(o)
 if bev:
  b=o.modifiers.new('Machined edges','BEVEL');b.width=bev;b.segments=3
  bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=b.name)
  n=o.modifiers.new('Weighted normals','WEIGHTED_NORMAL')
  try:bpy.ops.object.modifier_apply(modifier=n.name)
  except:pass
 return o
def box(n,loc,dim,ma,bev=.7):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.dimensions=dim;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);return finish(o,n,ma,bev)
def cyl(n,loc,r,depth,ma,axis='Z',verts=24):
 bpy.ops.mesh.primitive_cylinder_add(vertices=verts,radius=r,depth=depth,location=loc);o=bpy.context.object
 if axis=='Y':o.rotation_euler[0]=math.pi/2
 if axis=='X':o.rotation_euler[1]=math.pi/2
 bpy.ops.object.transform_apply(location=False,rotation=True,scale=True);return finish(o,n,ma,.35)
def line(n,a,b,width,ma):
 d=Vector(b)-Vector(a);o=box(n,(Vector(a)+Vector(b))/2,(width,width,d.length),ma,.12);o.rotation_euler=d.to_track_quat('Z','Y').to_euler();return o
def text(n,s,x,y,z,size,ma):
 bpy.ops.object.text_add(location=(x,y,z),rotation=(math.pi/2,0,math.pi));o=bpy.context.object;o.data.body=s;o.data.size=size;o.data.extrude=.025;o.data.align_x='CENTER';o.data.align_y='CENTER'
 bpy.context.view_layer.objects.active=o;bpy.ops.object.convert(target='MESH');finish(o,n,ma)
def savepart(n):
 global group
 bpy.ops.object.select_all(action='DESELECT')
 for o in group:o.select_set(True)
 bpy.context.view_layer.objects.active=group[0];bpy.ops.object.join();o=bpy.context.object;o.name=n
 bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');parts[n]=o
 bpy.ops.export_scene.fbx(filepath=OUT+'/'+n+'.fbx',use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,add_leaf_bones=False,bake_anim=False)
 group=[];o.hide_set(True)
for kind in ['Material','Weapon']:
 accent='Dock_Teal' if kind=='Material' else 'Dock_Orange';glow='Dock_GlowTeal' if kind=='Material' else 'Dock_GlowOrange';W=122 if kind=='Material' else 140
 # Flush bay rim and internal guide rails. Only a shallow seam remains outside the arm.
 bw=W+12;bh=112
 box('Bay recess',(0,-.4,0),(bw,1.2,bh),'Dock_Graphite',1)
 for x in [-bw/2,bw/2]:box('Bay vertical seal',(x,.2,0),(2,1.4,bh),'Dock_Steel',.5)
 for z in [-bh/2,bh/2]:box('Bay horizontal seal',(0,.2,z),(bw,1.4,2),'Dock_Steel',.5)
 for x in [-W/2+8,W/2-8]:
  box('Internal guide',(x,-68,0),(8,12,72),'Dock_Graphite',1)
  box('Rail',(x,-60,0),(3,3,62),'Dock_Steel',.5)
 savepart('SM_Dock_'+kind+'_Mount')
 for leaf,sign in [('Upper',1),('Lower',-1)]:
  box('Flush shutter',(0,1,sign*28),(bw-3,2,54.5),'Dock_Ivory',.7)
  box('Shutter seal',(0,2.1,sign*.9),(bw-5,.4,1),'Dock_Graphite',.15)
  if leaf=='Upper':
   box('Recessed label',(-bw/2+26,2.2,8),(42,.4,7),'Dock_Graphite',.3)
   text('Bay ID','MP / 01' if kind=='Material' else 'WM / 02',-bw/2+26,2.5,8,3.8,'Dock_White')
   box('Status light',(bw/2-12,2.5,8),(9,.4,1.4),glow,.1)
  for x in [-bw/2+7,bw/2-7]:cyl('Flush rivet',(x,2.2,sign*48),1.2,.6,'Dock_Steel','Y',8)
  savepart('SM_Dock_'+kind+'_'+leaf+'Door')
 # Carriage. The material processor has twin reaction vessels, weapon workshop has an armored tool cabinet.
 box('Backbone',(0,-2,-3),(W,37,66),'Dock_Graphite',4)
 box('Lower casing',(0,8,-24),(W,39,19),'Dock_Ivory',3)
 box('Accent belt',(0,29,-27),(W-6,3,6),accent,.7)
 if kind=='Material':
  for x in [-43,43]:
   cyl('Reaction vessel',(x,20,2),16,48,'Dock_Steel')
   for z in [-18,16]:cyl('Pressure band',(x,20,z),17,4,accent)
   box('Fluid indicator',(x,36,0),(6,2,24),glow,.5)
   line('Feed pipe',(x,0,21),(x,0,38),4,'Dock_Steel')
  box('Control manifold',(0,23,0),(42,18,42),'Dock_Ivory',3)
  for z in [-10,0,10]:box('Vent',(0,33,z),(26,2,3),'Dock_Graphite',.3)
 else:
  box('Tool cabinet',(-43,24,-2),(43,17,43),accent,3)
  for z in [-15,-3,9]:
   box('Drawer',(-43,34,z),(33,4,9),'Dock_Graphite',.6);box('Handle',(-43,37,z),(15,3,2),'Dock_Steel',.5)
  for x in [6,24,42]:
   box('Heat sink',(x,21,0),(9,23,40),'Dock_Steel',1)
   for z in [-14,-6,2,10]:box('Cooling slot',(x,34,z),(6,2,3),'Dock_Graphite',.2)
  cyl('Calibration rotor',(55,21,0),12,15,'Dock_Graphite','Y')
  cyl('Calibration light',(55,30,0),5,3,glow,'Y')
 for x in [-W/2+8,W/2-8]:
  cyl('Pivot',(x,27,-21),6,5,accent,'X')
 savepart('SM_Dock_'+kind+'_Body')
 # Lid/display: authored relative to its lower hinge, front normal +Y.
 sw=106 if kind=='Material' else 126;sh=64 if kind=='Material' else 59
 box('Display armor',(0,0,sh/2),(sw+8,9,sh+8),'Dock_Ivory' if kind=='Material' else 'Dock_Graphite',3)
 box('Bezel',(0,5,sh/2),(sw,3,sh),'Dock_Graphite',2)
 box('OLED',(0,7,sh/2),(sw-10,1,sh-10),'Dock_Screen',1)
 box('Top light',(0,7.8,sh-4),(sw-15,.6,1.2),glow,.2)
 text('Panel title','MATTER / PROCESS' if kind=='Material' else 'ARMAMENT / WORKSHOP',0,8,sh-12,4.7,glow)
 if kind=='Material':
  # Three linked reaction nodes, feeds, graph traces and a spectrum.
  for x,z,r in [(31,33,7),(0,26,10),(-33,33,7)]:
   bpy.ops.mesh.primitive_torus_add(major_radius=r,minor_radius=.65,major_segments=24,minor_segments=6,location=(x,8.1,z),rotation=(math.pi/2,0,0));finish(bpy.context.object,'Reaction node',glow)
   cyl('Node core',(x,8.3,z),2,1,glow,'Y',6)
  for a,b in [((24,8,33),(10,8,26)),((-10,8,26),(-26,8,33)),((0,8,16),(0,8,9))]:line('Flow path',a,b,.7,glow)
  for i in range(18):box('Spectral bars',(-42+i*5,8,8),(2,.6,3+(i*7)%9),glow,.1)
  text('Footer','FEED    >    REFINE    >    STORE',0,8,3,3,'Dock_White')
 else:
  # Schematic rifle, six tuning traces, calibration rings.
  for loc,dim in [((5,8,30),(45,1,9)),((-30,8,31),(28,1,3)),((33,8,31),(16,1,13)),((12,8,22),(5,1,10))]:box('Weapon schematic',loc,dim,glow,.5)
  line('Stock',(27,8,26),(39,8,22),2,glow)
  box('Receiver',(3,8,36),(17,1,3),'Dock_White',.3)
  for i in range(6):
   x=-45+i*18;box('Tuning baseline',(x,8,10),(12,.5,1),'Dock_Ink',.1);box('Tuning value',(x+3,8,13+i%3),(5,.7,3+i%3),glow,.2)
  text('Footer','CALIBRATION  //  6 CHANNELS',0,8,3,3.5,'Dock_White')
 for x in [-sw/2+2,sw/2-2]:
  for z in [4,sh-4]:cyl('Display screw',(x,6,z),1.5,2,'Dock_Steel','Y',6)
 savepart('SM_Dock_'+kind+'_Screen')
 # Fold-down service tray: materials feed funnel versus long gun cradle and tool rail.
 tw=120 if kind=='Material' else 144;td=47 if kind=='Material' else 57
 box('Worktop',(0,td/2,0),(tw,td,6),'Dock_Graphite',2)
 box('Front bumper',(0,td-1,0),(tw+3,7,11),accent,2)
 if kind=='Material':
  box('Intake tray',(-25,24,5),(52,32,5),'Dock_Steel',3)
  for x in [-49,-1]:box('Intake wall',(x,24,10),(3,32,10),'Dock_Ivory',1)
  for y in [11,20,29,38]:cyl('Feed roller',(-25,y,9),2.8,42,'Dock_Graphite','X')
  for x in [18,38]:
   cyl('Sample socket',(x,25,6),8,5,'Dock_Steel');cyl('Socket light',(x,25,9),4,1,glow)
 else:
  for x in [-42,29]:
   box('Gun cradle',(x,29,8),(9,24,12),'Dock_Steel',1)
   for y in [20,38]:box('Cradle jaw',(x,y,17),(10,5,13),accent,1)
  for x in [-50,-25,0,25,50]:box('Tool inset',(x,8,4),(13,7,1),'Dock_Steel',.5)
 for i in range(8):
  o=box('Warning stripe',(-tw/2+10+i*7,td+3,2),(3,.8,6),'Dock_Graphite',.1);o.rotation_euler[1]=-.4
 savepart('SM_Dock_'+kind+'_Tray')
# Dedicated central hull console, with a ship silhouette and subsystem status strip.
box('Terminal gasket',(0,-2,0),(139,8,102),'Dock_Graphite',6)
box('Terminal casing',(0,0,0),(134,10,97),'Dock_Ivory',5)
box('Armored bezel',(0,5,0),(124,5,86),'Dock_Graphite',4)
box('Terminal glass',(0,8,0),(113,1,75),'Dock_Screen',2)
text('Console header','EXPEDITION / CORE',0,9,29,5,'Dock_GlowTeal')
text('Console footer','SHIP MANAGEMENT',0,9,-28,4.5,'Dock_White')
# Twin arms, bridge and central cabin rendered as a luminous technical schematic.
for x in [-22,22]:
 box('Ship arm',(x,9,0),(8,.6,28),'Dock_GlowTeal',1)
 line('Nose',(x-4,9,14),(x+4,9,10),.65,'Dock_White')
box('Ship bridge',(0,9,1),(40,.6,5),'Dock_Ink',.4)
box('Ship cabin',(0,9,5),(11,.8,19),'Dock_GlowTeal',1)
for side in [-1,1]:
 for i in range(3):
  x=side*44;z=12-i*10
  box('Status track',(x,9,z),(13,.4,2),'Dock_Ink',.2)
  box('Status value',(x-1,9.5,z),(10-i*2,.5,2),'Dock_GlowTeal',.2)
for x in [-62,62]:
 for z in [-40,40]:cyl('Console bolt',(x,6,z),2,2,'Dock_Steel','Y',8)
box('Console lower key',(0,6,-43),(32,4,6),'Dock_Graphite',1)
box('Console wake indicator',(0,9,-43),(20,.5,1.2),'Dock_GlowTeal',.1)
savepart('SM_Ship_CentralTerminal')
cyl('Telescopic piston',(0,0,0),2,100,'Dock_Steel')
savepart('SM_Dock_Strut')
# Save an assembled, lit source scene and a review image.
for o in parts.values():o.hide_set(False)
for kind,offset in [('Material',-95),('Weapon',95)]:
 for part in ['Mount','UpperDoor','LowerDoor','Body','Screen','Tray']:
  o=parts['SM_Dock_'+kind+'_'+part];o.location.x+=offset
  if part in ['Body','Screen','Tray']:o.location+=Vector((0,35,-110))
  if part=='UpperDoor':o.location+=Vector((0,-12,54))
  if part=='LowerDoor':o.location+=Vector((0,-12,-54))
  if part=='Screen':o.location+=Vector((0,30,20));o.rotation_euler.x=math.radians(-8)
  if part=='Tray':o.location+=Vector((0,30,-20))
scene=bpy.context.scene
try:scene.render.engine='CYCLES'
except:pass
scene.cycles.samples=24;scene.render.resolution_x=1400;scene.render.resolution_y=850;scene.render.resolution_percentage=100
scene.world.color=(.12,.12,.12)
bpy.ops.object.camera_add(location=(360,560,220));cam=bpy.context.object;cam.rotation_euler=(Vector((0,30,-35))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=480;scene.camera=cam
for loc,power,size in [((180,200,400),1800000,350),((-300,100,140),1000000,300),((0,-160,250),2000000,200)]:
 bpy.ops.object.light_add(type='AREA',location=loc);l=bpy.context.object;l.data.energy=power;l.data.shape='DISK';l.data.size=size;l.rotation_euler=(Vector((0,0,-30))-l.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.wm.save_as_mainfile(filepath=OUT+'/ShipDocks.blend');scene.render.filepath=OUT+'/DockPreview.png'
if '--render' in __import__('sys').argv:bpy.ops.render.render(write_still=True)
print('SHIP DOCK MODELS EXPORTED',len(parts))
