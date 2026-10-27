"""Original menu actions, facial materials, suit thumbnails and movement pose QA."""
import bpy
import json
import math
from pathlib import Path
from mathutils import Vector, Quaternion, Matrix

OUT=Path('D:/projects/space/SourceArt/FrontEnd')
RENDER_PREVIEWS=globals().get('RENDER_PREVIEWS',True)
if Path(bpy.data.filepath) != OUT/'FrontEnd_Models.blend':
    raise RuntimeError('Open FrontEnd_Models.blend in a separate MCP call before running animation authoring')
scene=bpy.context.scene
rig=bpy.data.objects['JTS_AvatarRig']
characters=[bpy.data.objects['SK_FE_'+n] for n in ['Explorer','Scout','Engineer','Survey','Tactical']]
for obj in characters:obj.hide_render=True;obj.hide_set(True)
for name,index in [('Eye',0),('Mouth',1)]:
    mat=bpy.data.materials['FE_'+name]
    node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(OUT/'UI'/('T_FE_'+name+str(index)+'.png')),check_existing=True)
    tex.image.reload()
    mat.node_tree.links.new(tex.outputs['Alpha'],node.inputs['Alpha'])
    mat.surface_render_method='DITHERED'


def rotate(name,world_axis,degrees):
    bone=rig.pose.bones[name]
    orientation=(rig.matrix_world@rig.data.bones[name].matrix_local).to_quaternion()
    bone.rotation_mode='QUATERNION'
    bone.rotation_quaternion=Quaternion(orientation.inverted()@Vector(world_axis),math.radians(degrees))


def aim(name,point):
    """Aim a posed bone in world space, preserving its inherited scale and rest roll."""
    bpy.context.view_layer.update()
    bone=rig.pose.bones[name]
    matrix=rig.matrix_world@bone.matrix
    location,rotation,scale=matrix.decompose()
    direction=(rig.matrix_world.to_3x3()@(bone.tail-bone.head)).normalized()
    rotation=direction.rotation_difference(Vector(point)-location)@rotation
    bone.matrix=rig.matrix_world.inverted()@Matrix.LocRotScale(location,rotation,scale)
    bpy.context.view_layer.update()


def reach(side,elbow,hand):
    aim('UpperArm_'+side,elbow)
    aim('LowerArm_'+side,hand)


def pose(kind,t):
    for bone in rig.pose.bones:
        bone.rotation_mode='QUATERNION';bone.rotation_quaternion=Quaternion();bone.location=(0,0,0);bone.scale=(1,1,1)
    for side,sign in [('L',1),('R',-1)]:
        rotate('UpperArm_'+side,(0,1,0),sign*74)
        rotate('LowerArm_'+side,(0,0,1),-sign*5)
    breath=math.sin(t*math.tau)
    rotate('Torso',(1,0,0),breath*.65)
    if kind=='LookShip':rotate('Head',(0,0,1),math.sin(math.pi*t)*28)
    if kind in ['CheckWrist','AdjustGlove']:
        weight=math.sin(math.pi*t)**2
        rotate('UpperArm_L',(0,1,0),74-weight*45)
        rotate('LowerArm_L',(0,0,1),-weight*100)
        rotate('Head',(1,0,0),weight*16)
        if kind=='AdjustGlove':
            rotate('UpperArm_R',(0,1,0),-74+weight*50)
            rotate('LowerArm_R',(0,0,1),weight*95)
    if kind=='Join':
        weight=math.sin(math.pi*t)
        rotate('UpperArm_R',(0,1,0),-74+weight*130)
        rotate('LowerArm_R',(0,0,1),weight*(15+math.sin(t*math.pi*6)*13))
    if kind=='Ready':
        weight=min(1,t*3)
        rotate('Head',(1,0,0),math.sin(math.pi*t)*12)
        rotate('UpperArm_L',(0,1,0),74-weight*15)
        rotate('LowerArm_L',(0,0,1),-weight*55)
    if kind=='Unready':rotate('Head',(0,0,1),math.sin(t*math.pi*4)*9)
    if kind=='Leave':
        rotate('Torso',(0,0,1),t*18)
        rotate('Head',(0,0,1),-t*18)
    if kind=='RifleQA':
        reach('L',(.26,-.13,1.28),(.08,-.48,1.30))
        reach('R',(-.24,-.10,1.24),(-.08,-.39,1.30))
    if kind=='RunQA':
        rotate('UpperLeg_L',(1,0,0),-40);rotate('LowerLeg_L',(1,0,0),65)
        rotate('UpperLeg_R',(1,0,0),35);rotate('LowerLeg_R',(1,0,0),25)
        rotate('Torso',(1,0,0),12)
        reach('L',(.25,-.19,1.29),(.25,-.40,1.45))
        reach('R',(-.24,.10,1.30),(-.24,-.03,1.06))
    if kind=='CrouchQA':
        for side in ['L','R']:
            rotate('UpperLeg_'+side,(1,0,0),-58)
            rotate('LowerLeg_'+side,(1,0,0),102)
            rotate('Foot_'+side,(1,0,0),-44)
        bpy.context.view_layer.update()
        hips=rig.pose.bones['Body']
        world=rig.matrix_world@hips.matrix
        world.translation+=Vector((0,.16,-.34))
        hips.matrix=rig.matrix_world.inverted()@world
        rotate('Torso',(1,0,0),18)
        # This source rig has root-level IK feet, separate from the thigh chains.
        for side in ['L','R']:rotate('Foot_'+side,(1,0,0),0)
    if kind=='RunQA':
        bpy.context.view_layer.update()
        for side in ['L','R']:
            foot=rig.pose.bones['Foot_'+side]
            world=rig.matrix_world@foot.matrix
            world.translation=rig.matrix_world@rig.pose.bones['LowerLeg_'+side].tail+Vector((0,0,.038))
            foot.matrix=rig.matrix_world.inverted()@world
    bpy.context.view_layer.update()


