"""Scoped boss presentation authoring and disposable PIE verification."""
import unreal, json, math
from pathlib import Path
from mirror_boss_editor import FOLDER, ART, BPS, save, protected, world
from mirror_boss_validation import worlds, get, server
from pickaxe_combat_validation import spawn, capture
GS=unreal.GameplayStatics
OUT=Path(unreal.Paths.project_dir()).resolve()/'Saved/MirrorBoss'

def record_emergence():
    w=server();enc=get(w,unreal.JTSNestBossEncounter)[0];planet=enc.get_editor_property('planet')
    from mirror_boss_validation import position_players
    position_players(750)
    cameras=[]
    for pw in worlds():
        center=enc.get_actor_location();up=planet.get_radial_up_vector(center);f=enc.get_actor_forward_vector()
        for pc in get(pw,unreal.PlayerController):
            if not pc.is_local_controller():continue
            loc=center+up*460+f*950+up.cross(f)*300
            camera=spawn(pw,unreal.CameraActor,loc,center+up*100-loc,up)
            pc.set_view_target_with_blend(camera,0)
            cameras.append((pw,pc,camera))
    samples=[];start=GS.get_time_seconds(w)
    def tick(dt):
        age=GS.get_time_seconds(w)-start
        for pw,pc,camera in cameras:
            delta=unreal.MathLibrary.normalized_delta_rotator(pc.player_camera_manager.get_camera_rotation(),camera.get_actor_rotation())
            samples.append(dict(age=age,world=pw.get_path_name(),rotation=max(abs(delta.pitch),abs(delta.yaw),abs(delta.roll)),translation=pc.player_camera_manager.get_camera_location().distance(camera.get_actor_location())))
        if age>7:
            unreal.unregister_slate_post_tick_callback(unreal._boss_feedback_tick)
            (OUT/'FeedbackEmergenceSamples.json').write_text(json.dumps(samples,indent=2))
    unreal._boss_feedback_tick=unreal.register_slate_post_tick_callback(tick)
    for n in enc.get_editor_property('nests'):n.get_component_by_class(unreal.JTSHealthComponent).apply_damage(100000,None,None,True)
    return dict(recording=True,local_cameras=len(cameras))

def record_death():
    w=server();boss=get(w,unreal.JTSPlanetBoss)[0];planet=get(w,unreal.JTSNestBossEncounter)[0].get_editor_property('planet')
    center=boss.get_actor_location();up=planet.get_radial_up_vector(center);f=boss.get_actor_forward_vector();right=up.cross(f)
    # Capture at known presentation times from a fixed lunar frame.
    samples=[];done=set();start=GS.get_time_seconds(w)
    def tick(dt):
        age=GS.get_time_seconds(w)-start
        for pw in worlds():
            for b in get(pw,unreal.JTSPlanetBoss):
                mat=b.mesh.get_material(0)
                opacity=mat.call_method('K2_GetScalarParameterValue',('CorpseOpacity',)) if isinstance(mat,unreal.MaterialInstanceDynamic) else 1
                samples.append(dict(age=age,world=pw.get_path_name(),opacity=opacity,body=str(b.mesh.get_socket_location('body')),action=str(b.combat.get_action())))
        for at,name in [(1,'DeathDetach'),(3.7,'DeathHold'),(6.85,'DeathHalfTransparent')]:
            if age>=at and name not in done:
                done.add(name);capture(w,center+f*750+right*620+up*430,center+up*65,up,name)
        if age>10:
            unreal.unregister_slate_post_tick_callback(unreal._boss_feedback_tick)
            (OUT/'FeedbackDeathSamples.json').write_text(json.dumps(samples,indent=2))
    unreal._boss_feedback_tick=unreal.register_slate_post_tick_callback(tick)
    boss.health.apply_damage(100000,None,None,True)
    return dict(recording=True)

def configure():
    from moon_surface_unreal import fbx_task, import_tasks
    before=protected()
    mesh=unreal.load_asset(FOLDER+'/SK_MirrorBoss')
    import_tasks([fbx_task(ART/'A_MirrorBoss_Death.fbx',FOLDER+'/Animations','A_MirrorBoss_Death',True,True,mesh.skeleton)])
    clip=unreal.load_asset(FOLDER+'/Animations/A_MirrorBoss_Death');save(clip)
    lib=unreal.MaterialEditingLibrary
    fade=[]
    for slot in mesh.materials:
        original=slot.material_interface
        name=original.get_name()+'_CorpseFade';path=FOLDER+'/Materials/'+name
        mat=unreal.load_asset(path)
        if not mat:mat=unreal.EditorAssetLibrary.duplicate_asset(original.get_path_name(),path)
        mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
        mat.set_editor_property('two_sided',False)
        opacity=lib.create_material_expression(mat,unreal.MaterialExpressionScalarParameter)
        opacity.set_editor_property('parameter_name','CorpseOpacity');opacity.set_editor_property('default_value',1)
        lib.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
        lib.recompile_material(mat);save(mat);fade.append(mat)
    bp=unreal.load_asset(BPS+'/BP_MirrorBoss');cdo=unreal.get_default_object(bp.generated_class())
    component=cdo.get_component_by_class(unreal.JTSBossDeathPresentationComponent)
    component.set_editor_property('fade_materials',fade)
    component.set_editor_property('hold_duration',1.4);component.set_editor_property('fade_duration',4.5)
    tremor=cdo.get_component_by_class(unreal.JTSBossEmergenceFeedbackComponent)
    tremor.set_editor_property('radius',6000);tremor.set_editor_property('maximum_height',600)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
    assert protected()==before,'Protected surface or corpse changed'
    return dict(clip=clip.get_path_name(),fade_materials=[m.get_name() for m in fade],radius=6000,protected=True)

