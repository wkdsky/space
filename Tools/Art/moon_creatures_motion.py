"""Bake species-specific skeletal motion; analytical leg IK is export independent."""
import bpy
import json
import math
from mathutils import Matrix, Vector, Euler

TAU=2*math.pi
DEATH_FRAMES={"MirrorSkimmer":31,"Riftlip":37,"SpireWorker":31,"SpireSoldier":37}


def smooth(x):
    x=max(0,min(1,x))
    return x*x*(3-2*x)


def window(t,a,b,c,d):
    return smooth((t-a)/(b-a))* (1-smooth((t-c)/(d-c)))


def rot_at(point, angles):
    return Matrix.Translation(point) @ Euler(angles,"XYZ").to_matrix().to_4x4() @ Matrix.Translation(-Vector(point))


def orient(bone, point, direction):
    rest=bone.matrix_local.to_3x3()
    q=rest.col[1].normalized().rotation_difference(Vector(direction).normalized())
    result=(q.to_matrix()@rest).to_4x4()
    result.translation=point
    return result


def solve_leg(leg,h,f):
    a=(Vector(leg["k"])-Vector(leg["h"])).length
    b=(Vector(leg["f"])-Vector(leg["k"])).length
    delta=f-h
    d=min(max(delta.length,abs(a-b)+.00001),(a+b)*.999)
    axis=delta.normalized()
    target=h+axis*d
    pole=Vector(leg["k"])-Vector(leg["h"])
    pole=pole-axis*pole.dot(axis)
    if pole.length<.0001:
        pole=Vector((0,0,1))-axis*axis.z
    pole.normalize()
    along=(a*a-b*b+d*d)/(2*d)
    knee=h+along*axis+math.sqrt(max(0,a*a-along*along))*pole
    return knee,target,(target-f).length


def set_matrices(rig, desired):
    # Convert complete armature-space poses into local bone channels. No live
    # constraints, drivers or external targets are needed in Unreal.
    for bone in rig.data.bones:
        rest=bone.matrix_local
        if bone.parent:
            rest_local=bone.parent.matrix_local.inverted()@rest
            basis=rest_local.inverted()@desired[bone.parent.name].inverted()@desired[bone.name]
        else:
            basis=rest.inverted()@desired[bone.name]
        loc,quat,scale=basis.decompose()
        pb=rig.pose.bones[bone.name]
        pb.location=loc
        pb.rotation_quaternion=quat
        pb.scale=(1,1,1)


def gait(phase, stride, lift):
    # A linear stance keeps planted feet stationary at the documented ground
    # speed. Swing returns smoothly without dragging toes through the ground.
    phase%=1
    stance=.62
    if phase<stance:
        return stride*(.5-phase/stance),0
    u=(phase-stance)/(1-stance)
    return stride*(-.5+smooth(u)),lift*math.sin(math.pi*u)


def stride_multiplier(kind,clip):
    if clip=="Run":
        return 1.1 if kind=="Riftlip" else 1.35
    return 1.35 if clip=="Charge" else .75 if clip=="Carry" else 1


def clip_list(kind):
    walk={"MirrorSkimmer":49,"Riftlip":31,"SpireWorker":43,"SpireSoldier":49}[kind]
    run={"MirrorSkimmer":13,"Riftlip":11,"SpireWorker":19,"SpireSoldier":15}[kind]
    clips=[("Idle",91,True),("Walk",walk,True),("Run",run,True),
           ("FleeStart",31,False),("TurnLeft",37,False),("TurnRight",37,False),
           ("Attack",43,False),("Burrow",61,False),("UndergroundIdle",61,True),
           ("Emerge",61,False),("Forage",91,True),("Feed",91,True),
           ("HitReact",31,False),("Death",61,False)]
    extras={
        "MirrorSkimmer":[("Bask",121,True),("FlipRecover",73,False)],
        "Riftlip":[("SurfaceProbe",73,True),("ClingBite",55,False),("CrackDash",31,False)],
        "SpireWorker":[("Gather",91,True),("Carry",43,True),("Repair",91,True),
                       ("FeedColony",91,True),("FlipRecover",79,False)],
        "SpireSoldier":[("Guard",91,True),("ChargeWindup",46,False),("Charge",13,True),
                        ("ClampHold",61,True),("Stagger",49,False),("Recover",61,False),
                        ("FeedFromWorker",91,True)]
    }
    return [(name,DEATH_FRAMES[kind] if name=="Death" else count,loop)
            for name,count,loop in clips+extras[kind]]


