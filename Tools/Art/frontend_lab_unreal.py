"""Build an isolated, lit art-review hangar. Does not replace the shipping front end."""
import unreal
import json
import math
from pathlib import Path
REPORT_DIR=Path('D:/projects/space/Saved/Art/FrontEnd')
REPORT_DIR.mkdir(parents=True,exist_ok=True)
import traceback

ROOT=Path(unreal.Paths.project_dir()).resolve()
DEST='/Game/Space/FrontEnd'
EA=unreal.EditorAssetLibrary
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
REPORT={}


def mesh(name,position=(0,0,0),rotation=(0,0,0),scale=(1,1,1),label=None):
    asset=unreal.load_asset(DEST+'/Models/SM_FE_'+name)
    assert asset,name
    actor=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*position),unreal.Rotator(pitch=rotation[0],yaw=rotation[1],roll=rotation[2]))
    actor.set_actor_label(label or 'FE_'+name)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.static_mesh_component.set_static_mesh(asset)
    actor.set_folder_path('FrontEnd/'+('Environment' if name not in ['TwinforkDisplay','MaintenanceRobot'] else 'Hero'))
    return actor


def camera(name,position,target,fov=48):
    rotation=unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*position),unreal.Vector(*target))
    actor=actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(*position),rotation)
    actor.set_actor_label('FE_Camera_'+name)
    actor.camera_component.set_editor_property('field_of_view',fov)
    actor.camera_component.set_editor_property('constrain_aspect_ratio',False)
    actor.set_folder_path('FrontEnd/Cameras')
    return actor


