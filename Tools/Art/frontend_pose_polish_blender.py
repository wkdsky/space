"""Original relaxed idle: planted feet, soft knees, asymmetric arms and curled gloves."""
import bpy
import json
import math
from pathlib import Path
from mathutils import Vector, Quaternion, Matrix

OUT=Path('D:/projects/space/SourceArt/FrontEnd')
assert Path(bpy.data.filepath)==OUT/'FrontEnd_Models.blend'
scene=bpy.context.scene
rig=bpy.data.objects['JTS_AvatarRig']
rig.hide_set(False)
rig.data.pose_position='POSE'
rig.animation_data_clear()
rest={b.name:rig.matrix_world@b.head_local for b in rig.data.bones}

def rotate(name,axis,degrees,compose=False):
    bone=rig.pose.bones[name]
    orientation=(rig.matrix_world@rig.data.bones[name].matrix_local).to_quaternion()
    bone.rotation_mode='QUATERNION'
    rotation=Quaternion(orientation.inverted()@Vector(axis),math.radians(degrees))
    bone.rotation_quaternion=rotation@bone.rotation_quaternion if compose else rotation

def move(name,target):
    bpy.context.view_layer.update()
    matrix=rig.matrix_world@rig.pose.bones[name].matrix
    matrix.translation=target
    rig.pose.bones[name].matrix=rig.matrix_world.inverted()@matrix
    bpy.context.view_layer.update()

def aim(name,target,rest_point=None):
    bpy.context.view_layer.update()
    bone=rig.pose.bones[name]
    matrix=rig.matrix_world@bone.matrix
    location,rotation,scale=matrix.decompose()
    if rest_point is None:
        point=rig.matrix_world@bone.tail
    else:
        local=rig.data.bones[name].matrix_local.inverted()@rest_point
        point=rig.matrix_world@bone.matrix@local
    rotation=(point-location).rotation_difference(Vector(target)-location)@rotation
    bone.matrix=rig.matrix_world.inverted()@Matrix.LocRotScale(location,rotation,scale)
    bpy.context.view_layer.update()

def pose(t):
    phase=t*math.tau
    for b in rig.pose.bones:
        b.rotation_mode='QUATERNION';b.rotation_quaternion=Quaternion()
        b.location=(0,0,0);b.scale=(1,1,1)
    bpy.context.view_layer.update()
    move('Body',rest['Body']+Vector((.012+.008*math.sin(phase),.002,-.018+.002*math.sin(phase*2))))
    rotate('Hips',(0,1,0),.7+math.sin(phase)*.5)
    rotate('Abdomen',(0,1,0),-.45-math.sin(phase)*.35)
    rotate('Torso',(1,0,0),-.8+math.sin(phase*3)*.65)
    rotate('Chest',(0,1,0),math.sin(phase*2)*.45)
    rotate('Head',(0,0,1),-3+math.sin(phase)*2.2)
    rotate('Head',(1,0,0),math.sin(phase*2)*.6,compose=True)
    bpy.context.view_layer.update()
    for side,sign in [('L',1),('R',-1)]:
        # Solve against the garment's ankle rather than the source rig's long IK tail.
        hip=rig.matrix_world@rig.pose.bones['UpperLeg_'+side].head
        ankle_rest=Vector((sign*.121,-.062,.105))
        foot_offset=Vector((sign*.025,-.045 if side=='L' else .037,0))
        ankle=ankle_rest+foot_offset
        upper=(rest['LowerLeg_'+side]-rest['UpperLeg_'+side]).length
        lower=(ankle_rest-rest['LowerLeg_'+side]).length
        direction=(ankle-hip).normalized();distance=(ankle-hip).length
        along=(upper*upper-lower*lower+distance*distance)/(2*distance)
        bend=max(0,upper*upper-along*along)**.5
        pole=Vector((0,-1,0));pole=(pole-direction*pole.dot(direction)).normalized()
        knee=hip+direction*along+pole*bend
        aim('UpperLeg_'+side,knee)
        aim('LowerLeg_'+side,ankle,ankle_rest)
        move('Foot_'+side,rest['Foot_'+side]+foot_offset)
        rotate('Foot_'+side,(0,0,1),sign*5)
        # Elbows remain soft and slightly in front of the body; wrists hang naturally.
        shoulder=rig.matrix_world@rig.pose.bones['UpperArm_'+side].head
        sway=math.sin(phase+(0 if side=='L' else .7))
        elbow=shoulder+Vector((sign*.065,-.016+.004*sway,-.169))
        aim('UpperArm_'+side,elbow)
        elbow=rig.matrix_world@rig.pose.bones['LowerArm_'+side].head
        wrist=elbow+Vector((sign*.026,(-.115 if side=='L' else -.052)+.008*sway,-.205 if side=='L' else -.228))
        aim('LowerArm_'+side,wrist)
        for finger,amount in [('Index',20),('Middle',25),('Ring',31),('Pinky',36)]:
            rotate(finger+'2_'+side,(0,1,0),sign*(amount+math.sin(phase*2)*1.2))
            rotate(finger+'3_'+side,(0,1,0),sign*amount*.7)
        rotate('Thumb1_'+side,(0,1,0),sign*10)
        rotate('Thumb2_'+side,(0,1,0),sign*18)
    bpy.context.view_layer.update()