def death_params(kind,t,p):
    """Readable impact, species-specific loss of support, then a quiet held corpse."""
    p["grounded_death"]=True
    p["death_kick"]=.035*window(t,.44,.48,.64,.72)
    p["abdomen"]=[0,0,0]
    if kind=="MirrorSkimmer":
        flight=max(0,min(1,(t-.065)/.38));land=max(0,t-.445)
        p["death_air"]=.27*4*flight*(1-flight)
        p["angles"]=[math.radians(175)*smooth((t-.08)/.31)+.13*math.sin(land*40)*math.exp(-land*19),0,-.18*smooth(t/.4)]
        p["root"].x=-.055*smooth(t/.44)
        p["dead"]=smooth(t/.08);p["death_curl"]=.96*smooth((t-.10)/.23)
        p["open"]=.12*window(t,0,.08,.38,.5)
    elif kind=="Riftlip":
        flight=max(0,min(1,(t-.045)/.285));curl=smooth((t-.12)/.27)
        p["death_air"]=.21*4*flight*(1-flight)
        p["angles"]=[math.radians(160)*smooth((t-.07)/.26),-.12*curl,.28*curl]
        p["head"]=[0,.18*curl,1.12*curl];p["abdomen"]=[0,-.12*curl,-1.25*curl]
        p["root"].x=-.035*smooth(t/.33)
        p["dead"]=smooth(t/.07);p["death_curl"]=.96*curl
        p["open"]=.12*(1-curl);p["curl_sections"]=True
    elif kind=="SpireWorker":
        flight=max(0,min(1,(t-.045)/.32));land=max(0,t-.365);fall=smooth((t-.07)/.30)
        p["death_air"]=.16*4*flight*(1-flight)
        p["angles"]=[-math.radians(155)*fall-.11*math.sin(land*38)*math.exp(-land*18),.25*fall,-.55*fall]
        p["head"]=[0,.38*fall,-.12*fall];p["abdomen"]=[.18*fall,-.20*fall,.30*fall]
        p["root"].y=-.045*fall;p["dead"]=smooth(t/.08)
        p["death_splay"]=.8*window(t,.02,.08,.19,.30)
        p["death_curl"]=.90*smooth((t-.20)/.24);p["open"]=.20*smooth(t/.12)
    else:
        # The shield bearer loses its front supports, slams down, and loose plates follow.
        slam=smooth((t-.035)/.16);rear=smooth((t-.13)/.19);settle=max(0,t-.24)
        p["body"].z=-.10*slam
        p["angles"]=[.38*slam*(1-rear)+.06*rear+.07*math.sin(settle*35)*math.exp(-settle*18),.08*slam*(1-rear),.13*slam]
        p["head"]=[-.06*rear,-.03*slam,-.25*slam]
        p["head_offset"]=Vector((.04*slam,-.04*slam,-.01*slam))
        p["abdomen"]=[-.06*rear,0,.34*rear]
        p["abdomen_offset"]=Vector((-.055*rear,.042*rear,0))
        p["death_splay"]=1.0*slam;p["dead"]=.9*slam;p["death_curl"]=.62*rear
        p["death_kick"]=.02*window(t,.24,.30,.46,.56);p["open"]=.44*slam
    return p


