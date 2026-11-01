"""Export, inspect and preview the original Moon creature asset set."""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Vector, Matrix
from moon_creatures_geometry import KINDS, SCENE, material

ROOT=Path("D:/projects/space/SourceArt/MoonCreaturesV3")
QA=Path("D:/projects/space/Saved/MoonCreaturesV3")


def show(kind,clip="Idle",frame=1):
    scene=bpy.data.scenes[SCENE]
    bpy.context.window.scene=scene
    for k in KINDS:
        col=bpy.data.collections["CREATURE_"+k]
        col.hide_viewport=(k!=kind)
        col.hide_render=(k!=kind)
    rig=bpy.data.objects["RIG_"+kind]
    rig.location=(0,0,0)
    a=bpy.data.actions["A_"+kind+"_"+clip]
    rig.animation_data.action=a
    rig.animation_data.action_slot=a.slots[0]
    scene.frame_start=1;scene.frame_end=round(a.frame_range[1])
    scene.frame_set(frame)
    return scene,rig


def stage():
    scene=bpy.data.scenes[SCENE]
    bpy.context.window.scene=scene
    col=bpy.data.collections.get("STUDIO_MoonCreatures")
    if col:
        return col
    col=bpy.data.collections.new("STUDIO_MoonCreatures")
    scene.collection.children.link(col)
    world=bpy.data.worlds.new("MoonCreatureStudioWorld")
    world.use_nodes=True
    bg=next(n for n in world.node_tree.nodes if n.type=="BACKGROUND")
    bg.inputs["Color"].default_value=(.20,.23,.29,1)
    bg.inputs["Strength"].default_value=.45
    scene.world=world
    mesh=bpy.data.meshes.new("PreviewFloor")
    mesh.from_pydata([(-200,-200,-.004),(200,-200,-.004),(200,200,-.004),(-200,200,-.004)],[],[(0,1,2,3)])
    obj=bpy.data.objects.new("STUDIO_Floor",mesh)
    col.objects.link(obj)
    mesh.materials.append(material("M_MC_StudioFloor",(.16,.19,.24),.82))
    camera=bpy.data.cameras.new("MoonCreatureCamera")
    cam=bpy.data.objects.new("STUDIO_Camera",camera)
    col.objects.link(cam)
    cam.location=(.9,-1.25,.92)
    cam.rotation_euler=(Vector((.01,0,.10))-cam.location).to_track_quat("-Z","Y").to_euler()
    camera.type="ORTHO";camera.ortho_scale=.88
    camera.clip_start=.005;camera.clip_end=1000
    scene.camera=cam
    for name,loc,energy,size,color in (
        ("Key",(.35,-.5,1.4),25,1.1,(1,.91,.79)),
        ("Fill",(-.65,-.3,.65),18,1.0,(.72,.84,1)),
        ("Rim",(.1,.7,1.0),32,.8,(.87,.92,1))):
        light=bpy.data.lights.new("MC_"+name,"AREA")
        light.energy=energy;light.shape="DISK";light.size=size;light.color=color
        ob=bpy.data.objects.new("STUDIO_"+name,light);col.objects.link(ob)
        ob.location=loc
        ob.rotation_euler=(Vector((0,0,.08))-ob.location).to_track_quat("-Z","Y").to_euler()
    scene.render.resolution_x=960;scene.render.resolution_y=800
    scene.render.resolution_percentage=100
    scene.render.image_settings.file_format="PNG"
    scene.render.film_transparent=False
    return col


def portrait(kind,clip="Idle",frame=1,suffix=""):
    stage()
    scene,rig=show(kind,clip,frame)
    out=ROOT/"Previews";out.mkdir(parents=True,exist_ok=True)
    scene.render.filepath=str(out/(kind+suffix+".png"))
    bpy.ops.render.render(write_still=True)
    return scene.render.filepath


def curves(action):
    return [fc for layer in action.layers for strip in layer.strips
            for bag in strip.channelbags for fc in bag.fcurves]


