"""Independent station shell collision with exact airlock and quarantined-breach apertures."""
import bpy,bmesh,math,json
from pathlib import Path
from mathutils import Vector
out=Path('D:/projects/space/SourceArt/MarsStation/Refined')
manifest=json.loads((out/'manifest.json').read_text(encoding='utf8'))
ribs=bpy.data.objects.get('SM_MP_Dome_Ribs') or bpy.data.objects['SM_MP_Flow_DomeRibs'];ribs.name='SM_MP_Flow_DomeRibs'
glazing=bpy.data.objects.get('SM_MP_Dome_Glazing') or bpy.data.objects['SM_MP_Flow_DomeGlazing'];glazing.name='SM_MP_Flow_DomeGlazing'
# UE's FBX coordinate conversion reflects Y. The station opening is on UE -Y.
for part in (ribs,glazing):
 if not part.get('station_handedness_fixed',False):
  for v in part.data.vertices:v.co.y=-v.co.y
  bm=bmesh.new();bm.from_mesh(part.data);bmesh.ops.reverse_faces(bm,faces=list(bm.faces));bm.to_mesh(part.data);bm.free();part.data.update();part['station_handedness_fixed']=True
bpy.ops.object.select_all(action='DESELECT');ribs.hide_set(False);ribs.select_set(True);bpy.context.view_layer.objects.active=ribs
bpy.ops.export_scene.fbx(filepath=str(out/(ribs.name+'.fbx')),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False)
manifest['Flow_DomeRibs']={'mesh':ribs.name,'triangles':sum(len(p.vertices)-2 for p in ribs.data.polygons),'collision':'complex'};ribs.hide_set(True)
bpy.ops.object.select_all(action='DESELECT');glazing.hide_set(False);glazing.select_set(True);bpy.context.view_layer.objects.active=glazing
bpy.ops.export_scene.fbx(filepath=str(out/(glazing.name+'.fbx')),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False)
manifest['Flow_DomeGlazing']={'mesh':glazing.name,'triangles':sum(len(p.vertices)-2 for p in glazing.data.polygons),'collision':'none'};glazing.hide_set(True)
def split(poly,axis,bound,sign):
 inside=[];outside=[]
 for a,b in zip(poly,poly[1:]+poly[:1]):
  da=(a[axis]-bound)*sign;db=(b[axis]-bound)*sign
  (inside if da<=1e-7 else outside).append(a)
  if (da<0<db) or (db<0<da):
   v=a+(b-a)*(da/(da-db));inside.append(v);outside.append(v)
 return inside,outside
def subtract(poly,box):
 remaining=poly;pieces=[]
 for axis in range(3):
  for bound,sign in ((box[0][axis],-1),(box[1][axis],1)):
   if len(remaining)<3:return pieces
   remaining,side=split(remaining,axis,bound,sign)
   if len(side)>=3:pieces.append(side)
 return pieces
verts=[];faces=[];n=96;levels=24
def point(i,j):
 a=i*math.tau/n;b=j*(math.pi/2)/levels
 return Vector((19*math.cos(a)*math.cos(b),19*math.sin(a)*math.cos(b),16.5*math.sin(b)))
for j in range(levels):
 for i in range(n):
  p=[point(i,j),point(i+1,j),point(i+1,j+1),point(i,j+1)]
  pieces=subtract(p,((-2.55,16,-1),(2.55,100,4.1)))
  pieces=[fragment for poly in pieces for fragment in subtract(poly,((16,-4.2,-1),(100,4.2,5.6)))]
  for poly in pieces:
   start=len(verts);verts.extend(poly);faces.append(tuple(start+k for k in range(len(poly))))
old=bpy.data.objects.get('SM_MP_Flow_DomeSeal')
if old:bpy.data.objects.remove(old,do_unlink=True)
mesh=bpy.data.meshes.new('Sealed shell');mesh.from_pydata(verts,[],faces);mesh.update();o=bpy.data.objects.new('SM_MP_Flow_DomeSeal',mesh);bpy.context.collection.objects.link(o);mesh.materials.append(bpy.data.materials['MP_Palette'])
bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
bpy.ops.export_scene.fbx(filepath=str(out/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False)
manifest['Flow_DomeSeal']={'mesh':o.name,'triangles':sum(len(p.vertices)-2 for p in mesh.polygons),'collision':'complex'}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8');o.hide_set(True);bpy.ops.wm.save_as_mainfile(filepath=str(out/'Mars_Chapter_Refined.blend'));print({'seal_triangles':manifest['Flow_DomeSeal']['triangles']})
