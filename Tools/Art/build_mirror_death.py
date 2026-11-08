"""Rebake only MirrorSkimmer's grounded death clip in the existing Blender source."""
import bpy,json,sys,math
from pathlib import Path
from mathutils import Matrix,Vector
ROOT=Path('D:/projects/space')
sys.path.insert(0,str(ROOT/'Tools/Art'))
import moon_creatures_motion as motion
import moon_creatures_delivery as delivery
source=ROOT/'SourceArt/MoonCreaturesV3/MoonCreaturesV3.blend'
bpy.ops.wm.open_mainfile(filepath=str(source))
scene,rig=delivery.show('MirrorSkimmer','Death')
spec=json.loads(rig['motion_spec']);action=bpy.data.actions['A_MirrorSkimmer_Death']
for layer in list(action.layers):action.layers.remove(layer)
count=motion.DEATH_FRAMES['MirrorSkimmer']
action.frame_start=1;action.frame_end=count
scene.frame_start=1;scene.frame_end=count
mesh=bpy.data.objects['SK_MirrorSkimmer']
samples=[]
for frame in range(1,count+1):
    scene.frame_set(frame)
    t=(frame-1)/(count-1)
    motion.pose(rig,spec,motion.params('MirrorSkimmer','Death',t,spec),t)
    for bone in rig.pose.bones:
        bone.keyframe_insert('location',frame=frame,group=bone.name)
        bone.keyframe_insert('rotation_quaternion',frame=frame,group=bone.name)
    bpy.context.view_layer.update()
    evaluated=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
    points=[v.co for v in evaluated.data.vertices]
    samples.append(dict(frame=frame,min_z=min(p.z for p in points),max_z=max(p.z for p in points),width=max(p.y for p in points)-min(p.y for p in points)))
for curve in delivery.curves(action):
    for key in curve.keyframe_points:key.interpolation='LINEAR'
clips=json.loads(rig['clips'])
for clip in clips:
    if clip['name']=='Death':
        clip['frames']=count;clip['grounded_support']=True
        clip['behavior']='Impulse hop, flip onto back, curl legs, impact and held corpse pose.'
        action['clip_info']=json.dumps(clip)
rig['clips']=json.dumps(clips)
for track in rig.animation_data.nla_tracks:
    if track.name=='Death':
        for strip in track.strips:strip.action_frame_end=count

# Match the project's animation-only FBX unit convention without re-exporting other assets.
copy=rig.copy();copy.data=rig.data.copy();copy.animation_data_clear();scene.collection.objects.link(copy)
copy.name='Armature';copy.data.transform(Matrix.Scale(100,4))
scaled=action.copy()
for curve in delivery.curves(scaled):
    if curve.data_path.endswith('.location'):
        for key in curve.keyframe_points:
            key.co.y*=100;key.handle_left.y*=100;key.handle_right.y*=100
copy.animation_data_create();copy.animation_data.action=scaled;copy.animation_data.action_slot=scaled.slots[0]
for bone in copy.pose.bones:bone.location=(0,0,0);bone.rotation_quaternion=(1,0,0,0);bone.scale=(1,1,1)
units=scene.unit_settings.scale_length;scene.unit_settings.scale_length=.01
bpy.ops.object.select_all(action='DESELECT');copy.select_set(True);bpy.context.view_layer.objects.active=copy
scene.frame_set(1)
output=ROOT/'SourceArt/MoonCreaturesV3/MirrorSkimmer/A_MirrorSkimmer_Death.fbx'
bpy.ops.export_scene.fbx(filepath=str(output),use_selection=True,add_leaf_bones=False,use_armature_deform_only=True,
    axis_forward='-Y',axis_up='Z',global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',
    object_types={'ARMATURE'},bake_anim=True,bake_anim_use_all_bones=True,bake_anim_use_nla_strips=False,
    bake_anim_use_all_actions=False,bake_anim_force_startend_keying=True,bake_anim_step=1,bake_anim_simplify_factor=0)
data=copy.data;bpy.data.objects.remove(copy,do_unlink=True);bpy.data.armatures.remove(data);bpy.data.actions.remove(scaled)
scene.unit_settings.scale_length=units
delivery.show('MirrorSkimmer','Idle',1)
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(source),compress=True)

out=ROOT/'Saved/MirrorContactQA';out.mkdir(parents=True,exist_ok=True)
(out/'death_mesh_support.json').write_text(json.dumps(samples,indent=2),encoding='utf-8')
delivery.stage()
scene.camera.location=(.9,-1.25,.92)
scene.camera.rotation_euler=(Vector((0,0,.13))-scene.camera.location).to_track_quat('-Z','Y').to_euler()
scene.camera.data.type='ORTHO';scene.camera.data.ortho_scale=.88
scene.render.engine='BLENDER_WORKBENCH'
scene.display.shading.light='STUDIO';scene.display.shading.color_type='MATERIAL'
scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True
scene.render.resolution_x=640;scene.render.resolution_y=480
for frame in (1,8,15,23,count):
    delivery.show('MirrorSkimmer','Death',frame)
    scene.render.filepath=str(out/('death_%02d.png'%frame))
    bpy.ops.render.render(write_still=True)
print('MIRROR_DEATH_QA '+json.dumps(dict(frames=len(samples),lowest=min(s['min_z'] for s in samples),highest=max(s['max_z'] for s in samples),final=samples[-1])))
