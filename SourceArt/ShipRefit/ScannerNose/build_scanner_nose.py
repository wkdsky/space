import bpy, math, os
from mathutils import Vector
OUT='D:/projects/space/SourceArt/ShipRefit/ScannerNose'
os.makedirs(OUT,exist_ok=True)
bpy.ops.object.select_all(action='DESELECT')
parts=[]
colors={'Ceramic':(.43,.48,.47),'Frame':(.022,.036,.046),'Metal':(.16,.22,.25),'Teal':(.025,.29,.28),'Glow':(.035,.85,.68)}
mats={}
for n,c in colors.items():
 m=bpy.data.materials.new('Refit_'+n);m.diffuse_color=(*c,1);mats[n]=m

def finish(o,n,mat):
 o.name=n;o.data.materials.append(mats[mat]);parts.append(o);return o

def box(n,loc,dim,mat):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.dimensions=dim;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 b=o.modifiers.new('Machined edges','BEVEL');b.width=1.6;b.segments=3;bpy.ops.object.modifier_apply(modifier=b.name)
 return finish(o,n,mat)

def tube(n,x0,x1,r0,r1,mat):
 v=[];f=[];N=64
 for x,r in [(x0,r0),(x1,r0),(x0,r1),(x1,r1)]:
  v.extend((x,r*math.cos(i*math.tau/N),r*math.sin(i*math.tau/N)) for i in range(N))
 for i in range(N):
  j=(i+1)%N
  f.extend([(i,j,N+j,N+i),(2*N+i,3*N+i,3*N+j,2*N+j),(N+i,N+j,3*N+j,3*N+i),(i,2*N+i,2*N+j,j)])
 m=bpy.data.meshes.new(n);m.from_pydata(v,[],f);m.update();o=bpy.data.objects.new(n,m);bpy.context.collection.objects.link(o);return finish(o,n,mat)

def save(n):
 global parts
 bpy.ops.object.select_all(action='DESELECT')
 for o in parts:o.select_set(True)
 bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();o=bpy.context.object;o.name=n;bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
 bpy.context.scene.unit_settings.scale_length=.01
 bpy.ops.export_scene.fbx(filepath=OUT+'/'+n+'.fbx',use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False)
 parts=[];o.hide_set(True)
# Armored extension covers the existing solid nose, with a real open cavity ahead of it.
for z in [-32,32]:box('Nose armor lip',(34,0,z),(68,126,8),'Ceramic')
for y in [-59,59]:box('Nose armor cheek',(34,y,0),(68,8,58),'Ceramic')
plate=box('Recess face',(64,0,0),(8,112,58),'Frame')
bpy.ops.mesh.primitive_cylinder_add(vertices=64,radius=27,depth=22,location=(64,0,0),rotation=(0,math.pi/2,0));cut=bpy.context.object
bpy.context.view_layer.objects.active=plate;mod=plate.modifiers.new('Open bore','BOOLEAN');mod.operation='DIFFERENCE';mod.object=cut;bpy.ops.object.modifier_apply(modifier=mod.name);bpy.data.objects.remove(cut,do_unlink=True)
box('Dark rear cavity',(10,0,0),(4,110,56),'Frame');tube('Inner socket liner',16,66,27,26,'Metal')
for y in [-49,49]:box('Navigation index',(68,y,0),(1.5,4,16),'Glow')
save('SM_Nose_RecessHousing')
tube('Gimballed barrel',-10,27,17,12,'Metal');tube('Muzzle rim',24,30,18,12,'Frame');tube('Emitter ring',28,29,13,11.5,'Glow')
box('Bore darkness',(-8,0,0),(2,21,21),'Frame');save('SM_Nose_RecessBarrel')
# Scanner camera: wide line projector, separate from the screen assembly.
box('Camera case',(0,0,0),(52,23,16),'Frame');box('Camera armor',(0,-2,8),(56,25,3),'Ceramic')
box('Projector slit',(0,12,0),(40,2,5),'Glow')
for x in [-24,24]:box('Bracket',(x,-6,-7),(5,12,10),'Metal')
save('SM_Scanner_LineCamera')
bpy.ops.wm.save_as_mainfile(filepath=OUT+'/ScannerNose.blend')

