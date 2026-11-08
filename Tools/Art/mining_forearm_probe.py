"""In-editor integration probe using the pawn's gameplay camera and input mapping."""
import json
from pathlib import Path
import unreal


def run(stage):
    if stage == 'input_debug':
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        lib=unreal.get_default_object(unreal.EnhancedInputLibrary.static_class())
        value=lib.call_method('MakeInputActionValueOfType',(1.,0.,0.,unreal.InputActionValueType.BOOLEAN))
        return dict(value=unreal.EnhancedInputLibrary.conv_input_action_value_to_string(value),
            vector_inject=unreal.EnhancedInputLocalPlayerSubsystem.inject_input_vector_for_action.__doc__,
            pc=pc.get_path_name(),local=pc.is_local_controller(),pawn_local=unreal.GameplayStatics.get_player_pawn(world,0).is_locally_controlled(),
            controllers=[dict(path=p.get_path_name(),local=p.is_local_controller()) for p in unreal.ObjectIterator(unreal.PlayerController) if not p.get_name().startswith('Default')],
            players=[s.get_path_name() for s in unreal.ObjectIterator(unreal.LocalPlayer)],
            inputs=[s.get_path_name() for s in unreal.ObjectIterator(unreal.EnhancedPlayerInput)],
            widgets=[n for n in dir(unreal) if 'Widget' in n and 'Library' in n],
            subsystems=[dict(path=s.get_path_name(),outer=s.get_outer().get_path_name()) for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem)])
    if stage == 'reach_debug':
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        nodes=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSMoonResourceActor)
        return [dict(size=str(a.get_node_size()),work=a.get_remaining_mining_work()) for a in nodes[:25]]
    if stage == 'api':
        return dict(input_value=unreal.InputActionValue.__doc__,
            input_library=[n for n in dir(unreal.EnhancedInputLibrary) if 'value' in n],
            subsystem=unreal.EnhancedInputLocalPlayerSubsystem.inject_input_for_action.__doc__,
            local_player=[n for n in dir(unreal.PlayerController) if 'local_player' in n],
            audio=unreal.AudioMixerLibrary.stop_recording_output.__doc__,
            subsystems=[s.get_path_name() for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem)],
            actions=[a.get_path_name() for a in unreal.ObjectIterator(unreal.InputAction) if 'Look' in a.get_name() or 'Attack' in a.get_name()],
            exports=str(list(unreal.AudioRecordingExportType.__dict__.keys())))
    if stage == 'result': return getattr(unreal,'_mining_forearm_result',None)
    if stage == 'probe': return probe()
    if stage in ('feet','feet_far','waist','head','left','right','behind','distant','occluded'): return probe(stage)
    raise ValueError(stage)