def run(stage):
    if stage=='boss_feedback_close':
        run('boss_feedback_restore')
        unreal.SystemLibrary.quit_editor()
        return dict(closing=True)
    if stage=='boss_feedback_diagnose_pickup':
        w=server();key=get(w,unreal.JTSExpeditionKeyPickup)[0];rows=[]
        for pawn in get(w,unreal.JTSCharacter):
            eye,rot=pawn.get_actor_eyes_view_point()
            hit=unreal.SystemLibrary.line_trace_single(w,eye,key.get_actor_location(),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[pawn,key],unreal.DrawDebugTrace.NONE,True)
            rows.append(dict(pawn=pawn.get_name(),can=key.can_interact(pawn),distance=pawn.get_actor_location().distance(key.get_actor_location()),trace=str(hit),local=pawn.get_controller().is_local_controller()))
        pawn=next(p for p in get(w,unreal.JTSCharacter) if not p.get_controller().is_local_controller())
        key.interact(pawn)
        return dict(rows=rows,direct_server_collected=not unreal.SystemLibrary.is_valid(key))
    if stage=='boss_feedback_enable_hud':
        manager=get(server(),unreal.JTSSpaceWorldManager)[0]
        manager.set_travel_state(unreal.JTSSpaceTravelState.APPROACH)
        manager.set_travel_state(unreal.JTSSpaceTravelState.SURFACE)
        for pw in worlds():
            for pc in get(pw,unreal.PlayerController):
                if pc.is_local_controller():pc.call_method('ClientSetHUD',(unreal.JTSPrototypeHUD.static_class(),))
        return dict(hud=True)
    if stage=='boss_feedback_respawn':
        w=server();old=get(w,unreal.JTSNestBossEncounter)[0]
        enc=spawn(w,unreal.JTSNestBossEncounter,old.get_actor_location(),old.get_actor_forward_vector(),old.get_actor_up_vector())
        for p in ['planet','nests','boss_class']:enc.set_editor_property(p,old.get_editor_property(p))
        enc.set_editor_property('wake_delay',.1)
        return dict(respawning=True)
    if stage=='boss_feedback_focus_key':
        w=server();key=get(w,unreal.JTSExpeditionKeyPickup)[0]
        planet=next(p for p in get(w,unreal.JTSPlanetAnchor) if str(p.get_planet_id())=='Moon')
        for i,pawn in enumerate(get(w,unreal.JTSCharacter)):
            hit=planet.project_point_to_surface(key.get_actor_location()-key.get_actor_forward_vector()*140+key.get_actor_right_vector()*(i*320-160))
            pawn.set_actor_location(hit.impact_point+hit.impact_normal*96,False,True)
            inventory=pawn.get_inventory_component()
            for _ in range(inventory.get_inventory_capacity()+1):inventory.try_add_item_by_id(unreal.JTSItemId.POWER_HAMMER,1)
        for pw in worlds():
            localkey=get(pw,unreal.JTSExpeditionKeyPickup)[0]
            for pc in get(pw,unreal.PlayerController):
                if not pc.is_local_controller():continue
                loc=localkey.get_actor_location()-localkey.get_actor_forward_vector()*85+localkey.get_actor_up_vector()*90
                camera=spawn(pw,unreal.CameraActor,loc,localkey.get_actor_location()-loc,localkey.get_actor_up_vector())
                pc.set_view_target_with_blend(camera,0)
        return dict(focused=True)
    if stage=='boss_feedback_prompt':
        rows=[]
        for pw in worlds():
            for pc in get(pw,unreal.PlayerController):
                if not pc.is_local_controller():continue
                pawn=pc.get_controlled_pawn();interaction=pawn.get_component_by_class(unreal.InteractionComponent);interaction.refresh_interactable()
                library=unreal.get_default_object(unreal.load_class(None,'/Script/UMG.WidgetBlueprintLibrary'))
                huds=library.call_method('GetAllWidgetsOfClass',(pw,unreal.JTSPrototypeHUDWidget.static_class(),False))
                texts=[]
                for hud in huds:
                    text=unreal.find_object(None,hud.get_path_name()+'.WidgetTree.InteractionPromptText')
                    texts.append(dict(text=str(text.get_text()),visibility=str(text.get_visibility())))
                rows.append(dict(world=pw.get_path_name(),target=str(interaction.get_current_interactable()),prompt=str(interaction.get_current_interaction_prompt()),hud=texts,hud_actor=str(pc.get_hud()),phase=str(GS.get_game_state(pw).get_gameplay_phase()),slots=pawn.get_inventory_component().get_used_slot_count()))
        return rows
    if stage=='boss_feedback_collect':
        pw=next(pw for pw in worlds() if not GS.get_game_mode(pw))
        pc=next(pc for pc in get(pw,unreal.PlayerController) if pc.is_local_controller());pawn=pc.get_controlled_pawn()
        inventory=pawn.get_inventory_component();assert inventory.get_used_slot_count()==inventory.get_inventory_capacity()
        interaction=pawn.get_component_by_class(unreal.InteractionComponent)
        assert isinstance(interaction.get_current_interactable(),unreal.JTSExpeditionKeyPickup)
        return dict(client_intent=interaction.try_interact(),full_inventory=inventory.get_used_slot_count())
    if stage=='boss_feedback_collected':
        return [dict(world=pw.get_path_name(),keys=len(get(pw,unreal.JTSExpeditionKeyPickup)),shared=GS.get_game_state(pw).has_important_item('MarsEcologyAccess')) for pw in worlds()]
    if stage=='boss_feedback_emerge':return record_emergence()
    if stage=='boss_feedback_death':return record_death()
    if stage=='boss_feedback_unthrottle':
        perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
        if not hasattr(unreal,'_boss_feedback_throttle'):unreal._boss_feedback_throttle=perf.get_editor_property('bThrottleCPUWhenNotForeground')
        perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
        unreal.SystemLibrary.execute_console_command(world(),'t.IdleWhenNotForeground 0')
        return dict(unthrottled=True)
    if stage=='boss_feedback_setup':
        run('boss_feedback_unthrottle')
        settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
        unreal._boss_feedback_settings={p:settings.get_editor_property(p) for p in ['PlayNumberOfClients','RunUnderOneProcess','AdditionalServerGameOptions']}
        settings.set_editor_property('PlayNumberOfClients',2);settings.set_editor_property('RunUnderOneProcess',True)
        settings.set_editor_property('AdditionalServerGameOptions','?game=/Script/space.JTSGameplayGameModeBase')
        unreal._boss_feedback_mode=world().get_world_settings().get_editor_property('default_game_mode')
        world().get_world_settings().set_editor_property('default_game_mode',unreal.JTSGameplayGameModeBase)
        gm=unreal.get_default_object(unreal.JTSGameplayGameModeBase)
        unreal._boss_feedback_build=gm.get_editor_property('require_matching_build_version');gm.set_editor_property('require_matching_build_version',False)
        subsystem=unreal.get_default_object(unreal.JTSExpeditionSubsystem)
        unreal._boss_feedback_prefix=subsystem.get_editor_property('save_slot_prefix');subsystem.set_editor_property('save_slot_prefix','MirrorBossFeedback_Disposable_QA')
        return dict(clients=2)
    if stage=='boss_feedback_restore':
        if hasattr(unreal,'_boss_feedback_settings'):
            settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
            for p,v in unreal._boss_feedback_settings.items():settings.set_editor_property(p,v)
            world().get_world_settings().set_editor_property('default_game_mode',unreal._boss_feedback_mode)
            unreal.get_default_object(unreal.JTSGameplayGameModeBase).set_editor_property('require_matching_build_version',unreal._boss_feedback_build)
            unreal.get_default_object(unreal.JTSExpeditionSubsystem).set_editor_property('save_slot_prefix',unreal._boss_feedback_prefix)
            del unreal._boss_feedback_settings
        if hasattr(unreal,'_boss_feedback_throttle'):
            unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings')).set_editor_property('bThrottleCPUWhenNotForeground',unreal._boss_feedback_throttle)
            del unreal._boss_feedback_throttle
        return dict(restored=True)
    if stage=='boss_feedback_configure':return configure()
    if stage=='boss_feedback_tests':
        unreal.SystemLibrary.execute_console_command(world(),'Automation RunTests JTS.Moon.Boss')
        return dict(tests_started=True)
    if stage=='boss_feedback_inspect':
        bp=unreal.load_asset(BPS+'/BP_MirrorBoss');cdo=unreal.get_default_object(bp.generated_class())
        return dict(components=[c.get_class().get_name() for c in cdo.get_components_by_class(unreal.ActorComponent)],
            clip=str(cdo.get_editor_property('animations')[unreal.JTSBossAction.DEATH]),protected=protected())
    raise ValueError(stage)
