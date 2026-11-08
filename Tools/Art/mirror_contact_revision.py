"""Inspect and validate posed MirrorSkimmer combat bodies beside real nest mouths."""
import json, traceback
from pathlib import Path
import unreal
GS=unreal.GameplayStatics
OUT=Path(unreal.Paths.project_saved_dir())/'MirrorContactQA'
OUT.mkdir(parents=True,exist_ok=True)

def xyz(v):return [v.x,v.y,v.z]
def captures(world):
    from pickaxe_combat_validation import spawn
    f=unreal._mirror_fixture;views=[]
    for index,side in ((0,1),(1,1),(1,-1)):
        bug=f['bugs'][index];p=bug.get_actor_location()
        point=p-f['forward']*70+f['right']*side*85+f['up']*60
        actor=spawn(world,unreal.SceneCapture2D,point,p+f['up']*15-point,f['up'])
        lamp=spawn(world,unreal.PointLight,point+f['up']*20,f['forward'],f['up'])
        light=lamp.point_light_component;light.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.set_editor_property('intensity_units',unreal.LightUnits.CANDELAS);light.set_intensity(40);light.set_attenuation_radius(800)
        c=actor.capture_component2d
        tex=unreal.RenderingLibrary.create_render_target2d(world,1280,900,unreal.TextureRenderTargetFormat.RTF_RGBA8)
        c.set_editor_property('texture_target',tex);c.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        c.set_editor_property('capture_every_frame',False);c.set_editor_property('always_persist_rendering_state',True)
        c.set_editor_property('fov_angle',52);c.set_editor_property('hidden_actors',[f['pawn']])
        pp=c.get_editor_property('post_process_settings')
        pp.set_editor_property('override_auto_exposure_method',True);pp.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
        pp.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True);pp.set_editor_property('auto_exposure_apply_physical_camera_exposure',False)
        c.set_editor_property('post_process_settings',pp);c.set_editor_property('post_process_blend_weight',1.)
        views.append((actor,lamp,tex,'guard_%d_side_%d.png'%(index,side)))
    class Pending:
        def __init__(self):self.frames=0;self.handle=unreal.register_slate_post_tick_callback(self.tick)
        def tick(self,dt):
            self.frames+=1
            for actor,_,_,_ in views:actor.capture_component2d.capture_scene()
            if self.frames<18:return
            for actor,lamp,tex,name in views:
                unreal.RenderingLibrary.export_render_target(world,tex,str(OUT),name)
                actor.destroy_actor();lamp.destroy_actor()
            unreal.unregister_slate_post_tick_callback(self.handle)
    unreal._mirror_captures=Pending()
    return dict(files=[str(OUT/v[3]) for v in views])
def configure():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(),'Stop PIE first'
    import moon_surface_unreal as assets
    bp=unreal.load_asset('/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_MirrorSkimmer')
    cdo=unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('combat_body_bone','body')
    cdo.set_editor_property('combat_body_center',unreal.Vector(0,0,9.5))
    cdo.set_editor_property('combat_body_half_extent',unreal.Vector(27,20,9))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    folder='/Game/Space/Creatures/MoonV3/MirrorSkimmer'
    mesh=unreal.load_asset(folder+'/SK_MirrorSkimmer')
    name='A_MirrorSkimmer_Death'
    source=Path(unreal.Paths.project_dir()).resolve()/'SourceArt/MoonCreaturesV3/MirrorSkimmer'/(name+'.fbx')
    task=assets.fbx_task(source,folder+'/Animations',name,True,True,mesh.get_editor_property('skeleton'))
    assets.import_tasks([task])
    clip=unreal.load_asset(folder+'/Animations/'+name)
    assert not clip.get_editor_property('enable_root_motion')
    assert unreal.EditorAssetLibrary.save_loaded_asset(clip)
    return dict(clip=clip.get_path_name(),seconds=clip.get_play_length(),body_bone=str(cdo.get_editor_property('combat_body_bone')))