def probe(case='large'):
    import math
    from mining_feedback import sample_strike
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    pawn=unreal.GameplayStatics.get_player_pawn(world,0);pc=unreal.GameplayStatics.get_player_controller(world,0)
    unreal.WidgetLibrary.set_input_mode_game_only(pc,True)
    unreal.WidgetLibrary.set_focus_to_game_viewport()
    old_app_volume=unreal.SystemLibrary.get_console_variable_int_value('au.DisableAppVolume')
    unreal.SystemLibrary.execute_console_command(world,'au.DisableAppVolume 1',pc)
    inventory=pawn.get_component_by_class(unreal.JTSInventoryComponent)
    melee=pawn.get_component_by_class(unreal.JTSMeleeComponent)
    health=pawn.get_component_by_class(unreal.JTSHealthComponent);old_health=health.get_max_health();health.set_max_health(10000,True)
    saved=pawn.get_actor_transform();movement=pawn.character_movement;mode=movement.get_editor_property('movement_mode')
    movement.disable_movement()
    size=unreal.JTSMoonResourceNodeSize.LARGE_METAL_ROCK if case=='large' else unreal.JTSMoonResourceNodeSize.MEDIUM_METAL_ROCK
    node=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSMoonResourceActor)
        if a.get_node_size()==size and a.get_remaining_mining_work()>=(25 if case=='large' else 10))
    saved_node=node.get_actor_transform()
    mesh=node.get_editor_property('resource_mesh');origin,extent,radius=unreal.SystemLibrary.get_component_bounds(mesh)
    up=node.get_actor_up_vector();forward=node.get_actor_forward_vector();right=node.get_actor_right_vector()
    planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
    stand_off=radius+35 if case=='feet_far' else radius-10 if case=='feet' else radius+20 if case!='large' else radius-15
    ground=planet.project_point_to_surface(origin-forward*stand_off)
    pos=ground.impact_point+ground.impact_normal*88
    pawn.set_actor_location(pos,False,True);pawn.set_actor_rotation(unreal.MathLibrary.make_rot_from_xz(forward,up),True)
    pawn.initialize_planet_frame()
    if case!='large':
        shift=up*dict(feet=0,feet_far=0,waist=60,head=140,left=60,right=60,behind=60,distant=60,occluded=60)[case]+right*dict(feet=0,feet_far=0,waist=0,head=0,left=-65,right=65,behind=0,distant=0,occluded=0)[case]
        shift=shift+forward*dict(behind=-220,distant=300).get(case,0)
        node.set_actor_location(node.get_actor_location()+shift,False,True)
        origin=origin+shift
    if not inventory.get_item_count(unreal.JTSItemId.PICKAXE):
        if not inventory.can_add_item(unreal.JTSItemId.PICKAXE,1):
            inventory.call_method('ServerDestroyItemQuantityAtSlot',(0,inventory.get_item_at_slot(0).get_editor_property('stack_count')))
        assert inventory.try_add_item_by_id(unreal.JTSItemId.PICKAXE,1)
    inventory.select_quickbar_slot(next(i for i in range(inventory.get_inventory_capacity()) if inventory.get_item_at_slot(i).get_editor_property('item_id')==unreal.JTSItemId.PICKAXE))
    sub=next(s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if 'LocalPlayer_0.' in s.get_path_name())
    actions={a.get_name():a for a in unreal.ObjectIterator(unreal.InputAction) if a.get_outer()==pawn}
    input_lib=unreal.get_default_object(unreal.EnhancedInputLibrary.static_class())
    def inject(name,value,boolean=False):
        sub.inject_input_vector_for_action(actions[name],unreal.Vector(float(value),0,0),[],[])
    gameplay=unreal.get_default_object(unreal.GameplayStatics.static_class())
    obstacle=None
    if case=='occluded':
        pose=unreal.Transform(location=pos+forward*35,rotation=unreal.MathLibrary.make_rot_from_xz(forward,up))
        obstacle=gameplay.call_method('BeginDeferredActorSpawnFromClass',(world,unreal.StaticMeshActor.static_class(),pose,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        obstacle=gameplay.call_method('FinishSpawningActor',(obstacle,pose,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        obstacle.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        obstacle.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
        obstacle.static_mesh_component.set_collision_profile_name('BlockAll')
        obstacle.set_actor_scale3d(unreal.Vector(.18,1.8,2.8))
    capture_pose=unreal.Transform(location=pos-forward*70+right*300+up*60,
        rotation=unreal.MathLibrary.make_rot_from_xz(pos+forward*35-(pos-forward*70+right*300+up*60),up))
    capture=gameplay.call_method('BeginDeferredActorSpawnFromClass',(world,unreal.SceneCapture2D.static_class(),capture_pose,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    capture=gameplay.call_method('FinishSpawningActor',(capture,capture_pose,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    light_pose=unreal.Transform(location=pos+up*160+right*100)
    lamp=gameplay.call_method('BeginDeferredActorSpawnFromClass',(world,unreal.PointLight.static_class(),light_pose,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    lamp=gameplay.call_method('FinishSpawningActor',(lamp,light_pose,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    lamp.point_light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    lamp.point_light_component.set_editor_property('intensity_units',unreal.LightUnits.CANDELAS)
    lamp.point_light_component.set_intensity(130);lamp.point_light_component.set_attenuation_radius(1400)
    c=capture.capture_component2d
    tex=unreal.RenderingLibrary.create_render_target2d(world,1280,900,unreal.TextureRenderTargetFormat.RTF_RGBA8)
    c.set_editor_property('texture_target',tex);c.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    c.set_editor_property('capture_every_frame',False);c.set_editor_property('always_persist_rendering_state',True)
    c.set_editor_property('fov_angle',65.)
    folder=Path(unreal.Paths.project_dir()).resolve()/('Saved/MiningForearmQA' if case=='large' else 'Saved/MiningReachQA/'+case);folder.mkdir(parents=True,exist_ok=True)
    result=dict(case=case,pending=True,real_player_camera=pc.get_view_target()==pawn,frames=[],before=node.get_remaining_mining_work(),images={},initial_confirmed=pawn.get_component_by_class(unreal.JTSMiningStrikeComponent).get_confirmed_impact_count())
    unreal._mining_forearm_result=result
    dot=lambda a,b:a.x*b.x+a.y*b.y+a.z*b.z
    def local(point):
        delta=point-pawn.get_actor_location();return [dot(delta,pawn.get_actor_forward_vector()),dot(delta,pawn.get_actor_right_vector()),dot(delta,pawn.get_actor_up_vector())]
    class Pending:
        def __init__(self):
            self.at=unreal.GameplayStatics.get_time_seconds(world);self.phase='aim';self.last_swing=-100
            self.handle=unreal.register_slate_post_tick_callback(self.tick);melee.on_attack_started.add_callable(self.swing)
        def swing(self,kind):self.last_swing=unreal.GameplayStatics.get_time_seconds(world)
        def image(self,name,hud=False):
            if hud:unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename="'+str(folder/(name+'.png'))+'" -nosuffix',pc)
            else:c.capture_scene();unreal.RenderingLibrary.export_render_target(world,tex,str(folder),name+'.png')
            result['images'][name]=str(folder/(name+'.png'))
        def aim(self):
            cam=pc.player_camera_manager;direction=(origin-cam.get_camera_location()).normal()
            current=cam.get_camera_rotation().get_forward_vector()
            pitch=math.degrees(math.asin(max(-1,min(1,dot(direction,up))))-math.asin(max(-1,min(1,dot(current,up)))))
            flat=(current-up*dot(current,up)).normal();goal=(direction-up*dot(direction,up)).normal()
            cross=unreal.MathLibrary.cross_vector_vector(flat,goal)
            yaw=math.degrees(math.atan2(dot(cross,up),dot(flat,goal)))
            inject('LookPitchAction',pitch/.07*.18);inject('LookYawAction',yaw/.07*.18)
            result['aim_error_degrees']=[pitch,yaw]
        def finish(self):
            inject('AttackAction',0,True);melee.attack_released();melee.stop_attack()
            melee.on_attack_started.remove_callable(self.swing);unreal.unregister_slate_post_tick_callback(self.handle)
            capture.destroy_actor();lamp.destroy_actor()
            if obstacle:obstacle.destroy_actor()
            if unreal.SystemLibrary.is_valid(node):node.set_actor_transform(saved_node,False,True)
            pawn.set_actor_transform(saved,False,True);pawn.initialize_planet_frame();movement.set_movement_mode(mode)
            health.set_max_health(old_health,False);result['pending']=False
            unreal.SystemLibrary.execute_console_command(world,'au.DisableAppVolume '+str(old_app_volume),pc)
            (folder/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
        def tick(self,dt):
            try:
                now=unreal.GameplayStatics.get_time_seconds(world);elapsed=now-self.at;health.heal(100)
                if self.phase in ('aim','baseline','strike'):self.aim()
                if self.phase in ('aim','baseline','release'):inject('AttackAction',0,True)
                if self.phase=='aim' and elapsed>2.:
                    self.image('idle_side');self.image('idle_game',True)
                    unreal.AudioMixerLibrary.start_recording_output(world,2.)
                    self.phase='baseline';self.at=now;return
                if self.phase=='baseline' and elapsed>1:
                    unreal.AudioMixerLibrary.stop_recording_output(world,unreal.AudioRecordingExportType.WAV_FILE,'baseline',str(folder))
                    unreal.AudioMixerLibrary.start_recording_output(world,3.)
                    self.phase='strike';self.at=now;return
                if self.phase=='strike':
                    inject('AttackAction',1,True)
                    frame=sample_strike(pawn);frame.update(phase='strike',seconds=elapsed,cycle=now-self.last_swing)
                    for bone in ('UpperArm_R','LowerArm_R','Wrist_R','Index1_R'):
                        frame[bone]=local(pawn.mesh.get_socket_location(bone))
                    frame['sparks']=sum(1 for comp in node.get_components_by_class(unreal.StaticMeshComponent) if comp.get_material(0) and comp.get_material(0).get_name()=='M_MiningSpark')
                    result['frames'].append(frame)
                    if elapsed>(.4 if case=='large' else 0):
                        for name,start,end in [('raised',.01,.08),('cut',.10,.15),('hit',.16,.22),('rebound',.25,.32)]:
                            if start<frame['cycle']<end and name not in result['images']:self.image(name)
                        if frame['sparks'] and 'sparks_game' not in result['images']:self.image('sparks_game',True)
                    if elapsed>(1.35 if case=='large' else .24):
                        inject('AttackAction',0,True);melee.attack_released()
                        result['after']=node.get_remaining_mining_work() if unreal.SystemLibrary.is_valid(node) else 0
                        unreal.AudioMixerLibrary.stop_recording_output(world,unreal.AudioRecordingExportType.WAV_FILE,'striking',str(folder))
                        self.phase='release';self.at=now;return
                if self.phase=='release':
                    frame=sample_strike(pawn);frame.update(phase='release',seconds=elapsed);result['frames'].append(frame)
                    if elapsed>.6:
                        self.image('released_side');self.image('released_game',True)
                        result['confirmed']=pawn.get_component_by_class(unreal.JTSMiningStrikeComponent).get_confirmed_impact_count()
                        result['camera']=str(pc.player_camera_manager.get_camera_location());result['node']=str(origin)
                        cam=pc.player_camera_manager
                        hit=unreal.SystemLibrary.line_trace_single(world,cam.get_camera_location(),cam.get_camera_location()+cam.get_camera_rotation().get_forward_vector()*1400,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[pawn],unreal.DrawDebugTrace.NONE,True)
                        if hit:
                            parts=gameplay.call_method('BreakHitResult',(hit,))
                            result['camera_hit']=str(parts);result['hit_local']=local(parts[5])
                            hand=unreal.SystemLibrary.line_trace_single(world,pawn.get_actor_location()+up*35,parts[5]+cam.get_camera_rotation().get_forward_vector()*3,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[pawn],unreal.DrawDebugTrace.NONE,True)
                            if hand:
                                hand_parts=gameplay.call_method('BreakHitResult',(hand,));result['hand_hit']=str(hand_parts);result['hand_local']=local(hand_parts[5])
                        impact=node.get_component_by_class(unreal.JTSMiningImpactComponent) if unreal.SystemLibrary.is_valid(node) else None
                        if impact:result['impact_config']=dict(spark=str(impact.get_editor_property('spark_material')),sounds=str(impact.get_editor_property('impact_sounds')),count=impact.get_impact_count())
                        result['pose_config']={n:str(pawn.get_component_by_class(unreal.JTSMiningStrikeComponent).get_editor_property(n)) for n in ('elbow_reference_offset','forearm_reach')}
                        result['origin_local']=local(origin)
                        result['forearm_reference']=str(pawn.get_component_by_class(unreal.JTSMiningStrikeComponent).get_editor_property('elbow_reference_offset'))
                        if case in ('behind','distant','occluded'):
                            assert result['before']==result['after'],'Target hit outside valid reach or through a blocker'
                        else:
                            assert result['confirmed']-result['initial_confirmed']>=(3 if case=='large' else 1),'Actual input/camera did not produce mining contacts'
                            assert any(f.get('sparks',0)>0 for f in result['frames']),'No surface sparks'
                        self.finish()
            except Exception:
                import traceback
                result['error']=traceback.format_exc();self.finish()
    unreal._mining_forearm_probe=Pending()
    return dict(started=True)
