"""Export clean root-compatible FBX and the original reusable FX card."""
import bpy
import json
from pathlib import Path
from mathutils import Quaternion

OUT=Path('D:/projects/space/SourceArt/FrontEnd')
rig=bpy.data.objects['JTS_AvatarRig']
rig.animation_data_clear()
for bone in rig.pose.bones:
    bone.rotation_mode='QUATERNION';bone.rotation_quaternion=Quaternion();bone.location=(0,0,0)
rig.data.pose_position='REST';rig.name='Armature'
if max(abs(s-1) for s in rig.scale)>.001:
    meshes=[bpy.data.objects['SK_FE_'+s] for s in ['Explorer','Scout','Engineer','Survey','Tactical']]
    for mesh in meshes:
        matrix=mesh.matrix_world.copy();mesh.parent=None;mesh.matrix_world=matrix
    bpy.ops.object.select_all(action='DESELECT');rig.hide_set(False);rig.select_set(True);bpy.context.view_layer.objects.active=rig
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    for mesh in meshes:
        mesh.hide_set(False);bpy.ops.object.select_all(action='DESELECT');mesh.select_set(True);bpy.context.view_layer.objects.active=mesh
        bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
        mesh.parent=rig
for style in ['Explorer','Scout','Engineer','Survey','Tactical']:
    mesh=bpy.data.objects['SK_FE_'+style];mesh.hide_set(False)
    for lod,ratio in [(0,1),(1,.55),(2,.26)]:
        obj=mesh
        if lod:
            obj=mesh.copy();obj.data=mesh.data.copy();bpy.context.scene.collection.objects.link(obj)
            obj.hide_set(False)
        bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
        if lod:
            modifier=obj.modifiers.new('LOD reduction','DECIMATE');modifier.ratio=ratio
            bpy.ops.object.modifier_apply(modifier=modifier.name)
        rig.select_set(True)
        filename=mesh.name+('_LOD'+str(lod) if lod else '')+'.fbx'
        bpy.ops.export_scene.fbx(filepath=str(OUT/'Characters'/filename),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z',use_armature_deform_only=False,mesh_smooth_type='FACE')
        if lod:bpy.data.objects.remove(obj,do_unlink=True)
    mesh.hide_set(True)
rig.name='JTS_AvatarRig';rig.data.pose_position='POSE'
if 'SM_FE_FXCard' not in bpy.data.objects:
    bpy.ops.mesh.primitive_plane_add(size=1)
    card=bpy.context.object;card.name='SM_FE_FXCard'
else:card=bpy.data.objects['SM_FE_FXCard'];card.hide_set(False)
bpy.ops.object.select_all(action='DESELECT');card.select_set(True);bpy.context.view_layer.objects.active=card
bpy.ops.export_scene.fbx(filepath=str(OUT/'Models/SM_FE_FXCard.fbx'),use_selection=True,object_types={'MESH'},bake_anim=False,axis_forward='-Y',axis_up='Z')
card.hide_set(True);card.hide_render=True
manifest=json.loads((OUT/'models_manifest.json').read_text(encoding='utf-8'))
manifest=[e for e in manifest if e['id']!='SM_FE_FXCard']
manifest.append({'id':'SM_FE_FXCard','source':'original_blender','file':'Models/SM_FE_FXCard.fbx','triangles':2,'dimensions_m':[1,1,0]})
(OUT/'models_manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'FrontEnd_Models.blend'))
result={'clean_skeletal_exports':15,'fx_card':True}