def validate(kind):
    scene,rig=show(kind)
    mesh=bpy.data.objects["SK_"+kind]
    bones=set(rig.data.bones.keys())
    errors=[]
    unweighted=0;bad_weights=0
    for vertex in mesh.data.vertices:
        groups=[g for g in vertex.groups if mesh.vertex_groups[g.group].name in bones]
        if not groups:unweighted+=1
        if abs(sum(g.weight for g in groups)-1)>1e-5:bad_weights+=1
    if unweighted or bad_weights:errors.append("Invalid deformation weights")
    bm=bmesh.new();bm.from_mesh(mesh.data)
    topology=dict(boundary_edges=sum(e.is_boundary for e in bm.edges),
                  nonmanifold_edges=sum(not e.is_manifold for e in bm.edges),
                  degenerate_faces=sum(f.calc_area()<1e-10 for f in bm.faces),
                  signed_volume=bm.calc_volume(signed=True))
    bm.free()
    if any(topology[k] for k in ("boundary_edges","nonmanifold_edges","degenerate_faces")):
        errors.append("Mesh topology is not closed and nondegenerate")
    clips=json.loads(rig["clips"])
    loop_errors=[]
    extrema=[]
    dg=bpy.context.evaluated_depsgraph_get()
    for clip in clips:
        action=bpy.data.actions[clip["action"]]
        rig.animation_data.action=action;rig.animation_data.action_slot=action.slots[0]
        fcs=curves(action)
        if not fcs:errors.append("Empty action "+action.name)
        if clip["loop"]:
            diff=max((abs(fc.evaluate(1)-fc.evaluate(clip["frames"])) for fc in fcs),default=0)
            if diff>1e-5:loop_errors.append(dict(clip=clip["name"],difference=diff))
        clip_bounds=[]
        for frame in sorted(set(round(1+(clip["frames"]-1)*i/6) for i in range(7))):
            scene.frame_set(frame)
            evaluated=mesh.evaluated_get(dg)
            data=evaluated.to_mesh()
            coords=[v.co for v in data.vertices]
            low=[min(v[i] for v in coords) for i in range(3)]
            high=[max(v[i] for v in coords) for i in range(3)]
            evaluated.to_mesh_clear()
            if not all(math.isfinite(x) for x in low+high):errors.append("Nonfinite pose "+action.name)
            if max(high[i]-low[i] for i in range(3))>1.2:errors.append("Exploded pose "+action.name)
            clip_bounds.append(dict(frame=frame,min=low,max=high))
        extrema.append(dict(clip=clip["name"],samples=clip_bounds))
    if loop_errors:errors.append("Loop discontinuity")
    for required in ("Idle","Walk","Run","Attack","Burrow","Emerge","Forage","Feed","HitReact","Death"):
        if required not in {c["name"] for c in clips}:errors.append("Missing "+required)
    show(kind)
    result=dict(species=kind,vertices=len(mesh.data.vertices),triangles=len(mesh.data.polygons),
                bones=len(bones),unweighted_vertices=unweighted,invalid_weight_sums=bad_weights,
                topology=topology,clip_count=len(clips),loop_errors=loop_errors,
                max_ik_reach_correction_m=max(c["max_ik_reach_correction_m"] for c in clips),
                sampled_deformation_bounds=extrema,errors=errors,passed=not errors)
    QA.mkdir(parents=True,exist_ok=True)
    (QA/(kind+"_validation.json")).write_text(json.dumps(result,indent=2),encoding="utf-8")
    return {k:v for k,v in result.items() if k!="sampled_deformation_bounds"}


