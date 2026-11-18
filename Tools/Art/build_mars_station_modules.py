"""Dimensioned station dome, independent of every celestial mesh. Run with Blender."""
import bpy
import math
from pathlib import Path
from mathutils import Vector

ROOT=Path('D:/projects/space/SourceArt/MarsStation')
ROOT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene
scene.unit_settings.system='METRIC'
scene.unit_settings.scale_length=1

def material(name,color):
    m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);return m
rib=material('Rib',(.604,.306,.212))
skin=material('ETFE',(.82,.86,.78))
verts=[];faces=[];edges=set()
radius=19.0
# Hexagonal cushions mapped onto the upper ellipsoid. Missing side cells form an 8m breach.
step=2.25
def surface(x,y):
    rr=(x*x+y*y)/(radius*radius)
    return Vector((x,y,18*math.sqrt(max(0,1-rr))))
for row in range(-10,11):
    for col in range(-10,11):
        x=col*step*math.sqrt(3)+(row%2)*step*math.sqrt(3)/2;y=row*step*1.5
        if x*x+y*y>(radius-step*.4)**2:continue
        points=[surface(x+step*math.cos(math.pi/6+i*math.pi/3),y+step*math.sin(math.pi/6+i*math.pi/3)) for i in range(6)]
        if x>radius-5 and abs(y)<4.2:continue
        start=len(verts);verts.extend(points);faces.append(tuple(start+i for i in range(6)))
        for i in range(6):edges.add((start+i,start+(i+1)%6))
mesh=bpy.data.meshes.new('ETFE');mesh.from_pydata(verts,[],faces);mesh.update()
panels=bpy.data.objects.new('SM_Mars_EcologyDome_Cushions',mesh);scene.collection.objects.link(panels);panels.data.materials.append(skin)
parts=[]
for a,b in sorted(edges):
    p,q=Vector(verts[a]),Vector(verts[b]);d=q-p
    if d.length<.01:continue
    bpy.ops.mesh.primitive_cylinder_add(vertices=6,radius=.07,depth=d.length,location=(p+q)/2)
    o=bpy.context.object;o.rotation_mode='QUATERNION';o.rotation_quaternion=d.to_track_quat('Z','Y');parts.append(o)
bpy.ops.object.select_all(action='DESELECT')
for o in parts:o.select_set(True)
bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();ribs=bpy.context.object;ribs.name='SM_Mars_EcologyDome_Ribs';ribs.data.materials.append(rib)
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
for obj in (panels,ribs):
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(obj.name+'.fbx')),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',add_leaf_bones=False,bake_anim=False)
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'MarsStationModules.blend'))
print('MARS_STATION_MODULES_EXPORTED')
