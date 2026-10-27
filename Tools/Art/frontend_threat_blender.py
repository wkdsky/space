"""Original suspended alien beam front; authored through the live Blender MCP."""
import bpy
import math
import json
from pathlib import Path
OUT=Path('D:/projects/space/SourceArt/FrontEnd')
collection=bpy.data.collections.get('FE_AlienThreat')
if not collection:
    collection=bpy.data.collections.new('FE_AlienThreat');bpy.context.scene.collection.children.link(collection)
for obj in list(collection.objects):bpy.data.objects.remove(obj,do_unlink=True)
assets=[]

def move(obj):
    for c in list(obj.users_collection):c.objects.unlink(obj)
    collection.objects.link(obj)
    return obj

def cone(name,r1,r2,depth,z):
    bpy.ops.mesh.primitive_cone_add(vertices=64,radius1=r1,radius2=r2,depth=depth,location=(0,0,z))
    obj=move(bpy.context.object);obj.name=name
    return obj

def export(name,objects):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.hide_set(False);o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    bpy.ops.object.join();o=bpy.context.object;o.name=name
    bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    bpy.ops.export_scene.fbx(filepath=str(OUT/'Models'/(name+'.fbx')),use_selection=True,object_types={'MESH'},bake_anim=False,axis_forward='-Y',axis_up='Z')
    o.data.calc_loop_triangles()
    assets.append({'id':name,'file':'Models/'+name+'.fbx','source':'original_blender','triangles':len(o.data.loop_triangles),'purpose':'High altitude alien laser front, suspended before ground impact; no screenshot geometry'})
    o.hide_render=True;o.hide_set(True)

export('SM_FE_ThreatCore',[cone('Core',.42,.42,60,33),cone('Front',.018,.42,3,1.5)])
export('SM_FE_ThreatHalo',[cone('Halo',.72,.72,60,33),cone('HaloFront',.025,.72,3,1.5)])
rings=[]
for z,r in [(2.8,.95),(3.4,.80),(4.0,.70)]:
    bpy.ops.mesh.primitive_torus_add(major_radius=r,minor_radius=.022,major_segments=64,minor_segments=8,location=(0,0,z))
    rings.append(move(bpy.context.object))
export('SM_FE_ThreatFrontRings',rings)
bpy.data.libraries.write(str(OUT/'FrontEnd_Threat.blend'),{collection},fake_user=True)
(OUT/'threat_manifest.json').write_text(json.dumps(assets,indent=2),encoding='utf-8')
result={'assets':len(assets),'source':'original','blend':'FrontEnd_Threat.blend'}