(OUT/'Animations').mkdir(exist_ok=True)
actions=[]
scene.render.fps=30
for name,duration in [('Idle',8),('LookShip',5),('CheckWrist',4),('AdjustGlove',4),('Join',2),('Ready',2),('Unready',2),('Leave',2)]:
    rig.animation_data_clear();action=bpy.data.actions.new('A_FE_'+name);rig.animation_data_create();rig.animation_data.action=action
    last=duration*30+1;scene.frame_start=1;scene.frame_end=last
    for frame in range(1,last+1,5):
        pose(name,(frame-1)/(last-1))
        for bone in rig.pose.bones:
            bone.keyframe_insert(data_path='rotation_quaternion',frame=frame,group=bone.name)
    pose(name,1)
    for bone in rig.pose.bones:bone.keyframe_insert(data_path='rotation_quaternion',frame=last,group=bone.name)
    bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);bpy.context.view_layer.objects.active=rig
    path=OUT/'Animations'/('A_FE_'+name+'.fbx')
    rig.name='Armature' # UE's Blender FBX convention strips the object, preserving Root as bone zero.
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'ARMATURE'},add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,axis_forward='-Y',axis_up='Z')
    rig.name='JTS_AvatarRig'
    actions.append({'id':'A_FE_'+name,'source':'original_blender_keyframes','file':path.relative_to(OUT).as_posix(),'seconds':duration,'fps':30,'loop':name=='Idle'})
rig.animation_data_clear()
scene.frame_set(1);pose('Idle',0)
scene.render.resolution_x=512;scene.render.resolution_y=768;scene.render.resolution_percentage=100
scene.camera.data.ortho_scale=2.25
scene.render.film_transparent=True
ground=next(o for o in scene.objects if o.type=='MESH' and o.dimensions.x>150)
ground.name='FE_StudioGround'
ground.hide_render=True
for mesh in characters if RENDER_PREVIEWS else []:
    mesh.hide_render=False;mesh.hide_set(False)
    scene.render.filepath=str(OUT/'UI'/('T_Thumb_'+mesh.name.removeprefix('SK_FE_')+'.png'))
    bpy.ops.render.render(write_still=True)
    mesh.hide_render=True;mesh.hide_set(True)
    actions.append({'id':'T_Thumb_'+mesh.name.removeprefix('SK_FE_'),'source':'rendered_original_suit','file':'UI/'+Path(scene.render.filepath).name,'size':[512,768]})
characters[0].hide_render=False;characters[0].hide_set(False)
ground.hide_render=False;scene.render.film_transparent=False
scene.render.resolution_x=900;scene.render.resolution_y=1100
for kind in ['Idle','RifleQA','RunQA','CrouchQA','CheckWrist'] if RENDER_PREVIEWS else []:
    pose(kind,.5)
    if kind=='RifleQA':
        rifle=bpy.data.objects['SM_FE_RiflePose']
        rifle.hide_render=False;rifle.hide_set(False)
        rifle.location=(0,-.43,1.30);rifle.rotation_euler=(0,0,0)
    evaluated=characters[0].evaluated_get(bpy.context.evaluated_depsgraph_get())
    points=[evaluated.matrix_world@v.co for v in evaluated.data.vertices]
    bounds=[[min(p[i] for p in points),max(p[i] for p in points)] for i in range(3)]
    assert all(math.isfinite(c) for p in points for c in p)
    scene.render.filepath=str(OUT/'Previews'/('Pose_'+kind+'.png'));bpy.ops.render.render(write_still=True)
    if kind=='RifleQA':rifle.hide_render=True;rifle.hide_set(True)
    actions.append({'id':'QA_'+kind,'source':'original_pose_qa','file':'Previews/'+Path(scene.render.filepath).name,'bounds_m':bounds})
pose('Idle',0)
if not RENDER_PREVIEWS:
    actions += [e for e in json.loads((OUT/'animations_manifest.json').read_text(encoding='utf-8')) if not e['id'].startswith('A_FE_')]
(OUT/'animations_manifest.json').write_text(json.dumps(actions,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'FrontEnd_Models.blend'))
result={'actions':8,'thumbnails':5,'pose_previews':5}