def params(kind,clip,t,spec):
    p=dict(body=Vector((0,0,0)),angles=[0,0,0],head=[0,0,0],abdomen=[0,0,0],
           root=Vector((0,0,0)),root_roll=0,open=0,cycles=0,stride=0,lift=0,
           shovel=0,fore=Vector((0,0,0)),toe_pitch=0,dead=0,lip_roll=0,lip_pitch=0)
    wave=math.sin(TAU*t)
    p["body"].z=.0015*wave
    p["abdomen"][1]=.012*wave
    if clip in ("Walk","Run","Carry","Charge"):
        multiplier=stride_multiplier(kind,clip)
        p.update(cycles=t,stride=spec["stride"]*multiplier,lift=spec["lift"]*(.65 if kind=="MirrorSkimmer" else 1))
        p["body"].z=(.0015 if kind=="MirrorSkimmer" else .004)*math.sin(2*TAU*t)
        p["angles"][0]=(.009 if kind=="MirrorSkimmer" else .025)*wave
        p["head"][1]=.035 if clip=="Charge" else .008*math.sin(2*TAU*t)
        if clip=="Run":
            p["body"].z-=.018 if kind=="MirrorSkimmer" else .008
        if clip=="Charge":
            p["angles"][1]=.08; p["head"][1]=.10; p["open"]=.10
        if clip=="Carry":
            p["shovel"]=1; p["fore"]=Vector((.015,-.018,.064)); p["toe_pitch"]=-.6
            p["open"]=.045
    elif clip=="FleeStart":
        crouch=window(t,0,.18,.45,.75)
        p["body"].z=-.026*crouch
        p["head"][1]=.10*crouch
        p["cycles"]=max(0,(t-.45))*2
        p["stride"]=spec["stride"]*smooth((t-.45)/.3)
        p["lift"]=spec["lift"]*.6
    elif clip in ("TurnLeft","TurnRight"):
        sign=1 if clip=="TurnLeft" else -1
        p["cycles"]=t*2
        p["stride"]=spec["stride"]*.5
        p["lift"]=spec["lift"]*.8
        p["angles"][2]=sign*.12*math.sin(math.pi*t)**2
        p["head"][2]=sign*.14*math.sin(math.pi*t)**2
    elif clip in ("Attack","ClingBite"):
        wind=window(t,0,.22,.27,.43)
        strike=window(t,.26,.40,.52,.86)
        p["body"].x=-.022*wind+.045*strike
        p["body"].z=.006*wind-.012*strike
        p["head"][1]=-.12*wind+.15*strike
        jaw=window(t,.08,.26,.37,.46)
        p["open"]=(.20 if kind=="MirrorSkimmer" else .34)*jaw
        if kind=="MirrorSkimmer":
            # A harmless defensive shell nudge; never a predator's bite.
            p["body"].x*=.45; p["body"].z=-.022*strike; p["head"][1]=0
            p["open"]=.05*jaw
        elif kind=="SpireWorker":
            p["shovel"]=1
            p["fore"]=Vector((.024*strike-.018*wind,0,.055*wind+.010*strike))
            p["toe_pitch"]=-.7*wind+.3*strike
        elif kind=="SpireSoldier":
            p["angles"][1]=-.07*wind+.11*strike
            p["head"][1]=-.10*wind+.10*strike
            p["body"].x=.034*strike-.02*wind
        if clip=="ClingBite":
            p["open"]=.38*jaw+.035*window(t,.44,.5,.75,.9)
            p["head"][0]=.055*math.sin(4*TAU*t)*window(t,.45,.5,.75,.95)
    elif clip in ("Burrow","Emerge","CrackDash"):
        # Emerge has its own probing and brace beat rather than reversing dig.
        if clip=="Burrow":
            dive=smooth((t-.36)/.64)
            dig=window(t,0,.12,.60,.9)
            p["root"].z=-spec["depth"]*dive
            p["angles"][1]=.23*window(t,.05,.3,.78,1)
            p["body"].z=-.018*window(t,0,.2,.65,1)
            p["head"][1]=.18*dig
        elif clip=="CrackDash":
            dive=smooth((t-.23)/.77); dig=math.sin(math.pi*t)**2
            p["root"].z=-spec["depth"]*dive
            p["root"].x=.16*dive
            p["angles"][1]=.30*math.sin(math.pi*t)
            p["dead"]=1
        else:
            rise=smooth((t-.07)/.64); dig=window(t,.2,.4,.7,.9)
            p["root"].z=-spec["depth"]*(1-rise)
            p["angles"][1]=(-.12 if kind=="MirrorSkimmer" else -.17)*window(t,.1,.3,.65,.9)
            p["head"][2]=.16*math.sin(TAU*t)*window(t,.55,.68,.85,1)
        p["cycles"]=t*3
        p["stride"]=spec["stride"]*.55*dig
        p["lift"]=spec["lift"]*1.2*dig
        p["shovel"]=1
        p["fore"]=Vector((.025*math.sin(3*TAU*t)*dig,0,.026*(1-math.cos(3*TAU*t))*dig))
        p["toe_pitch"]=-.5*math.sin(3*TAU*t)*dig
        p["open"]=.12*dig
    elif clip=="UndergroundIdle":
        p["root"].z=-spec["depth"]
        p["body"].z=.001*wave
    elif clip in ("Forage","Feed","SurfaceProbe","Gather","Repair","FeedColony","FeedFromWorker"):
        # Lowering is achieved through the body and neck while the feet brace.
        lower=.5-.5*math.cos(TAU*t)
        feed=window(t,0,.18,.82,1)
        p["body"].z=-.012*lower
        p["angles"][1]=.05*lower
        p["head"][1]=(.16 if kind=="Riftlip" else .12)*lower
        p["head"][2]=.055*math.sin(TAU*t)*feed if clip in ("Forage","SurfaceProbe") else 0
        p["open"]=(.24 if kind=="Riftlip" else .16)*(.5+.5*math.sin(3*TAU*t))*feed
        if clip in ("Gather","Repair","FeedColony"):
            p["shovel"]=1
            p["fore"]=Vector((.023*math.sin(3*TAU*t)*feed,-.018*feed,
                               (.035+.022*math.sin(3*TAU*t))*feed))
            p["toe_pitch"]=-.5*feed+.25*math.sin(3*TAU*t)*feed
        if clip=="Repair":
            p["fore"].z+=.043*feed
            p["head"][1]=-.10*feed
            p["open"]*=.3
        if clip in ("FeedFromWorker","FeedColony"):
            p["head"][1]=-.12*lower
            p["open"]*=.45
    elif clip in ("HitReact","Stagger","Recover"):
        amount=math.sin(math.pi*t)**2
        if clip=="Recover":
            amount=1-smooth(t)
        p["body"].y=-.022*amount
        p["body"].z=-.014*amount
        p["angles"][0]=.28*amount
        p["head"][2]=-.14*amount
        p["open"]=.10*amount
        p["dead"]=.75*amount
        if clip=="Stagger":
            p["angles"][0]+=.08*math.sin(3*TAU*t)*amount
            p["cycles"]=t; p["stride"]=spec["stride"]*.3*amount; p["lift"]=spec["lift"]*amount
    elif clip=="Death":
        death_params(kind,t,p)
    elif clip=="FlipRecover":
        # Begin on the side, kick, roll upright, then settle onto all six feet.
        upright=smooth((t-.27)/.48)
        p["angles"][0]=1.8*(1-upright)
        p["body"].z=.025*math.sin(math.pi*upright)
        p["cycles"]=t*3; p["stride"]=spec["stride"]*.4*(1-upright)
        p["lift"]=spec["lift"]*1.8*(1-upright)
        p["dead"]=1
    elif clip=="Bask":
        p["angles"][1]=.03*wave
        p["angles"][2]=.035*math.sin(TAU*t)
        p["body"].z=-.008+.002*wave
    elif clip=="Guard":
        p["head"][1]=.075+.01*wave
        p["head"][2]=.025*wave
        p["body"].z=-.007
    elif clip=="ChargeWindup":
        load=smooth(t)
        p["body"].x=-.022*load; p["body"].z=-.014*load
        p["angles"][1]=.07*load
        p["head"][1]=.10*load; p["open"]=.12*load
    elif clip=="ClampHold":
        p["angles"][1]=.06
        p["head"][1]=.08
        p["body"].x=.015+.002*wave
        p["open"]=.04+.006*wave
    if kind=="Riftlip":
        # A slow opening tests the surface, then the lips settle and draw a
        # short scraping stroke. This is distinct from the defensive snap.
        if clip=="Idle":
            p["open"]=.018*(.5-.5*math.cos(TAU*t))
            p["lip_roll"]=.025*math.sin(TAU*t)
        elif clip in ("Forage","Feed","SurfaceProbe"):
            contact=window(t,.08,.28,.73,.94)
            pulse=.5-.5*math.cos(3*TAU*t)
            p["open"]=(.27 if clip=="SurfaceProbe" else .22)*pulse*contact
            p["lip_roll"]=(.10*pulse-.085)*contact
            p["lip_pitch"]=.022*(1-pulse)*contact
            p["head"][1]=.145*(.5-.5*math.cos(TAU*t))
            p["body"].x=-.008*(1-pulse)*contact
        elif clip in ("Attack","ClingBite"):
            p["lip_roll"]=.11*window(t,.08,.25,.37,.50)
            p["lip_pitch"]=-.045*window(t,.08,.25,.37,.50)
        elif clip in ("Burrow","CrackDash"):
            p["open"]*=.35
            p["lip_roll"]=-.045*window(t,0,.20,.70,1)
    return p


