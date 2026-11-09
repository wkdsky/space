"""Original mirror-skimmer brood leader, rig, baked attacks and export, authored through Blender MCP."""
import bpy, math, json, sys
from pathlib import Path
from mathutils import Vector, Matrix, Euler
sys.path.insert(0,'D:/projects/space/Tools/Art')
import moon_creatures_geometry as geo
import moon_creatures_motion as motion
KIND='MirrorBoss'
SCENE='JTS_MirrorBoss'
OUT=Path('D:/projects/space/SourceArt/MirrorBoss')
CLIPS=[('Idle',91,True),('Walk',41,True),('Emerge',91,False),('Slam',76,False),
       ('Sweep',91,False),('Charge',97,False),('Leap',112,False),('Stagger',28,False),('Death',97,False),
       ('TurnLeft',37,True),('TurnRight',37,True)]

def build():
    scene=bpy.data.scenes.get(SCENE) or bpy.data.scenes.new(SCENE)
    bpy.context.window.scene=scene
    if bpy.data.objects.get('RIG_'+KIND):return
    scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1;scene.render.fps=30
    b=geo.Builder.__new__(geo.Builder);b.kind=KIND;b.scene=scene
    b.collection=bpy.data.collections.new('CREATURE_'+KIND);scene.collection.children.link(b.collection)
    b.parts=[];b.bones=[]
    b.mats=[geo.material('M_MB_'+n,c,r) for n,c,r in [
      ('MirrorShell',(.54,.61,.65),.23),('WornRim',(.28,.34,.36),.55),
      ('Joint',(.035,.052,.057),.8),('Crown',(.78,.83,.82),.2),
      ('SoftTissue',(.28,.13,.075),.7),('Eyes',(.20,.72,.84),.17)]]
    for i in (0,3):next(n for n in b.mats[i].node_tree.nodes if n.type=='BSDF_PRINCIPLED').inputs['Metallic'].default_value=.78
    eye=next(n for n in b.mats[5].node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    eye.inputs['Emission Color'].default_value=(.05,.38,.55,1);eye.inputs['Emission Strength'].default_value=.7
    b.bone('root',(0,0,0),(0,0,.3))
    b.bone('body',(0,0,1.12),(.5,0,1.12),'root')
    b.bone('abdomen',(-.65,0,1.0),(-1.2,0,1),'body')
    b.bone('head',(.95,0,.82),(1.4,0,.82),'body')
    b.pebble('flexible_ventral_body',(-.05,0,.98),(2.05,1.55,.62),'body',2,power=3,n=20)
    # Same broad, bevelled mirror mantle as the small skimmer, with a high shoulder and heavy laminated edge.
    outline=geo.rounded(1.28,.96,4.5,32)
    b.loft('massive_mirror_mantle',outline,[(1.03,.88,0),(1.18,1,0),(1.65,.97,-.08),(1.82,.72,-.12)],'body',0,True)
    b.loft('thick_chipped_rim',outline,[(1.00,.91,0),(1.11,1.02,0),(1.22,1,0)],'body',1)
    # Overlapping rear armour and a raised, ridged brow distinguish the alpha silhouette.
    b.pebble('rear_armour',(-1.10,0,1.10),(1.02,1.52,.68),'abdomen',0,power=3,n=20)
    b.pebble('brow_shield',(.88,0,1.45),(.82,1.76,.52),'body',3,power=3.5,n=18)
    for i in range(5):
        y=(i-2)*.25
        b.pebble('brow_ridge_'+str(i),(.76,y,1.77+(.14 if i==2 else .07)),(.58,.15,.18),'body',1,power=3,n=8)
    b.pebble('recessed_head',(1.12,0,.86),(.60,.84,.45),'head',1,power=3,n=16)
    b.pebble('exposed_throat',(1.13,0,.64),(.38,.52,.19),'head',4,power=2.4,n=12)
    for side,s in [('L',1),('R',-1)]:
        b.pebble('eye_'+side,(1.39,s*.27,.99),(.11,.16,.10),'head',5,n=8)
        b.bone('mouth_'+side,(1.30,s*.14,.69),(1.58,s*.14,.66),'head')
        b.link('jaw_'+side,(1.3,s*.14,.69),(1.6,s*.09,.65),.10,.04,'mouth_'+side,1)
    legs=[]
    for side,s in [('L',1),('R',-1)]:
        for i,x in enumerate((.12,-.88)):
            h=Vector((x,s*.73,1.03));k=h+Vector((-.12,s*.61,-.15));f=Vector((x-.26,s*1.62,.08));toe=f+Vector((.22,s*.07,-.02))
            label=side+str(i+2);upper='leg_'+label+'_upper';lower='leg_'+label+'_lower';tip='leg_'+label+'_toe'
            b.bone(upper,h,k,'body');b.bone(lower,k,f,upper);b.bone(tip,f,toe,lower)
            b.link(label+'_haunch',h,k,.19,.155,upper,1,8)
            b.pebble(label+'_knee',k,(.32,.30,.28),lower,2,n=10)
            b.link(label+'_shin',k,f,.145,.075,lower,0,8)
            b.pebble(label+'_grounded_pad',(f+toe)*.5,(.42,.25,.14),tip,1,power=3,n=10)
            legs.append(dict(label=label,h=list(h),k=list(k),f=list(f),toe=list(toe),upper=upper,lower=lower,tip=tip))
        h=Vector((.62,s*.65,1.17));k=Vector((1.12,s*1.16,.97));w=Vector((1.72,s*1.22,.80))
        b.bone('claw_'+side+'_upper',h,k,'body');b.bone('claw_'+side+'_lower',k,w,'claw_'+side+'_upper')
        b.bone('claw_'+side,w,(2.18,s*1.22,.80),'claw_'+side+'_lower')
        b.bone('finger_'+side,(1.93,s*1.08,.8),(2.35,s*.97,.8),'claw_'+side)
        b.link('arm_shoulder_'+side,h,k,.28,.24,'claw_'+side+'_upper',1,10)
        b.pebble('arm_elbow_'+side,k,(.43,.42,.38),'claw_'+side+'_lower',2,n=12)
        b.link('arm_forearm_'+side,k,w,.25,.33,'claw_'+side+'_lower',0,10)
        b.pebble('chela_palm_'+side,(1.91,s*1.25,.80),(.81,.70,.48),'claw_'+side,0,power=3.3,n=16)
        b.pebble('chela_dorsal_'+side,(1.91,s*1.25,1.03),(.54,.47,.13),'claw_'+side,3,power=3,n=10)
        # Two visibly separated, hooked fingers, with dark serrations inside the closing gap.
        for moving,sign in [(False,1),(True,-1)]:
            bone='finger_'+side if moving else 'claw_'+side
            pts=[(2.04,s*(1.25+sign*.23),.80),(2.48,s*(1.25+sign*.29),.78),(2.78,s*(1.25+sign*.12),.73),(2.84,s*1.25,.71)]
            for j,(a,c) in enumerate(zip(pts,pts[1:])):b.link(('mobile' if moving else 'fixed')+'_talon_'+side+str(j),a,c,.16-j*.035,.12-j*.04,bone,1 if j==2 else 0,8)
            for j in range(3):
                b.pebble('tooth_'+side+str(moving)+str(j),(2.28+j*.12,s*(1.25+sign*.14),.78),(.10,.12,.15),bone,2,power=2,n=6)
        for j in range(3):b.pebble('claw_wear_'+side+str(j),(1.68+j*.14,s*1.52,.89),(.13,.10,.14),'claw_'+side,1,n=6)
    spec=dict(legs=legs,body_z=1.12,length=4.5,height=2,walk_speed=1.9,stride=.52,lift=.12)
    result=b.finish(spec)
    rig=bpy.data.objects['RIG_'+KIND]
    rig['clips']=json.dumps([dict(name=n,action='A_'+KIND+'_'+n,frames=f,loop=l) for n,f,l in CLIPS])
    rig['role']='Brood leader; forelegs evolved into two heavy chelae; four locomotor legs.'
    return result

def pose(rig,spec,clip,t):
    bones=rig.data.bones
    root=Matrix.Identity(4);body=Matrix.Identity(4)
    legs=spec['legs'];up=0;pitch=0;roll=0
    a=0;b=0;claw_angles={s:(0,0,0) for s in ('L','R')}
    if clip=='Idle':up=.016*math.sin(t*2*math.pi)
    if clip=='Walk':up=.025*math.sin(t*4*math.pi)
    if clip in ('TurnLeft','TurnRight'):
        up=.012*math.sin(t*4*math.pi)
        # A low, supported pivot, never a root pitch/roll flip.
        b=.05*math.sin(t*2*math.pi)
    if clip=='Emerge':
        u=motion.smooth((t-.08)/.72);root=Matrix.Translation((0,0,-2.25*(1-u)))
        pitch=.28*motion.window(t,.15,.4,.63,.9);up=.15*motion.window(t,.4,.6,.75,1)
        a=-.55*motion.window(t,.10,.3,.55,.85)
    if clip=='Slam':
        # 1.05 s rise / .25 s impact / 1.2 s exposed recovery.
        raise_=motion.smooth(t/.42);drop=motion.smooth((t-.42)/.10);recover=motion.smooth((t-.70)/.30)
        a=-1.35*raise_+1.65*drop-.30*recover
        pitch=-.12*raise_+.25*drop-.13*recover;up=.16*raise_-.25*drop+.09*recover
    if clip=='Sweep':
        wind=motion.smooth(t/.367);sweep=motion.smooth((t-.367)/.20);recover=motion.smooth((t-.74)/.26)
        roll=.1*math.sin(t*math.pi)
        claw_angles['L']=(-.2,-.35*wind+.45*sweep-.1*recover,-.95*wind+1.9*sweep-.95*recover)
        claw_angles['R']=(.1,-.20*wind+.25*sweep-.05*recover,-.65*wind+1.4*sweep-.75*recover)
    if clip=='Charge':
        crouch=motion.window(t,0,.2,.61,.84);up=-.19*crouch;pitch=.18*crouch
        a=.18*crouch;b=-.35*crouch
    if clip=='Leap':
        crouch=motion.window(t,0,.24,.35,.40);air=motion.window(t,.35,.43,.56,.61);impact=motion.window(t,.60,.63,.70,.95)
        up=-.25*crouch-.20*impact;pitch=.15*crouch-.22*air+.25*impact
        a=-.9*air+.28*impact
    if clip=='Stagger':pitch=-.16*math.sin(t*math.pi);roll=.12*math.sin(t*2*math.pi);up=-.20*math.sin(t*math.pi)
    if clip=='Death':
        # The chelae lose support first. The shell then settles upright as the legs splay.
        # Leave the last quarter entirely still; corpse fading is a runtime material effect.
        collapse=motion.smooth((t-.27)/.47)
        up=-.53*collapse
        pitch=.025*motion.window(t,.27,.44,.62,.74)
    body=Matrix.Translation((0,0,up)) @ motion.rot_at((0,0,1.12),(roll,pitch,0))
    desired={n:root@body@bone.matrix_local for n,bone in [(bo.name,bo) for bo in bones]}
    desired['root']=root@bones['root'].matrix_local
    for side,s in [('L',1),('R',-1)]:
        h=Vector((.62,s*.65,1.17));k=Vector((1.12,s*1.16,.97));w=Vector((1.72,s*1.22,.80))
        angles=claw_angles[side] if clip=='Sweep' else (0,a,s*b)
        transform=root@body@motion.rot_at(h,angles)
        if clip=='Death':
            release=motion.smooth((t-(.04 if side=='L' else .07))/.22)
            # Entire claw chain separates at the shoulder and lands independently of the torso.
            offset=Vector((.30*release,s*.40*release,-.37*release))
            transform=Matrix.Translation(offset)@motion.rot_at(h,(s*.06*release,.08*release,-s*.12*release))
        for name in ['claw_'+side+'_upper','claw_'+side+'_lower','claw_'+side,'finger_'+side]:desired[name]=transform@bones[name].matrix_local
        gape=.20*math.sin(t*2*math.pi) if clip in ('Idle','Emerge') else .32*motion.window(t,.05,.25,.6,.95)
        if clip=='Death':gape=.16*motion.smooth((t-.06)/.20)
        desired['finger_'+side]=transform@motion.rot_at((1.93,s*1.08,.8),(0,0,-s*gape))@bones['finger_'+side].matrix_local
    for i,leg in enumerate(legs):
        h=body@Vector(leg['h']);foot=Vector(leg['f']);toe=Vector(leg['toe'])
        if clip in ('Walk','Charge'):
            running=clip=='Charge' and .328<t<.625
            phase=t* (3 if running else 1)+(i%2)*.5+(i//2)*.5
            stride,lift=motion.gait(phase,.72 if running else .52,.22 if running else .12)
            foot+=Vector((stride,0,lift));toe+=Vector((stride,0,lift))
        if clip in ('TurnLeft','TurnRight'):
            sign=1 if clip=='TurnLeft' else -1
            phase=t+(i%2)*.5+(i//2)*.5
            stride,lift=motion.gait(phase,.30,.10)
            # Feet travel tangentially around the body: stance counter-rotates the root yaw.
            tangent=Vector((-foot.y,foot.x,0)).normalized()*sign*stride
            foot+=tangent+Vector((0,0,lift));toe+=tangent+Vector((0,0,lift))
        if clip=='Leap':
            fold=motion.window(t,.36,.43,.55,.61)
            foot=foot.lerp(body@Vector(leg['f'])+Vector((0,0,.55)),fold);toe=foot+Vector((.2,0,0))
        if clip=='Death':
            spread=motion.smooth((t-.27-(i%2)*.025)/.43)
            side=1 if foot.y>0 else -1
            foot+=Vector((.16 if leg['h'][0]>0 else -.16,side*.38,-.015))*spread
            toe=foot+Vector((.22,side*.07,-.02))
        knee,target,_=motion.solve_leg(leg,h,foot)
        desired[leg['upper']]=root@motion.orient(bones[leg['upper']],h,knee-h)
        desired[leg['lower']]=root@motion.orient(bones[leg['lower']],knee,target-knee)
        desired[leg['tip']]=root@motion.orient(bones[leg['tip']],target,toe-target)
    motion.set_matrices(rig,desired)

def animate():
    rig=bpy.data.objects['RIG_'+KIND];spec=json.loads(rig['motion_spec']);scene=bpy.context.scene
    for name,count,loop in CLIPS:
        action=bpy.data.actions.get('A_'+KIND+'_'+name)
        if action:bpy.data.actions.remove(action)
        action=bpy.data.actions.new('A_'+KIND+'_'+name);action.use_fake_user=True
        rig.animation_data_create();rig.animation_data.action=action
        for frame in range(1,count+1):
            pose(rig,spec,name,(frame-1)/(count-1))
            for pb in rig.pose.bones:
                pb.keyframe_insert('location',frame=frame);pb.keyframe_insert('rotation_quaternion',frame=frame)
        for layer in action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for curve in bag.fcurves:
                        for key in curve.keyframe_points:key.interpolation='LINEAR'
    return dict(clips=len(CLIPS),frames=sum(f for _,f,_ in CLIPS))

def delivery():
    import moon_creatures_delivery as deliver
    deliver.SCENE=SCENE;deliver.KINDS=(KIND,);deliver.ROOT=OUT;deliver.QA=Path('D:/projects/space/Saved/MirrorBoss')
    return deliver

def preview():
    d=delivery();d.stage();scene,rig=d.show(KIND)
    camera=scene.camera;camera.location=(7,-9,6)
    camera.rotation_euler=(Vector((.4,0,.9))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.ortho_scale=7.4
    for light in bpy.data.objects:
        if light.type=='LIGHT' and light.name.startswith('STUDIO_'):
            light.location*=7;light.data.energy*=49;light.data.size*=7
    scene.render.resolution_x=1200;scene.render.resolution_y=900
    OUT.mkdir(parents=True,exist_ok=True);scene.render.filepath=str(OUT/'MirrorBoss.png')
    bpy.ops.render.render(write_still=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'MirrorBoss.blend'),compress=True)
    return scene.render.filepath

def run():
    result=build();animation=animate();d=delivery();validation=validate()
    exported=d.export_fbx_centimetres(KIND);portrait=preview()
    print(json.dumps(dict(model=result,animation=animation,validation=validation,export=exported,preview=portrait)))

def revision_assets():
    """Append grounded pivot cycles and export a separate important-item access card."""
    rig=bpy.data.objects['RIG_'+KIND]
    all_clips=list(CLIPS)
    old_clips=rig['clips']
    try:
        globals()['CLIPS']=[c for c in all_clips if c[0].startswith('Turn')]
        animate()
        rig['clips']=json.dumps([dict(name=n,action='A_'+KIND+'_'+n,frames=f,loop=l) for n,f,l in CLIPS])
        result=delivery().export_fbx_centimetres(KIND)
    finally:
        globals()['CLIPS']=all_clips
        rig['clips']=json.dumps([dict(name=n,action='A_'+KIND+'_'+n,frames=f,loop=l) for n,f,l in all_clips])
    scene,rig=delivery().show(KIND,'TurnLeft',10)
    scene.render.filepath=str(OUT/'TurnLeft.png');bpy.ops.render.render(write_still=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'MirrorBoss.blend'),compress=True)
    return dict(export=result,validation=validate(),card=build_access_card())

def revision_death():
    """Rebake only the upright collapse and export only its animation-only FBX."""
    original=list(CLIPS)
    try:
        globals()['CLIPS']=[('Death',97,False)]
        animate()
    finally:globals()['CLIPS']=original
    d=delivery();scene,rig=d.show(KIND,'Death',1)
    mesh=bpy.data.objects['SK_'+KIND];samples=[]
    for frame in (1,10,20,28,40,55,72,80,97):
        scene.frame_set(frame);bpy.context.view_layer.update()
        obj=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get());data=obj.to_mesh()
        samples.append(dict(frame=frame,min_z=min(v.co.z for v in data.vertices),
            body=list(rig.pose.bones['body'].matrix.translation),
            body_up=list(rig.pose.bones['body'].matrix.col[2][:3]),
            claws=[list(rig.pose.bones['claw_'+s].matrix.translation) for s in ('L','R')],
            feet=[list(rig.pose.bones[l['tip']].matrix.translation) for l in json.loads(rig['motion_spec'])['legs']]))
        obj.to_mesh_clear()
    assert min(s['min_z'] for s in samples)>-.025,samples
    assert all(s['body_up'][2]>.99 for s in samples),samples
    assert (Vector(samples[-1]['body'])-Vector(samples[-2]['body'])).length<1e-6
    copy=rig.copy();copy.data=rig.data.copy();copy.animation_data_clear();scene.collection.objects.link(copy)
    scaled=bpy.data.actions['A_'+KIND+'_Death'].copy()
    units=scene.unit_settings.scale_length
    try:
        copy.name='Armature';copy.data.transform(Matrix.Scale(100,4))
        for curve in d.curves(scaled):
            if curve.data_path.endswith('.location'):
                for key in curve.keyframe_points:
                    key.co.y*=100;key.handle_left.y*=100;key.handle_right.y*=100
        copy.animation_data_create();copy.animation_data.action=scaled;copy.animation_data.action_slot=scaled.slots[0]
        for bone in copy.pose.bones:bone.location=(0,0,0);bone.rotation_quaternion=(1,0,0,0);bone.scale=(1,1,1)
        scene.unit_settings.scale_length=.01
        bpy.ops.object.select_all(action='DESELECT');copy.select_set(True);bpy.context.view_layer.objects.active=copy
        scene.frame_start=1;scene.frame_end=97;scene.frame_set(1)
        bpy.ops.export_scene.fbx(filepath=str(OUT/KIND/('A_'+KIND+'_Death.fbx')),use_selection=True,
            add_leaf_bones=False,use_armature_deform_only=True,axis_forward='-Y',axis_up='Z',
            global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',object_types={'ARMATURE'},
            bake_anim=True,bake_anim_use_all_bones=True,bake_anim_use_nla_strips=False,bake_anim_use_all_actions=False,
            bake_anim_force_startend_keying=True,bake_anim_step=1,bake_anim_simplify_factor=0)
    finally:
        data=copy.data;bpy.data.objects.remove(copy,do_unlink=True);bpy.data.armatures.remove(data)
        bpy.data.actions.remove(scaled);scene.unit_settings.scale_length=units
    scene,rig=d.show(KIND,'Death',97)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'MirrorBoss.blend'),compress=True)
    qa=Path('D:/projects/space/Saved/MirrorBoss');qa.mkdir(parents=True,exist_ok=True)
    (qa/'DeathPoseSamples.json').write_text(json.dumps(samples,indent=2))
    scene.render.resolution_x=800;scene.render.resolution_y=600;scene.render.resolution_percentage=100
    scene.render.filepath=str(qa/'DeathCollapsed.png');bpy.ops.render.render(write_still=True)
    return dict(animation='A_MirrorBoss_Death',samples=samples)