action=bpy.data.actions.new('A_FE_Idle_Relaxed')
rig.animation_data_create();rig.animation_data.action=action
scene.render.fps=30;scene.frame_start=1;scene.frame_end=361
samples=[]
for frame in range(1,362,3):
    # Author an unconstrained pose; an active action would re-evaluate frame 1
    # during IK dependency updates and erase newly calculated hip translations.
    rig.animation_data.action=None
    scene.frame_set(frame)
    pose((frame-1)/360)
    rig.animation_data.action=action
    for bone in rig.pose.bones:
        bone.keyframe_insert(data_path='rotation_quaternion',frame=frame,group=bone.name)
        bone.keyframe_insert(data_path='location',frame=frame,group=bone.name)
    if (frame-1)%30==0:
        samples.append({'frame':frame,'feet':[list(rig.matrix_world@rig.pose.bones['Foot_'+s].head) for s in ['L','R']],
                        'wrists':[list(rig.matrix_world@rig.pose.bones['Wrist_'+s].head) for s in ['L','R']]})
# Export a centimetre rig with no object-scale compensation. The source blend remains metric.
export_rig=rig.copy();export_rig.data=rig.data.copy();scene.collection.objects.link(export_rig)
export_rig.animation_data_clear();export_rig.data.transform(Matrix.Scale(100,4))
export_rig.name='Armature';export_rig.animation_data_create()
export_rig.animation_data.action=bpy.data.actions.new('A_FE_Idle_Centimetres')
for frame in range(1,362,3):
    scene.frame_set(frame);bpy.context.view_layer.update()
    for bone in export_rig.pose.bones:
        matrix=rig.pose.bones[bone.name].matrix_basis.copy();matrix.translation*=100
        bone.rotation_mode='QUATERNION';bone.matrix_basis=matrix
        bone.keyframe_insert(data_path='rotation_quaternion',frame=frame,group=bone.name)
        bone.keyframe_insert(data_path='location',frame=frame,group=bone.name)
        bone.keyframe_insert(data_path='scale',frame=frame,group=bone.name)
bpy.ops.object.select_all(action='DESELECT');export_rig.select_set(True);bpy.context.view_layer.objects.active=export_rig
original_units=scene.unit_settings.scale_length;scene.unit_settings.scale_length=.01
bpy.ops.export_scene.fbx(filepath=str(OUT/'Animations/A_FE_Idle.fbx'),use_selection=True,object_types={'ARMATURE'},
    add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,
    bake_anim_simplify_factor=0,axis_forward='-Y',axis_up='Z')
scene.unit_settings.scale_length=original_units
bpy.data.objects.remove(export_rig,do_unlink=True)
manifest=json.loads((OUT/'animations_manifest.json').read_text(encoding='utf-8'))
for asset in manifest:
    if asset['id']=='A_FE_Idle':asset.update(seconds=12,source='original_blender_planted_relaxed_idle',
        description='Soft knee IK, planted independent feet, weight shift, asymmetric elbow flex, curled fingers, breathing and head motion')
(OUT/'animations_manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'FrontEnd_Models.blend'))
(OUT/'relaxed_idle_pose_report.json').write_text(json.dumps({'samples':samples,'seconds':12,'source':'original'},indent=2),encoding='utf-8')
result={'animation':'A_FE_Idle','seconds':12,'samples':len(samples),'feet_planted':True}