def pose(rig, spec, p, t):
    rest=rig.data.bones
    root=Matrix.Translation(p["root"]) @ rot_at((0,0,0),(p["root_roll"],0,0))
    body=root @ Matrix.Translation(p["body"]) @ rot_at((0,0,spec["body_z"]),p["angles"])
    head=body @ Matrix.Translation(p.get("head_offset",Vector((0,0,0)))) @ rot_at(rest["head"].head_local,p["head"])
    abdomen=body @ Matrix.Translation(p.get("abdomen_offset",Vector((0,0,0)))) @ rot_at(rest["abdomen"].head_local,p["abdomen"])
    desired={"root":root@rest["root"].matrix_local,"body":body@rest["body"].matrix_local,
             "head":head@rest["head"].matrix_local,"abdomen":abdomen@rest["abdomen"].matrix_local}
    for side,sign in (("L",1),("R",-1)):
        name="mouth_"+side
        desired[name]=head@rot_at(rest[name].head_local,
            (sign*p["lip_roll"],p["lip_pitch"],sign*p["open"]))@rest[name].matrix_local
    error=0
    for leg in spec["legs"]:
        sign=1 if leg["label"][0]=="L" else -1
        index=int(leg["label"][1])-1
        offset=(0 if ((index+(0 if sign>0 else 1))%2==0) else .5)
        dx,lift=gait(p["cycles"]+offset,p["stride"],p["lift"])
        f=Vector(leg["f"])+Vector((dx,0,lift))
        if p["shovel"] and index==0:
            f+=Vector((p["fore"].x,sign*p["fore"].y,p["fore"].z))
        local_foot=f.copy()
        if "death_splay" in p:local_foot.y*=1+.60*p["death_splay"]
        if "death_curl" in p:
            tucked=Vector((leg["h"][0]-.012,leg["h"][1]*.58,spec["body_z"]-.012))
            curl=p["death_curl"]+p["death_kick"]*math.sin(t*30+index*1.7+sign)
            local_foot=local_foot.lerp(tucked,max(0,min(1,curl)))
        section=abdomen if p.get("curl_sections") and index==2 else body
        f=root@local_foot
        if p["dead"]:
            f=f.lerp(section@local_foot,p["dead"])
        h=section@Vector(leg["h"])
        knee,foot,err=solve_leg(leg,h,f)
        error=max(error,err)
        desired[leg["upper"]]=orient(rest[leg["upper"]],h,knee-h)
        desired[leg["lower"]]=orient(rest[leg["lower"]],knee,foot-knee)
        toe=Vector(leg["toe"])-Vector(leg["f"])
        if index==0 and p["shovel"]:
            toe=Euler((0,p["toe_pitch"],0)).to_matrix()@toe
        if "death_curl" in p:
            toe=toe.lerp(section.to_3x3()@toe,p["dead"])
        desired[leg["tip"]]=orient(rest[leg["tip"]],foot,toe)
    set_matrices(rig,desired)
    if p.get("grounded_death"):
        bpy.context.view_layer.update()
        mesh=bpy.data.objects["SK_"+rig["species"]].evaluated_get(bpy.context.evaluated_depsgraph_get())
        lowest=min(v.co.z for v in mesh.data.vertices)
        lift=Vector((0,0,.004+p.get("death_air",0)-lowest))
        for transform in desired.values():transform.translation+=lift
        set_matrices(rig,desired)
    return error