def build():
    path=DEST+'/Maps/L_FrontEndArtLab'
    dirty=unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert all(p.get_name()==path for p in dirty),'Save unrelated maps before creating Art Lab'
    if dirty:levels.save_current_level()
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    current=world.get_outermost().get_name() if world else ''
    del world
    if current==path:pass
    elif EA.does_asset_exist(path):levels.load_level(path)
    else:assert levels.new_level(path)
    for actor in list(actors.get_all_level_actors()):
        if actor.get_actor_label().startswith('FE_'):actors.destroy_actor(actor)
    for x in range(-4,5):
        for y in range(-3,5):mesh('GrateFloor' if x==2 and y in [1,2] else 'Floor',(x*400,y*400,0))
    for x in [-1400,1400]:
        for y in [-600,200,1000]:mesh('Column',(x,y,0))
        for y in [-600,1000]:mesh('Beam',(x,y,500),(0,0,90))
    for x in [-1200,-800,800,1200]:mesh('ServiceWall' if x in [-1200,1200] else 'Wall',(x,1600,0))
    for x in [-1400,1400]:
        for y in range(-600,1400,400):mesh('Rail',(x,y,0),(0,90,0))
    for x in [-1200,-800,-400,0,400,800,1200]:mesh('PlatformEdge',(x,-1250,0))
    mesh('Stairs',(-1100,-1450,-90))
    mesh('DoorFrame',(-650,1590,0));mesh('Door',(-650,1570,0))
    # The display ship is an adapted copy; it has no spacecraft gameplay component.
    ship=mesh('TwinforkDisplay',(-350,650,170),(0,25,0),(.63,.63,.63))
    for name,pos,yaw in [
        ('CrateLarge',(-800,900,0),10),('CrateSmall',(-700,800,0),-8),
        ('CrateSmall',(-800,900,97),15),('Toolbox',(-450,220,0),0),
        ('RepairTerminal',(1050,600,0),-20),('PowerUnit',(1150,1050,0),0),
        ('CableStraight',(1040,840,0),0),('CableCoil',(-260,480,0),0),
        ('RepairCart',(-650,260,0),15),('Barrier',(1000,200,0),-20),
        ('LightPole',(-900,500,0),0),('LightPole',(1200,1000,0),0),
        ('Vent',(1250,1300,0),0),('MaintenanceRobot',(-450,300,0),-25),
    ]:mesh(name,pos,(0,yaw,0))
    for i in range(4):mesh('DockMarker',(-240+i*160,-80,0),label='FE_CrewPad_'+str(i))
    for i,x in enumerate([-2400,2200]):
        mesh('TowerBase',(x,3200,0))
        for z in [400,800,1200]:mesh('TowerMiddle',(x,3200,z))
        mesh('TowerTop',(x,3200,1600));mesh('TowerArm',(x,3200,1200),(0,90 if i else -90,0))
    for x,y in [(-3500,4200),(2600,5000)]:mesh('DistantBuilding',(x,y,0));mesh('Antenna',(x,y,600))
    for i in range(3):mesh('Hill'+str(i+1),(-6000+i*6000,14000,0),(0,i*25,0),(2,2,.6))
    moon=mesh('Moon',(4500,14000,6500),scale=(6.5,6.5,6.5))
    moon.static_mesh_component.set_material(0,unreal.load_asset(DEST+'/Materials/M_FE_Moon'))
    dome=mesh('Moon',(0,0,0),scale=(1000,1000,1000),label='FE_SkyDome')
    dome.static_mesh_component.set_material(0,unreal.load_asset(DEST+'/Materials/M_FE_Sky'))
    dome.static_mesh_component.set_cast_shadow(False)
    dome.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,1600),unreal.Rotator(pitch=-12,yaw=-110,roll=0));sun.set_actor_label('FE_Sunset')
    sun.light_component.set_editor_property('intensity',1000);sun.light_component.set_light_color(unreal.LinearColor(1,.97,.90))
    sun.light_component.set_editor_property('atmosphere_sun_light',True)
    sky=actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,1000));sky.set_actor_label('FE_SkyLight')
    sky.light_component.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property('intensity',1.0)
    sky.light_component.set_editor_property('real_time_capture',True)
    sky.light_component.set_editor_property('lower_hemisphere_is_black',False)
    atmosphere=actors.spawn_actor_from_class(unreal.SkyAtmosphere,unreal.Vector(0,0,0));atmosphere.set_actor_label('FE_Atmosphere')
    for name,pos,color,intensity,radius in [
        ('Key',(-200,-400,420),(1,.78,.55),32000,1300),
        ('Rim',(650,200,350),(.32,.65,1),44000,1500),
        ('HangarWarm',(-800,1200,350),(1,.38,.12),52000,2000),
        ('Fill',(250,-300,250),(.55,.72,1),12000,1200),
    ]:
        light=actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*pos));light.set_actor_label('FE_Light_'+name)
        light.light_component.set_editor_property('intensity',intensity);light.light_component.set_editor_property('attenuation_radius',radius)
        light.light_component.set_editor_property('source_radius',80)
        light.light_component.set_light_color(unreal.LinearColor(*color))
    post=actors.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector());post.set_actor_label('FE_PostProcess');post.set_editor_property('unbound',True)
    settings=post.get_editor_property('settings')
    settings.set_editor_property('override_auto_exposure_method',True);settings.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
    settings.set_editor_property('override_auto_exposure_bias',True);settings.set_editor_property('auto_exposure_bias',-6.5)
    settings.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True)
    settings.set_editor_property('auto_exposure_apply_physical_camera_exposure',False)
    settings.set_editor_property('override_motion_blur_amount',True);settings.set_editor_property('motion_blur_amount',0)
    post.set_editor_property('settings',settings)
    home=camera('Home',(150,-600,215),(0,300,90),50)
    head=camera('Head',(0,-240,170),(-50,-80,167),35)
    full=camera('Full',(80,-460,180),(-50,-80,100),40)
    crew=camera('Crew',(-30,-1100,260),(0,40,115),48)
    actor=actors.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector(-50,-80,0),unreal.Rotator(pitch=0,yaw=200,roll=0));actor.set_actor_label('FE_AvatarPreview')
    component=actor.skeletal_mesh_component
    component.set_skeletal_mesh_asset(unreal.load_asset(DEST+'/Characters/SK_FE_Explorer'))
    component.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    animation=unreal.load_asset(DEST+'/Animations/A_FE_Idle')
    component.play_animation(animation,True)
    data=component.get_editor_property('animation_data');data.set_editor_property('anim_to_play',animation)
    component.set_editor_property('animation_data',data)
    component.set_update_animation_in_editor(True)
    component.set_forced_lod(1)
    # Original 60-second environment motion: robot service route and steam drift.
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    sequence_path=DEST+'/Animations/LS_FE_HangarLoop'
    if EA.does_asset_exist(sequence_path):EA.delete_asset(sequence_path)
    sequence=tools.create_asset('LS_FE_HangarLoop',DEST+'/Animations',unreal.LevelSequence,unreal.LevelSequenceFactoryNew())
    sequence.set_display_rate(unreal.FrameRate(30,1));sequence.set_playback_start(0);sequence.set_playback_end(1800)
    def motion(target,keys):
        binding=sequence.add_possessable(target)
        track=binding.add_track(unreal.MovieScene3DTransformTrack)
        section=track.add_section();section.set_range(0,1801)
        channels=section.get_all_channels()
        for frame,pos,rot,scale in keys:
            for channel,value in zip(channels,[*pos,rot[2],rot[0],rot[1],*scale]):
                channel.add_key(unreal.FrameNumber(frame),float(value),interpolation=unreal.MovieSceneKeyInterpolation.LINEAR)
    robot=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='FE_MaintenanceRobot')
    motion(robot,[(0,(-450,300,0),(0,-25,0),(1,1,1)),(600,(-580,480,0),(0,-25,0),(1,1,1)),(900,(-580,480,0),(0,155,0),(1,1,1)),(1500,(-450,300,0),(0,155,0),(1,1,1)),(1800,(-450,300,0),(0,-25,0),(1,1,1))])
    EA.set_metadata_tag(sequence,'JTS.LoopRouteVersion','1')
    for i in range(3):
        card=mesh('FXCard',(-300+i*35,450,220),rotation=(90,0,0),scale=(.1,.1,.1),label='FE_Steam_'+str(i))
        card.static_mesh_component.set_material(0,unreal.load_asset(DEST+'/Materials/M_FE_Particle'))
        card.static_mesh_component.set_cast_shadow(False);card.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        keys=[]
        for cycle in range(6):
            start=cycle*300
            keys.extend([(start,(-300+i*35,450,200),(90,0,0),(.001,.001,.001)),(start+100,(-290+i*35,455,255),(90,0,0),(.65,.65,.65)),(start+299,(-265+i*35,465,315),(90,0,0),(.001,.001,.001))])
        keys.append((1800,(-300+i*35,450,200),(90,0,0),(.001,.001,.001)));motion(card,keys)
    EA.save_loaded_asset(sequence)
    sequencer=actors.spawn_actor_from_class(unreal.LevelSequenceActor,unreal.Vector());sequencer.set_actor_label('FE_HangarLoop')
    sequencer.set_sequence(sequence)
    playback=sequencer.get_editor_property('playback_settings');playback.set_editor_property('auto_play',True);playback.set_editor_property('loop_count',unreal.MovieSceneSequenceLoopCount(-1));sequencer.set_editor_property('playback_settings',playback)
    for name,volume in [('S_Music_LaunchShift',.38),('S_Amb_Wind',.2),('S_Amb_Hangar',.35),('S_Amb_ShipCooling',.25)]:
        ambient=actors.spawn_actor_from_class(unreal.AmbientSound,unreal.Vector());ambient.set_actor_label('FE_Audio_'+name)
        ambient.audio_component.set_sound(unreal.load_asset(DEST+'/Audio/'+name));ambient.audio_component.set_volume_multiplier(volume)
    sky.light_component.recapture_sky()
    levels.save_current_level()
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(home.get_actor_location(),home.get_actor_rotation())
    REPORT.update({'success':True,'map':path,'actors':len(actors.get_all_level_actors()),'cameras':[c.get_actor_label() for c in [home,head,full,crew]],'display_ship':ship.get_actor_label()})


try:build()
except Exception:REPORT={'success':False,'traceback':traceback.format_exc()}
(REPORT_DIR/'art_lab_report.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