def verify(world):
    from pickaxe_combat_validation import spawn
    f=unreal._mirror_fixture;melee=f['pawn'].get_component_by_class(unreal.JTSMeleeComponent)
    tool=f['pawn'].get_component_by_class(unreal.JTSMiningStrikeComponent)
    lamp=spawn(world,unreal.PointLight,f['base']+f['right']*130+f['up']*150,f['forward'],f['up'])
    lamp.point_light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    lamp.point_light_component.set_intensity(200000);lamp.point_light_component.set_attenuation_radius(1500)
    result=dict(pending=True,guards=[],death=[]);unreal._mirror_result=result
    class Probe:
        def __init__(self):
            self.index=0;self.phase='aim';self.at=GS.get_time_seconds(world);self.shots=set()
            self.handle=unreal.register_slate_post_tick_callback(self.tick)
        def finish(self):
            melee.attack_released();melee.stop_attack();GS.set_global_time_dilation(world,1.)
            unreal.unregister_slate_post_tick_callback(self.handle)
            lamp.destroy_actor();result['pending']=False
            if result.get('killed') and not result.get('error'):result['settled_views']=captures(world)
            (OUT/'real_nest_contacts.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        def tick(self,dt):
            try:
                now=GS.get_time_seconds(world);age=now-self.at
                bug=f['bugs'][self.index];mesh=bug.get_component_by_class(unreal.SkeletalMeshComponent)
                body=bug.get_component_by_class(unreal.BoxComponent)
                hp=bug.get_component_by_class(unreal.JTSHealthComponent)
                target=body.get_world_location()
                f['camera'].set_actor_rotation(unreal.MathLibrary.make_rot_from_xz(target-f['camera'].get_actor_location(),f['up']),True)
                if self.phase=='aim':
                    if age<.35:return
                    melee.refresh_melee_target()
                    self.row=dict(bug=bug.get_name(),preview=melee.get_current_melee_target()==bug,before=hp.get_health(),shell=xyz(mesh.get_socket_location('body')),body=xyz(target))
                    result['guards'].append(self.row)
                    self.count=tool.get_confirmed_impact_count();melee.start_attack();self.phase='strike';self.at=now;return
                if self.phase=='strike':
                    if age<.36:return
                    self.row.update(after=hp.get_health(),impacts=tool.get_confirmed_impact_count()-self.count)
                    melee.attack_released();melee.stop_attack()
                    if self.index==0:
                        self.index=1;self.phase='aim';self.at=now;return
                    self.phase='kill_wait';self.at=now;return
                if self.phase=='kill_wait':
                    if age<.3:return
                    melee.start_attack();self.phase='kill';self.at=now;return
                if self.phase=='kill':
                    if not hp.is_dead and age<1:return
                    melee.attack_released();melee.stop_attack()
                    result['killed']=hp.is_dead;self.phase='death';self.at=now;return
                if self.phase=='death':
                    result['death'].append(dict(time=age,state=str(bug.get_editor_property('state')),body_collision=str(body.get_collision_enabled()),shell_local=xyz(bug.get_actor_transform().inverse_transform_location(mesh.get_socket_location('body')))))
                    if age>1.8:self.finish()
            except Exception:
                result['error']=traceback.format_exc();self.finish()
    GS.set_global_time_dilation(world,.2)
    unreal._mirror_probe=Probe()
    return result

def run(stage):
    if stage=='mirror_configure':return configure()
    if stage=='mirror_result':return getattr(unreal,'_mirror_result',{})
    if stage=='mirror_api':return {cls.__name__:{n:str(getattr(cls,n).__doc__)[:4000] for n in dir(cls) if 'hit' in n.lower() or 'tuple' in n} for cls in (unreal.HitResult,unreal.GameplayStatics,unreal.SystemLibrary,unreal.MathLibrary)}
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stage=='mirror_inspect':
        bp=unreal.load_asset('/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_MirrorSkimmer')
        cdo=unreal.get_default_object(bp.generated_class());mesh=cdo.get_component_by_class(unreal.SkeletalMeshComponent)
        return dict(animations={str(k):v.get_path_name() if v else None for k,v in cdo.get_editor_property('animations').items()},mesh=str(mesh.get_editor_property('skeletal_mesh_asset')),mesh_transform=str(mesh.get_relative_transform()),defense=str(cdo.get_editor_property('mirror_defense').get_editor_property('body_bone')))
    assert world,'Start PIE first'
    if stage=='mirror_verify':return verify(world)
    if stage=='mirror_captures':return captures(world)
    if stage=='mirror_setup':
        from pickaxe_combat_validation import spawn
        if hasattr(unreal,'_mirror_fixture'):
            old=unreal._mirror_fixture
            for actor in old['bugs']+[old['camera']]:
                try:actor.destroy_actor()
                except Exception:pass
        pawn=GS.get_player_pawn(world,0);pc=GS.get_player_controller(world,0)
        nest=next(a for a in GS.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat) if a.get_actor_label()=='Moon_Nest_01')
        planet=next(a for a in GS.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        mesh=nest.get_editor_property('refuge_mesh');frame=mesh.get_world_transform()
        ground=planet.project_point_to_surface(frame.transform_location(unreal.Vector(-190,0,0)))
        up=ground.impact_normal;forward=mesh.get_forward_vector();right=up.cross(forward);right.normalize();forward=right.cross(up)
        base=ground.impact_point+up*96
        pawn.character_movement.disable_movement();pawn.set_actor_location(base,False,True);pawn.set_actor_rotation(unreal.MathLibrary.make_rot_from_xz(forward,up),True)
        pawn.get_component_by_class(unreal.JTSHealthComponent).set_max_health(10000,True)
        inv=pawn.get_component_by_class(unreal.JTSInventoryComponent)
        if not inv.get_item_count(unreal.JTSItemId.PICKAXE):
            if not inv.can_add_item(unreal.JTSItemId.PICKAXE,1):
                old=inv.get_item_at_slot(0);inv.call_method('ServerDestroyItemQuantityAtSlot',(0,old.get_editor_property('stack_count')))
            assert inv.try_add_item_by_id(unreal.JTSItemId.PICKAXE,1)
        inv.select_quickbar_slot(next(i for i in range(inv.get_inventory_capacity()) if inv.get_item_at_slot(i).get_editor_property('item_id')==unreal.JTSItemId.PICKAXE))
        cls=unreal.load_class(None,'/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_MirrorSkimmer.BP_MirrorSkimmer_C')
        bugs=[]
        for side in (-1,1):
            hit=planet.project_point_to_surface(frame.transform_location(unreal.Vector(-95,side*45,0)))
            a=spawn(world,cls,hit.impact_point,forward,hit.impact_normal);a.get_component_by_class(unreal.JTSMirrorDefenseComponent).aim_at(pawn);bugs.append(a)
        camera=spawn(world,unreal.CameraActor,base-forward*280+right*55+up*60,bugs[0].get_actor_location()-(base-forward*280+right*55+up*60),up)
        pc.set_view_target_with_blend(camera,0)
        unreal._mirror_fixture=dict(world=world,pawn=pawn,pc=pc,nest=nest,bugs=bugs,camera=camera,up=up,forward=forward,right=right,base=base)
        return dict(nest=nest.get_name(),bugs=[a.get_name() for a in bugs],trace_doc=unreal.SystemLibrary.line_trace_single.__doc__)
    if stage=='mirror_rays':
        f=unreal._mirror_fixture;pawn=f['pawn'];camera=f['camera'];rows=[]
        for a in f['bugs']:
            mesh=a.get_component_by_class(unreal.SkeletalMeshComponent);target=a.get_component_by_class(unreal.JTSMirrorDefenseComponent).get_mirror_location()
            camera.set_actor_rotation(unreal.MathLibrary.make_rot_from_xz(target-camera.get_actor_location(),f['up']),True)
            rays=[]
            for origin in (camera.get_actor_location(),pawn.get_actor_transform().transform_location(unreal.Vector(3.2,16.1,27.6))):
                for complex in (False,True):
                    out=unreal.SystemLibrary.line_trace_single(world,origin,target,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,complex,[pawn],unreal.DrawDebugTrace.NONE,True)
                    rays.append(dict(complex=complex,result=[str(v) for v in out.to_tuple()] if out else None))
            rows.append(dict(bug=a.get_name(),target=xyz(target),shell=xyz(mesh.get_socket_location('body')),rays=rays))
        (OUT/'before_rays.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
        return rows
    if stage=='mirror_nests':
        return [dict(name=a.get_name(),label=a.get_actor_label(),location=xyz(a.get_actor_location()),offset=xyz(a.get_editor_property('entrance_interaction_offset')),components=[dict(name=c.get_name(),location=xyz(c.get_world_location()),extent=str(c.get_editor_property('box_extent'))) for c in a.get_components_by_class(unreal.BoxComponent)]) for a in GS.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat) if a.get_editor_property('can_block_entrance')]
    if stage=='mirror_population':
        return [dict(name=a.get_name(),location=xyz(a.get_actor_location()),state=str(a.get_editor_property('state')),body=str(a.get_component_by_class(unreal.SphereComponent).get_world_transform()),shell=str(a.get_component_by_class(unreal.SkeletalMeshComponent).get_socket_transform('body')),refuge=str(a.get_editor_property('refuge_actor'))) for a in GS.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceCreature) if 'MirrorSkimmer' in a.get_class().get_name()]
    if stage=='mirror_natural':
        nest=unreal._mirror_fixture['nest']
        return [dict(name=a.get_name(),local=xyz(nest.get_actor_transform().inverse_transform_location(a.get_actor_location())),state=str(a.get_editor_property('state')),hp=a.get_component_by_class(unreal.JTSHealthComponent).get_health(),body_enabled=str(a.get_component_by_class(unreal.BoxComponent).get_collision_enabled())) for a in GS.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceCreature) if 'MirrorSkimmer' in a.get_class().get_name() and a.get_editor_property('refuge_actor')==nest]
    raise ValueError(stage)