def bake(kind):
    rig=bpy.data.objects["RIG_"+kind]
    rig.animation_data_create()
    spec=json.loads(rig["motion_spec"])
    bpy.context.window.scene=bpy.data.scenes["JTS_MoonCreatures_V3"]
    for k in ("MirrorSkimmer","Riftlip","SpireWorker","SpireSoldier"):
        bpy.data.collections["CREATURE_"+k].hide_viewport=False
    clips=[]
    for clip,count,loop in clip_list(kind):
        name="A_"+kind+"_"+clip
        assert not bpy.data.actions.get(name), "Clip already exists: "+name
        action=bpy.data.actions.new(name)
        action.use_fake_user=True
        action.use_frame_range=True
        action.frame_start=1; action.frame_end=count; action.use_cyclic=loop
        rig.animation_data.action=action
        if len(action.slots):
            rig.animation_data.action_slot=action.slots[0]
        max_error=0
        for frame in range(1,count+1):
            t=(frame-1)/(count-1)
            p=params(kind,clip,t,spec)
            max_error=max(max_error,pose(rig,spec,p,t))
            for pb in rig.pose.bones:
                pb.keyframe_insert("location",frame=frame,group=pb.name)
                pb.keyframe_insert("rotation_quaternion",frame=frame,group=pb.name)
        # Dense baked samples should interpolate directly, with no bezier
        # overshoot at contacts, lips or loop boundaries.
        for layer in action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for fc in bag.fcurves:
                        for key in fc.keyframe_points:
                            key.interpolation="LINEAR"
        events=[]
        if clip in ("Attack","ClingBite"):
            events=[dict(frame=round(count*.42),name="Contact_PresentationOnly")]
        elif clip in ("Gather","Forage"):
            events=[dict(frame=round(count*.50),name="FoodContact_PresentationOnly")]
        elif clip=="Burrow":
            events=[dict(frame=round(count*.36),name="EnterSurface"),dict(frame=count,name="FullyHidden")]
        elif clip=="Emerge":
            events=[dict(frame=round(count*.4),name="BreakSurface"),dict(frame=count,name="FullyVisible")]
        for event in events:
            marker=action.pose_markers.new(event["name"]); marker.frame=event["frame"]
        desc=dict(name=clip,action=name,frames=count,fps=30,loop=loop,events=events,
                  vertical_root_motion=clip in ("Burrow","Emerge","CrackDash","UndergroundIdle"),
                  max_ik_reach_correction_m=round(max_error,6))
        if clip in ("Walk","Run","Carry","Charge"):
            multiplier=stride_multiplier(kind,clip)
            desc["ground_speed_cm_s"]=round(100*spec["stride"]*multiplier/(.62*(count-1)/30),3)
        action["clip_info"]=json.dumps(desc)
        action["authority"]="Animation only; contacts are hints. Server gameplay validates damage and food."
        if kind=="MirrorSkimmer" and clip=="Attack":
            action["behavior"]="Harmless defensive shell nudge; the species remains peaceful."
        track=rig.animation_data.nla_tracks.new(); track.name=clip
        strip=track.strips.new(name,1,action)
        strip.action_frame_start=1; strip.action_frame_end=count
        track.mute=True
        clips.append(desc)
    rig["clips"]=json.dumps(clips)
    idle=bpy.data.actions["A_"+kind+"_Idle"]
    rig.animation_data.action=idle
    rig.animation_data.action_slot=idle.slots[0]
    bpy.context.scene.frame_set(1)
    return dict(species=kind,clips=len(clips),worst_reach_correction_m=max(c["max_ik_reach_correction_m"] for c in clips))
