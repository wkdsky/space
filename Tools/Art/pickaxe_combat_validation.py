"""Disposable PIE probes for moving melee targets, arm clearance and safe refuge seams."""
import json, traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/PickaxeCombat'
OUT.mkdir(parents=True,exist_ok=True)
GS=unreal.GameplayStatics
PLAYER='/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2'

def xyz(v):return [v.x,v.y,v.z]
def spawn(world,cls,point,forward,up):
    if not isinstance(cls,unreal.Class):cls=cls.static_class()
    pose=unreal.Transform(location=point,rotation=unreal.MathLibrary.make_rot_from_xz(forward,up))
    lib=unreal.get_default_object(GS.static_class())
    actor=lib.call_method('BeginDeferredActorSpawnFromClass',(world,cls,pose,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    return lib.call_method('FinishSpawningActor',(actor,pose,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))

def capture(world,point,aim,up,name):
    actor=spawn(world,unreal.SceneCapture2D,point,aim-point,up)
    comp=actor.capture_component2d
    tex=unreal.RenderingLibrary.create_render_target2d(world,1280,900,unreal.TextureRenderTargetFormat.RTF_RGBA8)
    comp.set_editor_property('texture_target',tex)
    comp.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    comp.set_editor_property('capture_every_frame',False)
    comp.set_editor_property('always_persist_rendering_state',True)
    pp=comp.get_editor_property('post_process_settings')
    pp.set_editor_property('override_auto_exposure_method',True)
    pp.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
    pp.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True)
    pp.set_editor_property('auto_exposure_apply_physical_camera_exposure',False)
    comp.set_editor_property('post_process_settings',pp)
    comp.set_editor_property('post_process_blend_weight',1.)
    comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(world,tex,str(OUT),name+'.png')
    actor.destroy_actor()
    return str(OUT/(name+'.png'))

def seams(world):
    result=[]
    for actor in GS.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceAttachment):
        solid=actor.get_component_by_class(unreal.StaticMeshComponent)
        asset=solid.get_editor_property('static_mesh')
        if not asset or not asset.get_name().startswith('SM_MS3_RiftRim_'):continue
        patch=actor.get_component_by_class(unreal.ProceduralMeshComponent)
        result.append((actor,solid,patch,asset))
    return result

