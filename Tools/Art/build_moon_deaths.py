"""Rebake four distinct Moon deaths; preserve the skeleton and every other clip."""
import bpy,json,sys
from pathlib import Path
from mathutils import Matrix,Vector
ROOT=Path('D:/projects/space');sys.path.insert(0,str(ROOT/'Tools/Art'))
import moon_creatures_motion as motion
import moon_creatures_delivery as delivery
from moon_creatures_geometry import KINDS,bind_riftlip_tail
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'SourceArt/MoonCreaturesV3/MoonCreaturesV3.blend'))
OUT=ROOT/'Saved/MoonCombatRevision';OUT.mkdir(parents=True,exist_ok=True)
bind_riftlip_tail(bpy.data.objects['SK_Riftlip'])
summaries=[]
for kind in KINDS:
    scene,rig=delivery.show(kind,'Death');spec=json.loads(rig['motion_spec'])
    action=bpy.data.actions['A_'+kind+'_Death'];count=motion.DEATH_FRAMES[kind]
    for layer in list(action.layers):action.layers.remove(layer)
    action.frame_start=1;action.frame_end=count;scene.frame_start=1;scene.frame_end=count
    samples=[];mesh=bpy.data.objects['SK_'+kind]
    for frame in range(1,count+1):
        scene.frame_set(frame);t=(frame-1)/(count-1)
        motion.pose(rig,spec,motion.params(kind,'Death',t,spec),t)
        for bone in rig.pose.bones:
            bone.keyframe_insert('location',frame=frame,group=bone.name)
            bone.keyframe_insert('rotation_quaternion',frame=frame,group=bone.name)
        bpy.context.view_layer.update()
        points=[v.co for v in mesh.evaluated_get(bpy.context.evaluated_depsgraph_get()).data.vertices]
        samples.append(dict(frame=frame,min_z=min(v.z for v in points),max_z=max(v.z for v in points),body_z=rig.pose.bones['body'].matrix.translation.z))
    for curve in delivery.curves(action):
        for key in curve.keyframe_points:key.interpolation='LINEAR'
    clips=json.loads(rig['clips'])
    for clip in clips:
        if clip['name']=='Death':clip['frames']=count;clip['grounded_support']=True;action['clip_info']=json.dumps(clip)
    rig['clips']=json.dumps(clips)
    for track in rig.animation_data.nla_tracks:
        if track.name=='Death':
            for strip in track.strips:strip.action_frame_end=count
    # The same baked-centimetre convention as the existing animation delivery.
    copy=rig.copy();copy.data=rig.data.copy();copy.animation_data_clear();scene.collection.objects.link(copy)
    copy.name='Armature';copy.data.transform(Matrix.Scale(100,4));scaled=action.copy()
    for curve in delivery.curves(scaled):
        if curve.data_path.endswith('.location'):
            for key in curve.keyframe_points:key.co.y*=100;key.handle_left.y*=100;key.handle_right.y*=100
    copy.animation_data_create();copy.animation_data.action=scaled;copy.animation_data.action_slot=scaled.slots[0]
    for bone in copy.pose.bones:bone.location=(0,0,0);bone.rotation_quaternion=(1,0,0,0);bone.scale=(1,1,1)
    units=scene.unit_settings.scale_length;scene.unit_settings.scale_length=.01
    bpy.ops.object.select_all(action='DESELECT');copy.select_set(True);bpy.context.view_layer.objects.active=copy;scene.frame_set(1)
    folder=ROOT/'SourceArt/MoonCreaturesV3'/kind
    options=dict(use_selection=True,add_leaf_bones=False,use_armature_deform_only=True,axis_forward='-Y',axis_up='Z',
        global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS')
    bpy.ops.export_scene.fbx(filepath=str(folder/('A_'+kind+'_Death.fbx')),object_types={'ARMATURE'},bake_anim=True,
        bake_anim_use_all_bones=True,bake_anim_use_nla_strips=False,bake_anim_use_all_actions=False,
        bake_anim_force_startend_keying=True,bake_anim_step=1,bake_anim_simplify_factor=0,**options)
    if kind=='Riftlip':
        copy.animation_data_clear()
        for bone in copy.pose.bones:bone.location=(0,0,0);bone.rotation_quaternion=(1,0,0,0);bone.scale=(1,1,1)
        duplicate=mesh.copy();duplicate.data=mesh.data.copy();scene.collection.objects.link(duplicate)
        duplicate.data.transform(Matrix.Scale(100,4));duplicate.parent=copy
        for modifier in duplicate.modifiers:
            if modifier.type=='ARMATURE':modifier.object=copy
        duplicate.select_set(True)
        bpy.ops.export_scene.fbx(filepath=str(folder/'SK_Riftlip.fbx'),object_types={'MESH','ARMATURE'},bake_anim=False,
            use_mesh_modifiers=True,mesh_smooth_type='FACE',path_mode='AUTO',**options)
        data=duplicate.data;bpy.data.objects.remove(duplicate,do_unlink=True);bpy.data.meshes.remove(data)
    data=copy.data;bpy.data.objects.remove(copy,do_unlink=True);bpy.data.armatures.remove(data);bpy.data.actions.remove(scaled)
    scene.unit_settings.scale_length=units
    summary=dict(kind=kind,frames=count,seconds=(count-1)/30,samples=samples)
    assert min(s['min_z'] for s in samples)>.0038
    if kind=='SpireSoldier':assert samples[-1]['body_z']<samples[0]['body_z']-.02
    summaries.append(summary)
delivery.show('MirrorSkimmer','Idle',1);bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'SourceArt/MoonCreaturesV3/MoonCreaturesV3.blend'),compress=True)
manifest_path=ROOT/'SourceArt/MoonCreaturesV3/manifest.json'
manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
for species in manifest['creatures']:
    for clip in species['clips']:
        if clip['name']=='Death':
            clip['frames']=motion.DEATH_FRAMES[species['species']];clip['vertical_root_motion']='Baked grounded support and species-specific impulse'
manifest_path.write_text(json.dumps(manifest,indent=2),encoding='utf-8')
(OUT/'death_support.json').write_text(json.dumps(summaries,indent=2),encoding='utf-8')
delivery.stage();scene.camera.location=(.9,-1.25,.92)
scene.camera.rotation_euler=(Vector((0,0,.20))-scene.camera.location).to_track_quat('-Z','Y').to_euler()
scene.camera.data.type='ORTHO';scene.camera.data.ortho_scale=1.12
scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO';scene.display.shading.color_type='MATERIAL'
scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True
scene.render.resolution_x=640;scene.render.resolution_y=480
for kind in KINDS:
    folder=OUT/kind;folder.mkdir(exist_ok=True)
    for frame in range(1,motion.DEATH_FRAMES[kind]+1):
        delivery.show(kind,'Death',frame);scene.render.filepath=str(folder/('%03d.png'%frame))
        bpy.ops.render.render(write_still=True)
print('MOON_DEATHS '+json.dumps([dict(kind=s['kind'],seconds=s['seconds'],peak_ground_clearance=max(r['min_z'] for r in s['samples'])) for s in summaries]))