def export_fbx_centimetres(kind):
    """Bake metre-to-centimetre conversion into disposable geometry and bone keys.

    UE removes Blender's Armature wrapper. Keeping scale in that wrapper loses
    the unit conversion on animation-only FBXs, even when the mesh imports well.
    """
    scene,source_rig=show(kind)
    source_mesh=bpy.data.objects['SK_'+kind]
    rig=source_rig.copy();rig.data=source_rig.data.copy();rig.animation_data_clear()
    mesh=source_mesh.copy();mesh.data=source_mesh.data.copy()
    scene.collection.objects.link(rig);scene.collection.objects.link(mesh)
    original_units=scene.unit_settings.scale_length
    copies=[]
    try:
        assert not bpy.data.objects.get('Armature')
        rig.name='Armature'
        rig.data.transform(Matrix.Scale(100,4));mesh.data.transform(Matrix.Scale(100,4))
        if mesh.parent==source_rig:mesh.parent=rig
        for modifier in mesh.modifiers:
            if modifier.type=='ARMATURE':modifier.object=rig
        for bone in rig.pose.bones:
            bone.location=(0,0,0);bone.rotation_quaternion=(1,0,0,0);bone.scale=(1,1,1)
        scene.unit_settings.scale_length=.01
        bpy.ops.object.select_all(action='DESELECT')
        rig.select_set(True);mesh.select_set(True);bpy.context.view_layer.objects.active=rig
        out=ROOT/kind;out.mkdir(parents=True,exist_ok=True)
        base=dict(use_selection=True,add_leaf_bones=False,use_armature_deform_only=True,
            axis_forward='-Y',axis_up='Z',global_scale=1,apply_unit_scale=True,
            apply_scale_options='FBX_SCALE_UNITS',use_mesh_modifiers=True,mesh_smooth_type='FACE',path_mode='AUTO')
        bpy.ops.export_scene.fbx(filepath=str(out/(source_mesh.name+'.fbx')),bake_anim=False,object_types={'MESH','ARMATURE'},**base)
        clips=json.loads(source_rig['clips'])
        for clip in clips:
            source=bpy.data.actions[clip['action']];action=source.copy();copies.append(action)
            for layer in action.layers:
                for strip in layer.strips:
                    bag=strip.channelbag(action.slots[0])
                    if not bag:continue
                    for curve in bag.fcurves:
                        if not curve.data_path.endswith('.location'):continue
                        for key in curve.keyframe_points:
                            key.co.y*=100;key.handle_left.y*=100;key.handle_right.y*=100
            rig.animation_data_create();rig.animation_data.action=action;rig.animation_data.action_slot=action.slots[0]
            scene.frame_start=1;scene.frame_end=clip['frames'];scene.frame_set(1)
            bpy.ops.export_scene.fbx(filepath=str(out/(source.name+'.fbx')),object_types={'ARMATURE'},
                bake_anim=True,bake_anim_use_all_bones=True,bake_anim_use_nla_strips=False,bake_anim_use_all_actions=False,
                bake_anim_force_startend_keying=True,bake_anim_step=1,bake_anim_simplify_factor=0,**base)
        return dict(species=kind,fbx_files=1+len(clips),unit='centimetres baked into geometry and keys')
    finally:
        scene.unit_settings.scale_length=original_units
        mesh_data=mesh.data;rig_data=rig.data
        bpy.data.objects.remove(mesh,do_unlink=True);bpy.data.objects.remove(rig,do_unlink=True)
        bpy.data.meshes.remove(mesh_data);bpy.data.armatures.remove(rig_data)
        for action in copies:bpy.data.actions.remove(action)
        show(kind)


def export(kind):
    export_fbx_centimetres(kind)
    scene,rig=show(kind)
    mesh=bpy.data.objects["SK_"+kind]
    out=ROOT/kind;out.mkdir(parents=True,exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True);mesh.select_set(True);bpy.context.view_layer.objects.active=rig
    clips=json.loads(rig["clips"])
    # GLB packages all clips and embedded materials for portable inspection.
    bpy.ops.export_scene.gltf(filepath=str(out/(kind+".glb")),export_format="GLB",use_selection=True,
        export_animations=True,export_animation_mode="ACTIONS",export_skins=True,
        export_anim_single_armature=False,
        export_force_sampling=True,export_frame_range=False,export_yup=True,
        export_def_bones=True,export_optimize_animation_size=False)
    return dict(species=kind,fbx_files=1+len(clips),glb=str(out/(kind+".glb")))


