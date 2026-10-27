"""Configure original shipping maps only after the complete asset-first gate."""
import unreal
import json
import gc
from pathlib import Path

ROOT=Path(unreal.Paths.project_dir()).resolve()
EA=unreal.EditorAssetLibrary
ACTORS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LEVELS=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
DEST='/Game/Space/FrontEnd'
assert json.loads((ROOT/'SourceArt/FrontEnd/asset_gate.json').read_text())['passed']
job=json.loads((ROOT/'Saved/FrontEndJob.json').read_text())
kind=job['step']

def configure(kind):
    target=DEST+'/Maps/'+('L_Entry' if kind=='entry' else 'L_PreLaunchLobby')
    # The editor template API owns world duplication/context transfer and saves the new map.
    # Asset-library duplication followed by LoadLevel leaves a standalone world in UE 5.8.
    current=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_outermost().get_name()
    if not EA.does_asset_exist(target):assert LEVELS.new_level_from_template(target,DEST+'/Maps/L_FrontEndArtLab')
    elif current!=target:assert LEVELS.load_level(target)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    gm=unreal.load_class(None,'/Script/space.JTSMainMenuGameMode')
    if kind=='lobby':
        gm_path=DEST+'/Blueprints/BP_FE_LobbyGameMode'
        bp=unreal.load_asset(gm_path)
        if not bp:
            factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',unreal.load_class(None,'/Script/space.JTSPreLaunchLobbyGameMode'))
            bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_FE_LobbyGameMode',DEST+'/Blueprints',unreal.Blueprint,factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        cdo=unreal.get_default_object(bp.generated_class())
        cdo.set_editor_property('require_matching_build_version',True)
        cdo.set_editor_property('default_pawn_class',unreal.load_class(None,'/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2.BP_JTSPlayer_Casual_2_C'))
        EA.save_loaded_asset(bp,only_if_is_dirty=False)
        gm=bp.generated_class()
    world.get_world_settings().set_editor_property('default_game_mode',gm)
    cameras={a.get_actor_label()[10:]:a for a in ACTORS.get_all_level_actors() if a.get_actor_label().startswith('FE_Camera_')}
    views={}
    for name,c in cameras.items():
        view=unreal.JTSMenuCameraView()
        view.set_editor_property('transform',c.get_actor_transform())
        view.set_editor_property('fov',c.camera_component.field_of_view)
        views[name]=view
    # Preserve editor-authored views when rebuilding an already configured map.
    if not views:
        for a in ACTORS.get_all_level_actors():
            if a.get_actor_label()=='FE_MainPresentation':views=dict(a.get_editor_property('views'))
            elif a.get_actor_label()=='FE_LobbyPresentation':
                c=a.get_editor_property('lobby_camera')
                view=unreal.JTSMenuCameraView();view.set_editor_property('transform',c.get_relative_transform());view.set_editor_property('fov',c.field_of_view)
                views['Crew']=view
    assert views,'Presentation cameras are missing from the template/map'
    if kind=='entry':
        for name,pos,look_at,fov in [('Head',(45,-340,180),(-5,-80,162),35),('Full',(150,-460,180),(20,-80,100),44)]:
            view=unreal.JTSMenuCameraView()
            view.set_editor_property('transform',unreal.Transform(location=unreal.Vector(*pos),rotation=unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*pos),unreal.Vector(*look_at))))
            view.set_editor_property('fov',fov);views[name]=view
    for a in list(ACTORS.get_all_level_actors()):
        if a.get_actor_label() in ['FE_LobbyPresentation','FE_MainPresentation']:
            ACTORS.destroy_actor(a);continue
        if a.get_actor_label()=='FE_AvatarPreview':ACTORS.destroy_actor(a)
        elif isinstance(a,unreal.AmbientSound):
            a.set_editor_property('tags',['FrontEndAudio','Music'] if 'Music' in a.get_actor_label() else ['FrontEndAudio'])
        elif isinstance(a,unreal.LevelSequenceActor):a.set_editor_property('tags',['FrontEndLoop'])
        if a.get_actor_label()=='FE_TwinforkDisplay':
            a.set_actor_location(unreal.Vector(-350,1050,130),False,False)
            a.set_actor_scale3d(unreal.Vector(.38,.38,.38))
            a.set_actor_rotation(unreal.Rotator(yaw=45),False)
        elif a.get_actor_label()=='FE_Moon':
            # Source sphere is one metre; Unreal imports it at 100 cm.
            a.set_actor_scale3d(unreal.Vector(6.5,6.5,6.5))
        elif a.get_actor_label()=='FE_PostProcess':
            pp=a.get_editor_property('settings');pp.set_editor_property('auto_exposure_bias',-6.5);a.set_editor_property('settings',pp)
    actor_by_label={a.get_actor_label():a for a in ACTORS.get_all_level_actors()}
    if 'FE_SkyDome' not in actor_by_label:
        sky=ACTORS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector())
        sky.set_actor_label('FE_SkyDome');actor_by_label['FE_SkyDome']=sky
    sky=actor_by_label['FE_SkyDome'];sky.set_actor_scale3d(unreal.Vector(20000,20000,20000))
    sky.static_mesh_component.set_static_mesh(unreal.load_asset(DEST+'/Models/SM_FE_Moon'))
    sky.static_mesh_component.set_material(0,unreal.load_asset(DEST+'/Materials/M_FE_Sky'))
    sky.static_mesh_component.set_cast_shadow(False);sky.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    for part in ['Core','Halo','FrontRings']:
        label='FE_AlienThreat_'+part;a=actor_by_label.get(label)
        if not a:a=ACTORS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector());a.set_actor_label(label)
        a.set_actor_location(unreal.Vector(-100000,400000,25000),False,False)
        a.set_actor_scale3d(unreal.Vector(80,80,80));a.set_actor_rotation(unreal.Rotator(),False)
        a.static_mesh_component.set_static_mesh(unreal.load_asset(DEST+'/Models/SM_FE_Threat'+part))
        a.static_mesh_component.set_material(0,unreal.load_asset(DEST+'/Materials/M_FE_Threat'+part))
        a.static_mesh_component.set_cast_shadow(False);a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        a.set_folder_path('FrontEnd/DistantThreat')
    for a in actor_by_label.values():
        if a.get_actor_label()=='FE_Sunset':
            a.set_actor_rotation(unreal.Rotator(pitch=-42,yaw=-25),False)
            a.light_component.set_editor_property('intensity',1000)
            a.light_component.set_light_color(unreal.LinearColor(1,.97,.90))
            a.light_component.set_editor_property('light_source_angle',3)
        elif a.get_actor_label().startswith('FE_Light_'):
            levels={'Key':12000,'Rim':4400,'Fill':8000,'HangarWarm':2200}
            a.light_component.set_editor_property('intensity',levels.get(a.get_actor_label()[9:],2400))
            if a.get_actor_label()=='FE_Light_HangarWarm':a.light_component.set_light_color(unreal.LinearColor(1,.90,.72))
            elif a.get_actor_label() in ['FE_Light_Key','FE_Light_Fill']:a.light_component.set_light_color(unreal.LinearColor(.90,.96,1))
    for a in actor_by_label.values():
        if not isinstance(a,unreal.LevelSequenceActor):continue
        sequence=a.get_sequence()
        overrides=[]
        for binding in sequence.get_bindings():
            label=str(binding.get_display_name())
            if label in actor_by_label:
                # SetBinding invalidates an uninitialized editor player's evaluation
                # state in UE 5.8. Author the serialized overrides directly instead.
                override=unreal.MovieSceneBindingOverrideData()
                override.set_editor_property('object_binding_id',sequence.get_binding_id(binding))
                override.set_editor_property('object',actor_by_label[label])
                override.set_editor_property('overrides_default',True)
                overrides.append(override)
            if label=='FE_MaintenanceRobot' and EA.get_metadata_tag(sequence,'JTS.LoopRouteVersion')!='1':
                for track in binding.get_tracks():
                    for section in track.get_sections():
                        channels=section.get_all_channels()
                        for key in channels[0].get_keys():key.set_value(-key.get_value())
                        for key in channels[1].get_keys():key.set_value(key.get_value()+440)
        a.get_editor_property('binding_overrides').set_editor_property('binding_data',overrides)
        EA.set_metadata_tag(sequence,'JTS.LoopRouteVersion','1');EA.save_loaded_asset(sequence,only_if_is_dirty=False)
    if kind=='entry':
        stage=ACTORS.spawn_actor_from_class(unreal.load_class(None,'/Script/space.JTSEntryPresentationStage'),unreal.Vector())
        stage.set_actor_label('FE_MainPresentation')
        stage.set_editor_property('views',views)
        component=stage.get_editor_property('appearance_component')
        component.set_editor_property('catalog',unreal.load_asset(DEST+'/Data/DA_FE_AvatarCatalog'))
        m=stage.get_editor_property('preview_mesh')
        m.set_relative_location(unreal.Vector(-50,-80,0),False,False)
        m.set_relative_rotation(unreal.Rotator(yaw=200),False,False)
        m.set_skeletal_mesh_asset(unreal.load_asset(DEST+'/Characters/SK_FE_Explorer'))
        m.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        data=m.get_editor_property('animation_data');data.set_editor_property('anim_to_play',unreal.load_asset(DEST+'/Animations/A_FE_Idle'));data.set_editor_property('saved_looping',True);m.set_editor_property('animation_data',data)
        stage.show_view('Home')
    else:
        stage=ACTORS.spawn_actor_from_class(unreal.load_class(None,'/Script/space.JTSPreLaunchLobbyStage'),unreal.Vector())
        stage.set_actor_label('FE_LobbyPresentation')
        # Character capsules remain unchanged. Pads place the mesh feet on the hangar floor.
        stage.set_editor_property('presentation_offsets',[unreal.Vector(240-i*160,-80,98) for i in range(4)])
        stage.set_editor_property('presentation_rotation',unreal.Rotator(yaw=-70))
        c=stage.get_editor_property('lobby_camera');c.set_relative_transform(views['Crew'].get_editor_property('transform'),False,False);c.set_field_of_view(views['Crew'].get_editor_property('fov'))
    for c in cameras.values():ACTORS.destroy_actor(c)
    assert LEVELS.save_current_level()
    return {'success':True,'map':target,'actors':len(ACTORS.get_all_level_actors())}

REPORT=configure(kind)
import runpy
REPORT['presentation_polish']=runpy.run_path(str(ROOT/'Tools/Art/frontend_presentation_polish_unreal.py'),
    init_globals={'AUTHORING_JOB':{'operation':'scene','map':kind}})['REPORT']
(ROOT/('SourceArt/FrontEnd/'+kind+'_map_report.json')).write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