def visual(world):
    assert not hasattr(unreal,'_pickaxe_visual_pending'),'Probe already running'
    pawn=GS.get_player_pawn(world,0);pc=GS.get_player_controller(world,0)
    planet=next(a for a in GS.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
    seam=seams(world)[0][0]
    ground=planet.project_point_to_surface(seam.get_actor_location()-seam.get_actor_right_vector()*350)
    up=ground.impact_normal;forward=seam.get_actor_forward_vector();right=up.cross(forward);right.normalize();forward=right.cross(up)
    base=ground.impact_point+up*96
    pawn.character_movement.disable_movement();pawn.set_actor_location(base,False,True)
    pawn.set_actor_rotation(unreal.MathLibrary.make_rot_from_xz(forward,up),True)
    hp=pawn.get_component_by_class(unreal.JTSHealthComponent);hp.set_max_health(10000,True)
    inv=pawn.get_component_by_class(unreal.JTSInventoryComponent)
    if inv.get_item_count(unreal.JTSItemId.PICKAXE)==0:
        if not inv.can_add_item(unreal.JTSItemId.PICKAXE,1):
            old=inv.get_item_at_slot(0)
            inv.call_method('ServerDestroyItemQuantityAtSlot',(0,old.get_editor_property('stack_count')))
        assert inv.try_add_item_by_id(unreal.JTSItemId.PICKAXE,1)
    inv.select_quickbar_slot(next(i for i in range(inv.get_inventory_capacity()) if inv.get_item_at_slot(i).get_editor_property('item_id')==unreal.JTSItemId.PICKAXE))
    melee=pawn.get_component_by_class(unreal.JTSMeleeComponent)
    tool=pawn.get_component_by_class(unreal.JTSMiningStrikeComponent)
    mesh=pawn.get_component_by_class(unreal.SkeletalMeshComponent)
    components={c.get_name():c for c in pawn.get_components_by_class(unreal.SceneComponent)}
    held=next(c for c in pawn.get_components_by_class(unreal.StaticMeshComponent) if c.get_editor_property('static_mesh') and c.get_editor_property('static_mesh').get_name()=='SM_MiningPickaxe')
    held_asset=held.get_editor_property('static_mesh')
    vertices=[]
    for section in range(held_asset.get_num_sections(0)):
        vertices.extend(unreal.ProceduralMeshLibrary.get_section_from_static_mesh(held_asset,0,section)[0])
    camera=spawn(world,unreal.CameraActor,base-forward*280+right*55+up*60,forward,up)
    old_view=pc.get_view_target();pc.set_view_target_with_blend(camera,0)
    lamp=spawn(world,unreal.PointLight,base+right*130+up*150,forward,up)
    lamp.point_light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    lamp.point_light_component.set_intensity(2000)
    lamp.point_light_component.set_attenuation_radius(1500)
    result=dict(pending=True,cases=[],frames=[],images=[])
    unreal._pickaxe_visual_result=result
    GS.set_global_time_dilation(world,.2)
    class Probe:
        def __init__(self):
            self.index=-1;self.target=None;self.phase='next';self.at=GS.get_time_seconds(world);self.shots=set()
            self.handle=unreal.register_slate_post_tick_callback(self.tick)
        def finish(self):
            melee.attack_released();melee.stop_attack()
            GS.set_global_time_dilation(world,1.)
            pc.set_view_target_with_blend(old_view,0)
            for a in (camera,lamp,self.target):
                if a:a.destroy_actor()
            unreal.unregister_slate_post_tick_callback(self.handle)
            result['pending']=False
            (OUT/'visual.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
            del unreal._pickaxe_visual_pending
        def tick(self,dt):
            try:
                now=GS.get_time_seconds(world)
                hp.heal(100)
                if self.phase=='next':
                    if now-self.at<.25:return
                    self.index+=1
                    if self.index==4:self.finish();return
                    kind=('MirrorSkimmer','Riftlip','SpireWorker','SpireSoldier')[self.index]
                    cls=unreal.load_class(None,'/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_'+kind+'.BP_'+kind+'_C')
                    self.target=spawn(world,cls,base+forward*75-up*65,forward,up)
                    self.target.set_actor_tick_enabled(False)
                    self.before=self.target.get_component_by_class(unreal.JTSHealthComponent).get_health()
                    self.count=tool.get_confirmed_impact_count()
                    self.at=now;self.phase='aim';self.shots=set();return
                position=base+forward*(75+min(20,max(0,now-self.at)*65))+right*min(12,max(0,now-self.at)*35)-up*65
                self.target.set_actor_location(position,False,True)
                camera.set_actor_rotation(unreal.MathLibrary.make_rot_from_xz(position-camera.get_actor_location(),up),True)
                if self.phase=='aim':
                    if now-self.at<.25:return
                    melee.refresh_melee_target()
                    self.row=dict(kind=self.target.get_class().get_name(),preview=melee.get_current_melee_target()==self.target,before=self.before)
                    result['cases'].append(self.row)
                    melee.start_attack();self.at=now;self.phase='swing';return
                age=now-self.at
                tip=components['WeaponMuzzle'].get_world_location()
                palm=mesh.get_socket_location('Wrist_R')+(mesh.get_socket_location('Index1_R')-mesh.get_socket_location('Wrist_R'))*1.35
                anchor=components['WeaponHandAttachmentAnchor'].get_world_location()
                body_points=[pawn.get_actor_transform().inverse_transform_location(held.get_world_transform().transform_location(v)) for v in vertices]
                torso_clearance=min(((v.x/20.)**2+(v.y/17.)**2 for v in body_points if -35<v.z<56),default=100.)
                result['frames'].append(dict(case=self.index,time=age,palm_error=(anchor-palm).length(),tip_to_body=(tip-position).length(),
                    torso_ellipse_clearance=torso_clearance,checked_vertices=len(vertices),tip_local=xyz(pawn.get_actor_transform().inverse_transform_location(tip)),grip_local=xyz(pawn.get_actor_transform().inverse_transform_location(anchor))))
                if self.index==0:
                    for name,t in [('windup',.055),('contact',.213),('recovery',.32)]:
                        if age>=t and name not in self.shots:
                            self.shots.add(name)
                            result['images'].append(capture(world,base+forward*170+right*280+up*80,base+forward*45,up,name))
                if age<.36:return
                self.row.update(after=self.target.get_component_by_class(unreal.JTSHealthComponent).get_health(),impacts=tool.get_confirmed_impact_count()-self.count)
                melee.stop_attack();self.target.destroy_actor();self.target=None;self.phase='next';self.at=now
            except Exception:
                result['error']=traceback.format_exc();self.finish()
    unreal._pickaxe_visual_pending=Probe()
    return result

def run(stage):
    if stage=='qa_visual_result':return getattr(unreal,'_pickaxe_visual_result',{})
    if stage=='qa_api':return dict(worlds=[dict(path=w.get_path_name(),mode=str(GS.get_game_mode(w)),players=[dict(name=a.get_name(),local=a.is_locally_controlled()) for a in GS.get_all_actors_of_class(w,unreal.JTSCharacter)]) for w in unreal.EditorLevelLibrary.get_pie_worlds(False)],mesh_section_doc=unreal.ProceduralMeshLibrary.get_section_from_static_mesh.__doc__)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world,'Start PIE first'
    if stage=='qa_visual':return visual(world)
    if stage=='qa_burrow':
        planet=next(a for a in GS.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        candidates=[a for a in GS.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceCreature) if 'Riftlip' in a.get_class().get_name() and a.get_editor_property('refuge_actor')]
        a=candidates[0]
        refuge=a.get_editor_property('refuge_actor')
        ground=planet.project_point_to_surface(a.get_editor_property('refuge')+refuge.get_actor_right_vector()*120)
        a.set_actor_location(ground.impact_point,False,True)
        # Exercise the existing return-from-feeding path without creating an aggressor/retaliation state.
        a.call_method('OnFeedingTargetDestroyed',(None,))
        result=dict(pending=True,creature=a.get_name(),samples=[])
        unreal._pickaxe_burrow_result=result
        class Watch:
            def __init__(self):self.at=GS.get_time_seconds(world);self.handle=unreal.register_slate_post_tick_callback(self.tick)
            def tick(self,dt):
                age=GS.get_time_seconds(world)-self.at
                state=str(a.get_editor_property('state'))
                dist=planet.approximate_surface_arc_distance(a.get_actor_location(),a.get_editor_property('refuge'))
                result['samples'].append(dict(time=age,state=state,distance_cm=dist))
                if 'BURROW' in state or 'HIDDEN' in state or age>25:
                    result['pending']=False
                    result['entered_at_seam']=dist<8.1 and ('BURROW' in state or 'HIDDEN' in state)
                    unreal.unregister_slate_post_tick_callback(self.handle)
                    (OUT/'burrow.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        unreal._pickaxe_burrow_watch=Watch()
        return dict(creature=a.get_name(),started=True)
    if stage=='qa_burrow_result':return getattr(unreal,'_pickaxe_burrow_result',{})
    if stage=='qa_seams':
        rows=[]
        for a,solid,patch,asset in seams(world):
            planet=a.get_owner();up=planet.get_radial_up_vector(a.get_actor_location())
            samples=[]
            for offset in (-100,-50,0,50,100):
                p=a.get_actor_location()+a.get_actor_forward_vector()*offset
                hit=planet.project_point_to_surface(p+up*40)
                samples.append((hit.impact_point-p).length())
            rows.append(dict(name=a.get_name(),mesh=asset.get_name(),sections=patch.get_num_sections(),visible=patch.is_visible(),
                cast_shadow=patch.get_editor_property('cast_shadow'),collision=str(patch.get_collision_enabled()),solid_collision=str(solid.get_collision_enabled()),ground_samples_cm=samples))
        assert rows
        a=seams(world)[0][0];up=a.get_actor_up_vector();p=a.get_actor_location()
        lamp=spawn(world,unreal.PointLight,p+up*250,a.get_actor_forward_vector(),up)
        lamp.point_light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        lamp.point_light_component.set_editor_property('intensity_units',unreal.LightUnits.CANDELAS)
        lamp.point_light_component.set_intensity(120)
        lamp.point_light_component.set_attenuation_radius(1200)
        image=capture(world,p+a.get_actor_right_vector()*150+up*340,p,up,'riftlip_seam')
        lamp.destroy_actor()
        return dict(count=len(rows),rows=rows,image=image)
    raise ValueError(stage)