def save_source():
    # Use the normal writer: Blender 5.2.1's partial library writer can crash
    # while copying a newly created scene's view layers.
    stage()
    scene=bpy.data.scenes[SCENE]
    bpy.context.window.scene=scene
    scene["motion_notes"]="30 fps baked Actions. Walk/Run are in place. Burrow/Emerge contain vertical root displacement."
    scene["gameplay_notes"]="Animation contacts do not apply damage; server gameplay remains authoritative."
    ROOT.mkdir(parents=True,exist_ok=True)
    target=ROOT/"MoonCreaturesV3.blend"
    preserve=bool(bpy.data.filepath and Path(bpy.data.filepath).resolve()!=target.resolve())
    bpy.ops.wm.save_as_mainfile(filepath=str(target),copy=preserve,compress=True)
    return str(ROOT/"MoonCreaturesV3.blend")


def manifest():
    data=dict(version=3,coordinate_system="Blender metres; +X forward; +Z surface normal",fps=30,
              provenance="Original geometry, rig and animation authored in Blender through MCP. No third-party meshes embedded.",
              creatures=[])
    for kind in KINDS:
        rig=bpy.data.objects["RIG_"+kind];mesh=bpy.data.objects["SK_"+kind]
        data["creatures"].append(dict(species=kind,mesh=mesh.name,bones=len(rig.data.bones),
            triangles=len(mesh.data.polygons),clips=json.loads(rig["clips"]),
            mesh_fbx=kind+"/"+mesh.name+".fbx",blend="MoonCreaturesV3.blend",glb=kind+"/"+kind+".glb"))
    ROOT.mkdir(parents=True,exist_ok=True)
    (ROOT/"manifest.json").write_text(json.dumps(data,indent=2),encoding="utf-8")
    return dict(creatures=len(data["creatures"]),animations=sum(len(c["clips"]) for c in data["creatures"]))


REEL_COMMON=("Idle","Walk","Run","Attack","Forage","Burrow","Emerge","HitReact")
REEL_EXTRA={"MirrorSkimmer":("Bask","FlipRecover"),"Riftlip":("SurfaceProbe","CrackDash"),
            "SpireWorker":("Repair","Carry"),"SpireSoldier":("Charge","Stagger")}


def gallery():
    stage()
    scene=bpy.data.scenes[SCENE]
    bpy.context.window.scene=scene
    camera=scene.camera
    camera.location=(2,-3,2.2)
    camera.rotation_euler=(Vector((0,0,.10))-camera.location).to_track_quat("-Z","Y").to_euler()
    camera.data.ortho_scale=1.93
    r=camera.rotation_euler.to_matrix()
    projection=Matrix(((r[0][0],r[1][0]),(r[0][1],r[1][1]))).inverted()
    centers=((- .47,.36),(.47,.36),(-.47,-.36),(.47,-.36))
    for kind,point in zip(KINDS,centers):
        col=bpy.data.collections["CREATURE_"+kind]
        col.hide_viewport=False;col.hide_render=False
        rig=bpy.data.objects["RIG_"+kind]
        xy=projection@Vector(point)
        rig.location=(xy.x,xy.y,0)
        action=bpy.data.actions["A_"+kind+"_Idle"]
        rig.animation_data.action=action;rig.animation_data.action_slot=action.slots[0]
    scene.render.resolution_x=1100;scene.render.resolution_y=900
    scene.render.resolution_percentage=100
    scene.frame_set(1)
    return scene


