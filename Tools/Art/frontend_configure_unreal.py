"""Dispatch shipping front-end authoring/QA through Unreal MCP."""
import unreal
import json
import runpy
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
job_path=ROOT/'Saved/FrontEndJob.json'
job=json.loads(job_path.read_text()) if job_path.exists() else {'step':'probe'}
step=job['step']
if step=='probe':
    umg=unreal.get_default_object(unreal.UMGToolSet)
    REPORT={'umg':str(umg),'fonts':{n:unreal.load_asset('/Game/Space/FrontEnd/Fonts/'+n).get_class().get_name() for n in ['JTSUISans-Regular','JTSUISans-Bold']},'editor_methods':[n for n in dir(unreal.LevelEditorSubsystem) if 'play' in n], 'play_settings':str(unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings')))}
    REPORT['defaults']={}
    for path in ['/Game/Space/FrontEnd/Widgets/WBP_FE_Main','/Game/Space/FrontEnd/Widgets/WBP_FE_Lobby','/Game/Space/Blueprints/Modes/BP_EarthGameMode','/Game/Space/Blueprints/Modes/BP_SpaceWorldGameMode']:
        c=unreal.get_default_object(unreal.load_class(None,path+'.'+path.rsplit('/',1)[1]+'_C'))
        REPORT['defaults'][path]=str(c.get_editor_property('lobby')) if 'Widgets/' in path else str(c.get_editor_property('default_pawn_class'))
    pawn=unreal.get_default_object(unreal.load_class(None,'/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2.BP_JTSPlayer_Casual_2_C'))
    REPORT['avatar_catalog']=str(pawn.get_editor_property('avatar_catalog'))
elif step=='ui':REPORT=runpy.run_path(str(ROOT/'Tools/Art/frontend_build_ui_unreal.py')).get('REPORT',{})
elif step=='daylight_assets':REPORT=runpy.run_path(str(ROOT/'Tools/Art/frontend_daylight_unreal.py')).get('REPORT',{})
elif step=='presentation_polish':REPORT=runpy.run_path(str(ROOT/'Tools/Art/frontend_presentation_polish_unreal.py')).get('REPORT',{})
elif step=='polish':
    umg=unreal.get_default_object(unreal.UMGToolSet)
    changes={'Home_Sub':'1–4 人合作远征 · 集结队友，从地球出发','Lobby_Sub':'集结你的队员。所有人准备后，由房主带队出发。'}
    for name in ['WBP_FE_Main','WBP_FE_Lobby']:
        bp=unreal.load_asset('/Game/Space/FrontEnd/Widgets/'+name)
        for item in umg.call_method('GetWidgets',(bp,)).widgets:
            w=item.widget
            if str(item.widget_name) in changes:w.set_text(changes[str(item.widget_name)])
            elif str(item.widget_name)=='ProviderStatus':
                w.slot.set_position(unreal.Vector2D(1060 if name=='WBP_FE_Lobby' else 1530,36));w.slot.set_size(unreal.Vector2D(334,36))
            elif str(item.widget_name)=='CreditsBody':w.set_text(str(w.get_text()).replace('均通过 Blender / Unreal MCP 制作。','为本项目制作。').replace('完整来源、许可文本、修改记录和 SHA256：SourceArt/FrontEnd/asset_manifest.json','感谢所有开放素材作者。完整许可文本随游戏发行文件提供。'))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
    REPORT={'success':True}
elif step=='scene_probe':
    REPORT={'actors':[]}
    for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if any(s in a.get_actor_label() for s in ['Sky','Moon','Hill','MainPresentation']):
            item={'name':a.get_actor_label(),'location':str(a.get_actor_location()),'scale':str(a.get_actor_scale3d()),'bounds':str(a.get_actor_bounds(False))}
            if isinstance(a,unreal.StaticMeshActor):item.update(mesh=str(a.static_mesh_component.static_mesh),materials=[str(m) for m in a.static_mesh_component.get_materials()])
            REPORT['actors'].append(item)
elif step=='capture_fallback':
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    stage=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='FE_MainPresentation')
    camera=stage.get_editor_property('presentation_camera')
    capture=actors.spawn_actor_from_class(unreal.SceneCapture2D,camera.get_world_location(),camera.get_world_rotation())
    target=unreal.RenderingLibrary.create_render_target2d(world,1920,1080,unreal.TextureRenderTargetFormat.RTF_RGBA8)
    component=capture.capture_component2d;component.set_editor_property('texture_target',target)
    component.set_editor_property('fov_angle',camera.field_of_view);component.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    component.capture_scene()
    unreal.RenderingLibrary.export_render_target(world,target,str(ROOT/'SourceArt/FrontEnd/Textures'),'T_FE_FallbackBackground')
    actors.destroy_actor(capture)
    source=ROOT/'SourceArt/FrontEnd/Textures/T_FE_FallbackBackground'
    if source.exists():source.replace(source.with_suffix('.png'))
    task=unreal.AssetImportTask()
    for k,v in {'filename':str(source.with_suffix('.png')),'destination_path':'/Game/Space/FrontEnd/Textures','destination_name':'T_FE_FallbackBackground','automated':True,'replace_existing':True,'save':True}.items():task.set_editor_property(k,v)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    REPORT={'success':True,'file':'Textures/T_FE_FallbackBackground.png'}
elif step in ['entry','lobby']:REPORT=runpy.run_path(str(ROOT/'Tools/Art/frontend_maps_unreal.py')).get('REPORT',{})
elif step=='player':
    bp=unreal.load_asset('/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2')
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cls=unreal.load_class(None,'/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2.BP_JTSPlayer_Casual_2_C')
    cdo=unreal.get_default_object(cls)
    cdo.set_editor_property('avatar_catalog',unreal.load_asset('/Game/Space/FrontEnd/Data/DA_FE_AvatarCatalog'))
    component=cdo.get_component_by_class(unreal.load_class(None,'/Script/space.JTSAvatarAppearanceComponent'))
    assert component
    component.set_editor_property('catalog',unreal.load_asset('/Game/Space/FrontEnd/Data/DA_FE_AvatarCatalog'))
    subobjects=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library=unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subobjects.k2_gather_subobject_data_for_blueprint(bp):
        data=library.get_data(handle)
        obj=library.get_object_for_blueprint(data,bp)
        if obj and obj.get_class().get_name()=='JTSAvatarAppearanceComponent':
            obj.set_editor_property('catalog',component.get_editor_property('catalog'))
    cdo.mesh.set_skeletal_mesh_asset(unreal.load_asset('/Game/Space/FrontEnd/Characters/SK_FE_Explorer'))
    assert unreal.EditorAssetLibrary.save_asset('/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2',only_if_is_dirty=False)
    REPORT={'success':True,'pawn':cls.get_path_name()}
elif step in ['load_entry','load_lobby']:
    path='/Game/Space/FrontEnd/Maps/'+('L_Entry' if step=='load_entry' else 'L_PreLaunchLobby')
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_outermost().get_name()!=path:assert levels.load_level(path)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
    settings.set_editor_property('PlayNumberOfClients',job.get('clients',1))
    settings.set_editor_property('RunUnderOneProcess',True)
    from editor_toolset.toolsets.object import ObjectTools
    assert ObjectTools.set_properties(settings,json.dumps({'PlayNetMode':'PIE_ListenServer' if job.get('clients',1)>1 else 'PIE_Standalone','NewWindowWidth':job.get('width',960),'NewWindowHeight':job.get('height',540)}))
    # PIE auto-connects without service credentials. Restore the shipping value
    # when returning to the one-player entry preview, before any package save.
    cdo=unreal.get_default_object(unreal.load_class(None,'/Game/Space/FrontEnd/Blueprints/BP_FE_LobbyGameMode.BP_FE_LobbyGameMode_C'))
    cdo.set_editor_property('require_matching_build_version',job.get('clients',1)==1)
    unreal.SystemLibrary.execute_console_command(world,'net.AllowPIESeamlessTravel 1')
    REPORT={'success':True,'map':path,'clients':job.get('clients',1)}
elif step=='video_qa':
    video=unreal.GameUserSettings.get_game_user_settings()
    groups=['view_distance','shadow','global_illumination','reflection','anti_aliasing','texture','visual_effect','post_processing','foliage','shading','landscape']
    if job.get('setup'):
        assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
        for group,value in zip(groups,[3,1,2,1,3,2,1,2,0,3,1]):getattr(video,'set_'+group+'_quality')(value)
        video.set_resolution_scale_value_ex(73)
        video.apply_non_resolution_settings()
    REPORT={'success':True,'quality':{group:getattr(video,'get_'+group+'_quality')() for group in groups},
            'resolution_scale':video.get_resolution_scale_information_ex()[1],
            'resolution':{'x':video.get_screen_resolution().x,'y':video.get_screen_resolution().y},
            'mode':str(video.get_fullscreen_mode()),'vsync':video.is_v_sync_enabled(),'fps':video.get_frame_rate_limit(),
            'overall':video.get_overall_scalability_level()}
elif step=='finalize':
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE before finalizing'
    path='/Game/Space/FrontEnd/Blueprints/BP_FE_LobbyGameMode'
    cdo=unreal.get_default_object(unreal.load_class(None,path+'.BP_FE_LobbyGameMode_C'))
    cdo.set_editor_property('require_matching_build_version',True)
    assert unreal.EditorAssetLibrary.save_asset(path,only_if_is_dirty=False)
    REPORT={'success':True,'require_matching_build_version':bool(cdo.get_editor_property('require_matching_build_version')),
            'world':unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name(),
            'dirty_content':[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()],
            'dirty_maps':[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]}
else:REPORT=runpy.run_path(str(ROOT/'Tools/Art/frontend_runtime_qa_unreal.py')).get('REPORT',{})
(ROOT/'SourceArt/FrontEnd/configure_report.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
