"""Render model contact sheets in Blender, without changing production mesh data."""
import bpy
import math
from pathlib import Path
from mathutils import Vector

ROOT=Path('D:/projects/space/Saved/MoonResources')
ROOT.mkdir(parents=True,exist_ok=True)
source=bpy.data.scenes['JTS_MoonResources']
scene=bpy.data.scenes.get('JTS_MoonResources_QA') or bpy.data.scenes.new('JTS_MoonResources_QA')
bpy.context.window.scene=scene
for o in list(scene.objects):bpy.data.objects.remove(o,do_unlink=True)
scene.render.engine='CYCLES';scene.cycles.samples=24
scene.render.resolution_x=1800;scene.render.resolution_y=1100;scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('MoonResourceQAWorld');scene.world.use_nodes=True
background=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
background.inputs['Color'].default_value=(.19,.21,.24,1)
background.inputs['Strength'].default_value=.4
scene.view_settings.view_transform='AgX'

def light(name,location,power,size):
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size
    o=bpy.data.objects.new(name,data);scene.collection.objects.link(o);o.location=location
    o.rotation_euler=(Vector((6,4,0))-o.location).to_track_quat('-Z','Y').to_euler()
light('Key',(0,-2,12),2200,6);light('Fill',(10,6,9),1300,8)
camera=bpy.data.objects.new('Camera',bpy.data.cameras.new('Camera'));scene.collection.objects.link(camera);scene.camera=camera
camera.data.type='ORTHO';camera.data.ortho_scale=18.5
camera.location=(8,-11,13);camera.rotation_euler=(Vector((6.25,3,0))-camera.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,0));plane=bpy.context.object
plane.data.materials.append(bpy.data.materials['M_MR_Regolith'])

def text(label,x,y):
    data=bpy.data.curves.new(label,'FONT');data.body=label;data.size=.23;data.extrude=0
    o=bpy.data.objects.new(label,data);scene.collection.objects.link(o);o.location=(x,y,.01)

objects=[]
for row,kind in enumerate(('Rock','Metal','Crystal')):
    text(kind.upper(),-1.2,row*3-.7)
    for i in range(6):
        original=source.objects[f'SM_MR_{kind}_Medium_{i+1:02d}']
        o=original.copy();o.data=original.data;scene.collection.objects.link(o);o.location=(i*2.5,row*3,0);objects.append(o)
        o.rotation_euler.z=.3*(i%3)
        o.scale=(1.4,)*3
scene.render.filepath=str(ROOT/'Rocks_ContactSheet.png');bpy.ops.render.render(write_still=True)
for o in objects:bpy.data.objects.remove(o,do_unlink=True)
for o in list(scene.objects):
    if o.type=='FONT':bpy.data.objects.remove(o,do_unlink=True)
camera.data.ortho_scale=6.4
camera.location=(2.5,-4.5,3.4);camera.rotation_euler=(Vector((2.1,0,0))-camera.location).to_track_quat('-Z','Y').to_euler()
for i in range(3):
    original=source.objects[f'SM_MR_Carbon_Accretion_{i+1:02d}']
    o=original.copy();o.data=original.data;scene.collection.objects.link(o);o.location=(i*2.1,0,0)
scene.render.filepath=str(ROOT/'Carbon_ContactSheet.png');bpy.ops.render.render(write_still=True)
bpy.context.window.scene=source
print('Moon resource contact sheets rendered')