def build_access_card():
    scene=bpy.data.scenes.get('JTS_MarsAccessCard') or bpy.data.scenes.new('JTS_MarsAccessCard')
    bpy.context.window.scene=scene
    objects=[]
    def mat(name,color,metal=.0,emission=0):
        m=bpy.data.materials.get(name) or bpy.data.materials.new(name);m.use_nodes=True
        node=next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
        node.inputs['Base Color'].default_value=(*color,1);node.inputs['Metallic'].default_value=metal
        node.inputs['Roughness'].default_value=.35
        node.inputs['Emission Color'].default_value=(*color,1);node.inputs['Emission Strength'].default_value=emission
        return m
    shell=mat('M_CardAlloy',(.60,.66,.69),.65)
    dark=mat('M_CardInset',(.016,.036,.049),.15)
    light=mat('M_CardSignal',(.02,.65,.90),.2,2)
    mars=mat('M_CardMars',(.7,.12,.045),.3)
    def box(name,position,size,material,bevel=0):
        bpy.ops.mesh.primitive_cube_add(size=1,location=position);o=bpy.context.object;o.name=name
        o.dimensions=size;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
        if bevel:
            mod=o.modifiers.new('RoundedMachinedEdge','BEVEL');mod.width=bevel;mod.segments=2
            bpy.ops.object.modifier_apply(modifier=mod.name)
        o.data.materials.append(material);objects.append(o);return o
    box('access_card_shell',(0,0,0),(.26,.17,.014),shell,.009)
    box('recessed_face',(0,0,.008),(.226,.136,.004),dark,.004)
    box('signal_stripe',(-.099,0,.012),(.009,.12,.004),light,.002)
    box('chip',(-.048,.025,.012),(.038,.042,.007),light,.003)
    for i in range(3):box('engraved_data_'+str(i),(.027,.033-i*.024,.012),(.086-i*.014,.004,.003),shell)
    box('mars_id',(.066,-.044,.012),(.038,.018,.004),mars,.003)
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0];bpy.ops.object.join()
    mesh=bpy.context.object;mesh.name='SM_MarsAccessCard'
    mesh.scale=(100,100,100);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=.01
    path=OUT/'SM_MarsAccessCard.fbx'
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},bake_anim=False,
        axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS')
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'MirrorBoss.blend'),compress=True)
    return str(path)

