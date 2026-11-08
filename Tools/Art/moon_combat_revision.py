"""Scoped editor configuration and disposable QA for Moon combat presentation."""
import json,traceback
from pathlib import Path
import unreal
KINDS=('MirrorSkimmer','Riftlip','SpireWorker','SpireSoldier')
ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/MoonCombatRevision';OUT.mkdir(parents=True,exist_ok=True)
GS=unreal.GameplayStatics
def xyz(v):return [v.x,v.y,v.z]
def inventory(world):
    inv=GS.get_player_pawn(world,0).get_component_by_class(unreal.JTSInventoryComponent)
    return [dict(item=str(inv.get_item_at_slot(i).get_editor_property('item_id')),count=inv.get_item_at_slot(i).get_editor_property('stack_count')) for i in range(inv.get_inventory_capacity()) if inv.get_item_at_slot(i).get_editor_property('stack_count')>0]

def deaths(world):
    from pickaxe_combat_validation import spawn
    if hasattr(unreal,'_moon_revision_death_fixture'):
        old=unreal._moon_revision_death_fixture
        for actor in old['bugs']+[v for camera,lamp,_,_ in old['views'] for v in (camera,lamp)]:
            try:actor.destroy_actor()
            except Exception:pass
    f=unreal._moon_revision_fixture;views=[];bugs=[]
    result=dict(pending=True,samples=[],files=[]);unreal._moon_revision_deaths=result
    for i,kind in enumerate(KINDS):
        ground=f['planet'].project_point_to_surface(f['origin']+f['forward']*1150+f['right']*(i-1.5)*180)
        up=ground.impact_normal;forward=f['forward'];right=up.cross(forward);right.normalize();forward=right.cross(up)
        cls=unreal.load_class(None,'/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_'+kind+'.BP_'+kind+'_C')
        bug=spawn(world,cls,ground.impact_point,forward,up);bugs.append(bug)
        p=bug.get_actor_location();side=-1 if kind=='SpireSoldier' else 1
        point=p+forward*(100*side)-right*(110*side)+up*85
        camera=spawn(world,unreal.SceneCapture2D,point,p+up*20-point,up)
        lamp=spawn(world,unreal.PointLight,point+up*25,forward,up)
        light=lamp.point_light_component;light.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.set_editor_property('intensity_units',unreal.LightUnits.CANDELAS);light.set_intensity(45);light.set_attenuation_radius(600)
        c=camera.capture_component2d
        tex=unreal.RenderingLibrary.create_render_target2d(world,840,620,unreal.TextureRenderTargetFormat.RTF_RGBA8)
        c.set_editor_property('texture_target',tex);c.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        c.set_editor_property('capture_every_frame',False);c.set_editor_property('always_persist_rendering_state',True);c.set_editor_property('fov_angle',48)
        pp=c.get_editor_property('post_process_settings')
        pp.set_editor_property('override_auto_exposure_method',True);pp.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
        pp.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True);pp.set_editor_property('auto_exposure_apply_physical_camera_exposure',False)
        c.set_editor_property('post_process_settings',pp);c.set_editor_property('post_process_blend_weight',1.)
        views.append((camera,lamp,tex,kind))
    for i,(camera,_,_,_) in enumerate(views):camera.capture_component2d.set_editor_property('hidden_actors',[f['pawn']]+f['bugs']+[b for j,b in enumerate(bugs) if j!=i])
    class Probe:
        def __init__(self):self.frames=0;self.at=None;self.shot=False;self.handle=unreal.register_slate_post_tick_callback(self.tick)
        def export(self,suffix):
            for _,_,tex,kind in views:
                name=kind+'_'+suffix+'.png';unreal.RenderingLibrary.export_render_target(world,tex,str(OUT),name)
                result['files'].append(str(OUT/name))
        def finish(self):
            result['pending']=False;unreal.unregister_slate_post_tick_callback(self.handle)
            (OUT/'runtime_deaths.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        def tick(self,dt):
            try:
                self.frames+=1
                for camera,_,_,_ in views:camera.capture_component2d.capture_scene()
                if self.at is None:
                    if self.frames<18:return
                    self.export('alive')
                    for bug in bugs:GS.apply_damage(bug,10000,f['pc'],f['pawn'],unreal.DamageType)
                    self.at=GS.get_time_seconds(world);return
                age=GS.get_time_seconds(world)-self.at
                row=dict(time=age,creatures=[])
                for bug,kind in zip(bugs,KINDS):
                    mesh=bug.get_component_by_class(unreal.SkeletalMeshComponent)
                    row['creatures'].append(dict(kind=kind,state=str(bug.get_editor_property('state')),position=mesh.get_position(),body=xyz(bug.get_actor_transform().inverse_transform_location(mesh.get_socket_location('body'))),collision=[str(c.get_collision_enabled()) for c in bug.get_components_by_class(unreal.ShapeComponent)]))
                result['samples'].append(row)
                if age>.3 and not self.shot:self.export('impulse');self.shot=True
                if age>2.2:self.export('settled');result['final']=row;self.finish()
            except Exception:result['error']=traceback.format_exc();self.finish()
    unreal._moon_revision_death_fixture=dict(bugs=bugs,views=views)
    unreal._moon_revision_death_probe=Probe()
    return dict(pending=True)
def formation(world):
    from pickaxe_combat_validation import spawn
    pawn=GS.get_player_pawn(world,0);pc=GS.get_player_controller(world,0)
    planet=next(a for a in GS.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
    nest=next(a for a in GS.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat) if a.get_actor_label()=='Moon_Nest_01')
    mesh=nest.get_editor_property('refuge_mesh');frame=mesh.get_world_transform()
    origin=next(c for c in nest.get_components_by_class(unreal.BoxComponent) if c.get_name()=='EntranceTarget').get_world_location()
    exit=frame.transform_location(nest.get_editor_property('entrance_exit_offset'))
    emergence=frame.transform_location(nest.get_editor_property('entrance_emergence_offset'))
    hit=planet.project_point_to_surface(exit);up=hit.impact_normal
    forward=exit-emergence;right=up.cross(forward);right.normalize();forward=right.cross(up)
    ground=planet.project_point_to_surface(origin+forward*650)
    pawn.character_movement.disable_movement();pawn.set_actor_location(ground.impact_point+ground.impact_normal*96,False,True)
    pawn.get_component_by_class(unreal.JTSHealthComponent).set_max_health(10000,True)
    camera=spawn(world,unreal.CameraActor,pawn.get_actor_location()+up*55,origin+forward*240+up*25-(pawn.get_actor_location()+up*55),up)
    pc.set_view_target_with_blend(camera,0)
    cls=unreal.load_class(None,'/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_MirrorSkimmer.BP_MirrorSkimmer_C')
    bugs=[]
    for i in range(3):
        a=spawn(world,cls,hit.impact_point,forward,up)
        assert a.call_method('InitializeForSettlement',(planet,origin,hit.impact_point))
        a.defend_entrance(pawn,origin,1800,1,3);bugs.append(a)
    f=dict(planet=planet,origin=origin,up=up,forward=forward,right=right,bugs=bugs,camera=camera,pawn=pawn,pc=pc)
    unreal._moon_revision_fixture=f
    result=dict(pending=True,inventory=inventory(world),samples=[],peak_glare=0,peak_white=0.)
    unreal._moon_revision_result=result
    class Probe:
        def __init__(self):self.at=GS.get_time_seconds(world);self.handle=unreal.register_slate_post_tick_callback(self.tick)
        def tick(self,dt):
            try:
                age=GS.get_time_seconds(world)-self.at
                positions=[a.get_actor_location() for a in bugs]
                defenses=[a.get_component_by_class(unreal.JTSMirrorDefenseComponent) for a in bugs]
                visible=sum(d.can_glare_at(pc) for d in defenses)
                widgets=unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.JTSMirrorGlareWidget,False)
                white=max((w.get_full_white_strength() for w in widgets),default=0.)
                result['peak_glare']=max(result['peak_glare'],visible);result['peak_white']=max(result['peak_white'],white)
                row=dict(time=age,positions=[xyz(p) for p in positions],distance=min((positions[i]-positions[j]).length() for i,j in ((0,1),(0,2),(1,2))),reflecting=[d.call_method('IsReflecting',()) for d in defenses],visible=visible,white=white)
                result['samples'].append(row)
                if white>.1 and not result.get('screenshot'):
                    result['screenshot']=str(OUT/'formation_white.png')
                    unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename='+result['screenshot']+' -nosuffix')
                if age<10:return
                result['pending']=False;result['final']=row
                unreal.unregister_slate_post_tick_callback(self.handle)
                (OUT/'formation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
            except Exception:
                result['error']=traceback.format_exc();result['pending']=False
                unreal.unregister_slate_post_tick_callback(self.handle)
                (OUT/'formation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    unreal._moon_revision_probe=Probe()
    return {k:v for k,v in result.items() if k!='samples'}
def run(stage):
    if stage=='moon_revision_glare_shot':
        f=unreal._moon_revision_fixture;f['pawn'].set_actor_hidden_in_game(True)
        unreal.SystemLibrary.execute_console_command(f['pawn'].get_world(),'Shot SHOWUI filename='+str(OUT/'formation_white.png')+' -nosuffix')
        return dict(file=str(OUT/'formation_white.png'))
    if stage=='moon_revision_deaths':return deaths(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world())
    if stage=='moon_revision_death_result':return {k:v for k,v in getattr(unreal,'_moon_revision_deaths',{}).items() if k!='samples'}
    if stage=='moon_revision_sample':
        f=unreal._moon_revision_fixture;world=f['pawn'].get_world()
        positions=[a.get_actor_location() for a in f['bugs']]
        defenses=[a.get_component_by_class(unreal.JTSMirrorDefenseComponent) for a in f['bugs']]
        widgets=unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.JTSMirrorGlareWidget,False)
        result=dict(positions=[xyz(p) for p in positions],distance=min((positions[i]-positions[j]).length() for i,j in ((0,1),(0,2),(1,2))),states=[str(a.get_editor_property('state')) for a in f['bugs']],reflecting=[d.call_method('IsReflecting',()) for d in defenses],visible=[d.can_glare_at(f['pc']) for d in defenses],counts=[w.get_illuminating_mirror_count() for w in widgets],white=max((w.get_full_white_strength() for w in widgets),default=0.))
        (OUT/'formation_settled.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        return result
    if stage=='moon_revision_result':return {k:v for k,v in getattr(unreal,'_moon_revision_result',{}).items() if k!='samples'}
    if stage=='moon_revision_formation':
        return formation(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world())
    if stage=='moon_revision_configure':
        assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        bp=unreal.load_asset('/Game/Space/Blueprints/Modes/BP_SpaceWorldGameMode')
        cdo=unreal.get_default_object(bp.generated_class())
        cdo.set_editor_property('starting_item_ids',[unreal.JTSItemId.PICKAXE,unreal.JTSItemId.SHORT_BLADE])
        unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
        return dict(items=[str(i) for i in cdo.get_editor_property('starting_item_ids')])
    if stage=='moon_revision_import':
        import moon_surface_unreal as assets
        assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        result=[]
        for k in KINDS:
            folder='/Game/Space/Creatures/MoonV3/'+k
            mesh=unreal.load_asset(folder+'/SK_'+k);skeleton=mesh.get_editor_property('skeleton')
            if k=='Riftlip':
                materials=list(mesh.get_editor_property('materials'))
                task=assets.fbx_task(ROOT/'SourceArt/MoonCreaturesV3'/k/('SK_'+k+'.fbx'),folder,'SK_'+k,True,False,skeleton)
                task.options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose',False)
                mesh.get_editor_property('asset_import_data').set_editor_property('update_skeleton_reference_pose',False)
                assets.import_tasks([task]);mesh.set_editor_property('materials',materials)
                assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
            name='A_'+k+'_Death'
            task=assets.fbx_task(ROOT/'SourceArt/MoonCreaturesV3'/k/(name+'.fbx'),folder+'/Animations',name,True,True,skeleton)
            assets.import_tasks([task]);clip=unreal.load_asset(folder+'/Animations/'+name)
            assert not clip.get_editor_property('enable_root_motion')
            assert unreal.EditorAssetLibrary.save_loaded_asset(clip)
            bp=unreal.load_asset('/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_'+k)
            cdo=unreal.get_default_object(bp.generated_class());rates=dict(cdo.get_editor_property('animation_play_rates'))
            rates[unreal.JTSForagerState.DEAD]=1.;cdo.set_editor_property('animation_play_rates',rates)
            unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
            result.append(dict(kind=k,length=clip.get_play_length()))
        return result
    if stage=='moon_revision_inspect':
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        cls=world.get_world_settings().get_editor_property('default_game_mode')
        cdo=unreal.get_default_object(cls)
        return dict(mode=cls.get_path_name(),items=[str(i) for i in cdo.get_editor_property('starting_item_ids')],creatures=[dict(kind=k,clips={str(s):a.get_path_name() for s,a in unreal.get_default_object(unreal.load_asset('/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_'+k).generated_class()).get_editor_property('animations').items() if s==unreal.JTSForagerState.DEAD}) for k in KINDS])
    raise ValueError(stage)