def reel_chunk(start,end,frames_per_section=36):
    scene=gallery()
    scene.render.resolution_x=1000;scene.render.resolution_y=820
    scene.eevee.taa_render_samples=16
    raw=QA/"ReelRaw";raw.mkdir(parents=True,exist_ok=True)
    for kind in KINDS:
        bpy.data.objects["RIG_"+kind].animation_data.action=None
    for index in range(start,end):
        section=index//frames_per_section
        local=index%frames_per_section
        for kind in KINDS:
            clip=REEL_COMMON[section] if section<len(REEL_COMMON) else REEL_EXTRA[kind][section-len(REEL_COMMON)]
            action=bpy.data.actions["A_"+kind+"_"+clip]
            info=json.loads(action["clip_info"])
            if info["loop"]:
                frame=1+(local/18*30)%(info["frames"]-1)
            else:
                frame=1+local/(frames_per_section-1)*(info["frames"]-1)
            rig=bpy.data.objects["RIG_"+kind]
            for fc in curves(action):
                value=rig.path_resolve(fc.data_path)
                value[fc.array_index]=fc.evaluate(frame)
        bpy.context.view_layer.update()
        scene.render.filepath=str(raw/f"frame_{index:04}.png")
        bpy.ops.render.render(write_still=True)
    return dict(rendered=end-start,total_done=end,total=frames_per_section*10)


def finish_gallery():
    # Imported QA data has no users; remove it without touching authored clips.
    for action in list(bpy.data.actions):
        if not action.name.startswith("A_") and action.users==0:
            bpy.data.actions.remove(action)
    scene=gallery()
    scene.eevee.taa_render_samples=64
    scene.render.filepath=str(ROOT/"Previews"/"Gallery.png")
    bpy.ops.render.render(write_still=True)
    return save_source()


def riftlip_review_camera():
    stage()
    scene,rig=show("Riftlip")
    camera=scene.camera
    camera.location=(.90,-.80,.74)
    camera.rotation_euler=(Vector((.16,0,.088))-camera.location).to_track_quat("-Z","Y").to_euler()
    camera.data.ortho_scale=.44
    scene.render.resolution_x=1000;scene.render.resolution_y=800
    scene.render.resolution_percentage=100
    return scene


def riftlip_review_stills():
    scene=riftlip_review_camera()
    scene.eevee.taa_render_samples=64
    paths=[]
    for clip,frame,suffix in (("Idle",1,"_Head"),("Attack",12,"_Open"),("SurfaceProbe",37,"_Feed")):
        paths.append(portrait("Riftlip",clip,frame,suffix))
    camera=scene.camera
    camera.location=(.10,0,1.2)
    camera.rotation_euler=(Vector((.10,0,.08))-camera.location).to_track_quat("-Z","Y").to_euler()
    camera.data.ortho_scale=.69
    paths.append(portrait("Riftlip",suffix="_Top"))
    camera.location=(.9,-1.25,.92)
    camera.rotation_euler=(Vector((.01,0,.10))-camera.location).to_track_quat("-Z","Y").to_euler()
    camera.data.ortho_scale=.82
    scene.render.resolution_x=960;scene.render.resolution_y=800
    paths.append(portrait("Riftlip"))
    return paths


RIFTLIP_REVIEW_CLIPS=("Idle","SurfaceProbe","Feed","ClingBite")


def riftlip_reel_chunk(start,end):
    scene=riftlip_review_camera()
    scene.eevee.taa_render_samples=24
    raw=QA/"RiftlipCloseupRaw";raw.mkdir(parents=True,exist_ok=True)
    rig=bpy.data.objects["RIG_Riftlip"]
    rig.animation_data.action=None
    for index in range(start,end):
        section=index//60;local=index%60
        action=bpy.data.actions["A_Riftlip_"+RIFTLIP_REVIEW_CLIPS[section]]
        info=json.loads(action["clip_info"])
        elapsed=local*30/20
        frame=1+(elapsed%(info["frames"]-1) if info["loop"] else min(elapsed,info["frames"]-1))
        for fc in curves(action):
            rig.path_resolve(fc.data_path)[fc.array_index]=fc.evaluate(frame)
        bpy.context.view_layer.update()
        scene.render.filepath=str(raw/f"frame_{index:04}.png")
        bpy.ops.render.render(write_still=True)
    return dict(rendered=end-start,total_done=end,total=240)