def validate():
    d=delivery();scene,rig=d.show(KIND);mesh=bpy.data.objects['SK_'+KIND]
    errors=[]
    for vertex in mesh.data.vertices:
        if abs(sum(g.weight for g in vertex.groups)-1)>1e-5:errors.append('Invalid weight')
    for name,count,loop in CLIPS:
        scene,rig=d.show(KIND,name)
        if loop:
            error=max(abs(c.evaluate(1)-c.evaluate(count)) for c in d.curves(rig.animation_data.action))
            if error>1e-5:errors.append('Loop discontinuity '+name)
        for f in range(1,count+1,5):
            scene.frame_set(f);dg=bpy.context.evaluated_depsgraph_get();dg.update()
            obj=mesh.evaluated_get(dg);data=obj.to_mesh()
            coords=[v.co for v in data.vertices]
            if any(not math.isfinite(c) for v in coords for c in v):errors.append('Nonfinite '+name)
            if any(max(v[i] for v in coords)-min(v[i] for v in coords)>8 for i in range(3)):errors.append('Exploded '+name)
            obj.to_mesh_clear()
    assert not errors,errors
    return dict(passed=True,vertices=len(mesh.data.vertices),triangles=len(mesh.data.polygons),bones=len(rig.data.bones),clips=len(CLIPS))
if __name__ == '__main__':run()
