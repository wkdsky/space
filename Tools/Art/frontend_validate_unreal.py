"""Inspect the isolated art stage; never edits shipping maps."""
import unreal
import json
from pathlib import Path

ROOT=Path(unreal.Paths.project_dir()).resolve()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
REPORT={'actors':[]}
for actor in actors.get_all_level_actors():
    name=actor.get_actor_label()
    if name in ['FE_AvatarPreview','FE_TwinforkDisplay','FE_Sunset','FE_Atmosphere','FE_PostProcess','FE_Camera_Home']:
        center,extent=actor.get_actor_bounds(False)
        item={'name':name,'location':str(actor.get_actor_location()),'rotation':str(actor.get_actor_rotation()),'bounds':[str(center),str(extent)]}
        if isinstance(actor,unreal.SkeletalMeshActor):
            c=actor.skeletal_mesh_component
            item.update(mesh=str(c.get_editor_property('skeletal_mesh_asset')),scale=str(c.get_world_scale()),visible=c.is_visible(),animation=str(c.get_editor_property('animation_data')))
        REPORT['actors'].append(item)
(ROOT/'SourceArt/FrontEnd/editor_probe.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
home=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='FE_Camera_Home')
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).pilot_level_actor(home)
REPORT['asset_checks']=[]
for name in ['Explorer','Scout','Engineer','Survey','Tactical']:
    mesh=unreal.load_asset('/Game/Space/FrontEnd/Characters/SK_FE_'+name)
    lods=unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).get_lod_count(mesh)
    assert lods==3,(name,lods)
    skeleton=mesh.get_editor_property('skeleton')
    assert skeleton.get_path_name().startswith('/Game/Space/FrontEnd/')
    REPORT['asset_checks'].append({'suit':name,'lods':lods,'skeleton':skeleton.get_path_name()})
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
capture=actors.spawn_actor_from_class(unreal.SceneCapture2D,home.get_actor_location(),home.get_actor_rotation())
capture.set_actor_label('FE_FallbackCapture')
target=unreal.RenderingLibrary.create_render_target2d(world,1920,1080,unreal.TextureRenderTargetFormat.RTF_RGBA8)
capture.capture_component2d.set_editor_property('texture_target',target)
capture.capture_component2d.set_editor_property('fov_angle',home.camera_component.field_of_view)
capture.capture_component2d.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
capture.capture_component2d.capture_scene()
unreal.RenderingLibrary.export_render_target(world,target,str(ROOT/'SourceArt/FrontEnd/Textures'),'T_FE_FallbackBackground')
actors.destroy_actor(capture)
source=ROOT/'SourceArt/FrontEnd/Textures/T_FE_FallbackBackground'
if source.exists():source.replace(source.with_suffix('.png'))
REPORT['animation_api']=[n for n in dir(unreal.AnimationLibrary) if any(s in n for s in ['bone','track','pose'])]
avatar=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='FE_AvatarPreview')
REPORT['bone_locations']={b:str(avatar.skeletal_mesh_component.get_socket_location(b)) for b in ['Root','Body','Head','UpperArm_L','Foot_L']}
REPORT['anim_track_names']=list(map(str,unreal.AnimationLibrary.get_animation_track_names(unreal.load_asset('/Game/Space/FrontEnd/Animations/A_FE_Idle'))))
REPORT['anim_pose_api']=unreal.AnimationLibrary.get_bone_pose_for_time.__doc__
anim=unreal.load_asset('/Game/Space/FrontEnd/Animations/A_FE_Idle')
REPORT['bone_local_poses']={b:str(unreal.AnimationLibrary.get_bone_pose_for_time(anim,b,0,False)) for b in ['Root','Body','Head']}
(ROOT/'SourceArt/FrontEnd/art_validation_report.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
REPORT['success']=True
(ROOT/'SourceArt/FrontEnd/art_validation_report.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
