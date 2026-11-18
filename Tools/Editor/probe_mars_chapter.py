"""Editor-only interactive probes; all mutations are confined to the running PIE world."""
from pathlib import Path
import json, runpy
import unreal as u

def run():
    actionfile=Path(u.Paths.project_dir()).resolve()/'Saved/MarsProbeAction.txt'
    action=actionfile.read_text(encoding='utf-8-sig').strip() if actionfile.exists() else 'status'
    if action=='access_prepare':
        defaults=u.get_default_object(u.JTSExpeditionSubsystem)
        defaults.set_editor_property('save_slot_prefix','MarsAccessValidation')
        defaults.set_editor_property('save_slot_name','MarsAccessValidation')
        return {'isolated_from_start':True}
    if action=='audio_schema':
        return {'factory':hasattr(u,'SoundAttenuationFactory'),'fields':[n for n in dir(u.SoundAttenuationSettings()) if not n.startswith('_')],'ambient':[n for n in dir(u.AmbientSound) if 'audio' in n.lower()]}
    if action.startswith('play'):
        n=int(action[4:]);settings=u.get_default_object(u.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
        settings.set_editor_property('PlayNumberOfClients',n)
        settings.set_editor_property('RunUnderOneProcess',True)
        return {'prepared_clients':n}
    if action=='apidiag':
        return {'subsystem_api':[n for n in dir(u) if 'subsystem' in n.lower() or 'iterator' in n.lower() or 'objects_of' in n.lower()], 'library':str(getattr(u,'SubsystemLibrary',None))}
    if action=='dirty':
        return {'content':[p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()],'maps':[p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_map_packages()]}
    if action=='editor':
        aa=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
        p=next(a for a in aa if isinstance(a,u.JTSPlanetAnchor) and str(a.get_planet_id())=='Mars')
        marker=next(a for a in aa if a.get_actor_label()=='Mars_Site_Station')
        f=p.get_surface_frame_at(marker.get_actor_location(),u.Vector(1,0,0))
        cp=f.location+f.up*6500-f.forward*9000
        cr=u.MathLibrary.make_rot_from_xz(f.location-cp,f.up)
        u.EditorLevelLibrary.set_level_viewport_camera_info(cp,cr)
        u.EditorLevelLibrary.editor_set_game_view(True)
        u.EditorLevelLibrary.editor_invalidate_viewports()
        return {'frame':str(f),'marker':str(marker.get_actor_location()),'camera':str(u.EditorLevelLibrary.get_level_viewport_camera_info()),'planet':str(p.get_actor_transform()),'surface':str(p.get_editor_property('gameplay_surface_actor').get_actor_transform())}
    worlds=u.EditorLevelLibrary.get_pie_worlds(True)
    if not worlds:return {'playing':False}
    w=next((w for w in worlds if u.GameplayStatics.get_game_state(w) and u.GameplayStatics.get_game_state(w).has_authority()),worlds[0])
    aa=u.GameplayStatics.get_all_actors_of_class(w,u.Actor)
    by={a.get_actor_label():a for a in aa}
    gs=u.GameplayStatics.get_game_state(w);ship=gs.get_active_spacecraft()
    players=[a for a in aa if isinstance(a,u.JTSCharacter) and a.get_controller() and not a.get_component_by_class(u.JTSHealthComponent).call_method('IsDead',())]
    planets={str(a.get_planet_id()):a for a in aa if isinstance(a,u.JTSPlanetAnchor)}
    p=players[0] if players else None
    if action.startswith('access:'):
        return runpy.run_path(str(Path(__file__).with_name('refine_mars_access.py')))['probe'](action[7:],w,worlds,players)
    def prop(a,name):return a.get_editor_property(name)
    def move(target,offset=140,player=p):
        planet=prop(target,'planet') if isinstance(target,u.JTSInteriorZone) else planets['MarII' if 'Deimos' in target.get_actor_label() or 'Jummt' in target.get_actor_label() else 'Mars']
        player.set_gameplay_planet(planet)
        up=planet.get_radial_up_vector(target.get_actor_location())
        player.set_actor_location_and_rotation(target.get_actor_location()+up*offset,target.get_actor_rotation(),False,False)
        player.initialize_planet_frame();player.get_component_by_class(u.CharacterMovementComponent).stop_movement_immediately()
    result={'action':action}
    if action=='interaction_status':
        c=p.get_component_by_class(u.InteractionComponent)
        a=c.get_current_interactable()
        result['interaction']={'target':a.get_actor_label() if a else None,'prompt':str(c.get_current_interaction_prompt()),'progress':c.get_interaction_hold_progress()}
        result['storage']={str(t):ship.get_resource_amount(t) for t in (u.JTSResourceType.ORE,u.JTSResourceType.CRYSTAL,u.JTSResourceType.FOOD,u.JTSResourceType.WATER)}
    elif action.startswith('aim:'):
        a=by[action[4:]];move(a,120)
        up=planets['Mars'].get_radial_up_vector(a.get_actor_location())
        front=a.get_actor_right_vector()
        point=a.get_actor_location()+front*180+a.get_actor_forward_vector()*100+up*120
        p.set_actor_location(point,False,False)
        direction=a.get_actor_location()+up*100-point
        rotation=u.MathLibrary.make_rot_from_xz(direction,up)
        p.set_actor_rotation(rotation,False);p.get_controller().set_control_rotation(rotation)
        result['aimed']=a.get_actor_label()
        result['prompt']=str(a.get_interaction_prompt(p))
        result['executable']=a.can_execute_interaction(p)
    elif action=='capture':
        u.SystemLibrary.execute_console_command(w,'HighResShot 1280x720',p.get_controller())
        result['requested']=True
    elif action=='cancel':p.get_component_by_class(u.InteractionComponent).cancel_interaction()
    elif action=='fast_time':u.GameplayStatics.set_global_time_dilation(w,20)
    elif action=='normal_time':
        u.GameplayStatics.set_global_time_dilation(w,1);u.GameplayStatics.set_game_paused(w,False)
    elif action=='hold':
        c=p.get_component_by_class(u.InteractionComponent);c.refresh_interactable();c.try_interact()
        a=c.get_current_interactable();result['target']=a.get_actor_label() if a else None;result['prompt']=str(c.get_current_interaction_prompt())
    elif action=='boss_art':
        result['art']=[]
        for a in aa:
            if isinstance(a,u.JTSPlanetBoss):
                phase=a.get_component_by_class(u.JTSBossPhaseComponent)
                result['art'].append({'boss':a.get_name(),'phases':len(prop(phase,'phases')),'clips':len(prop(a,'animations')),'bounds':str(a.mesh.get_skeletal_mesh_asset().get_bounds()),'flip':prop(phase,'cooperative_flip')})
    elif action=='connect_test_clients':
        u.SystemLibrary.execute_console_command(w,'jts.Editor.Mars.ConnectClients')
        # The native fixture already issues ClientTravel. A second `open` races
        # the first join and can disconnect a client after admission is closed.
        result['requested']=len(worlds)-1
    elif action=='resume_test_phase':
        u.SystemLibrary.execute_console_command(w,'jts.Editor.Mars.Resume')
        for player in players:player.get_component_by_class(u.JTSHealthComponent).set_invulnerable(True)
    elif action=='save_reload':
        u.SystemLibrary.execute_console_command(w,'jts.Editor.Mars.SaveReload')
        result['requested']=True
    elif action=='load_validation':
        instance=u.GameplayStatics.get_game_instance(w)
        subsystem=next(s for s in u.ObjectIterator(u.JTSExpeditionSubsystem) if s.get_outer()==instance)
        subsystem.set_editor_property('save_slot_prefix','MarsValidation');subsystem.set_editor_property('save_slot_name','MarsValidation')
        result['loaded']=subsystem.load_expedition_slot(4)
    elif action=='assembly_break':
        points=[a for a in aa if a.get_actor_label().startswith('BP_FabricatorAssembly') and not a.health.call_method('IsDead',())]
        result['found']=len(points)
        for a in points:u.GameplayStatics.apply_damage(a,100,u.GameplayStatics.get_player_controller(w,0),p,None)
    elif action=='assembly_status':
        result['points']=[(a.get_name(),a.health.get_health()) for a in aa if a.get_actor_label().startswith('BP_FabricatorAssembly')]
        result['summons']=[(a.get_name(),a.get_owner().get_name() if a.get_owner() else '') for a in aa if isinstance(a,u.JTSPlanetSurfaceCreature) and a.get_owner() and isinstance(a.get_owner(),u.JTSPlanetBoss)]
    elif action in ('isolate','isolate_assembly'):
        instance=u.GameplayStatics.get_game_instance(w)
        subsystem=next(s for s in u.ObjectIterator(u.JTSExpeditionSubsystem) if s.get_outer()==instance)
        subsystem.set_editor_property('save_slot_prefix','MarsValidationAssembly' if action=='isolate_assembly' else 'MarsValidation')
        subsystem.set_editor_property('save_slot_name','MarsValidation')
        result['new_slot']=subsystem.begin_new_expedition_in_slot(4,'Mars implementation verification')
        for player in players:player.get_component_by_class(u.JTSHealthComponent).set_invulnerable(True)
    elif action=='cage':
        a=by['MC_Noctis_Cage'];move(a,110);result['can_use']=a.can_interact(p);a.interact(p)
        result['started_at']=u.GameplayStatics.get_time_seconds(w)
    elif action=='cage_fast':
        a=by['MC_Noctis_Cage'];move(a,110);a.interact(p)
        start=u.GameplayStatics.get_time_seconds(w);handles=[]
        u.GameplayStatics.set_global_time_dilation(w,20)
        def sample_after_ride(dt):
            elapsed=u.GameplayStatics.get_time_seconds(w)-start
            if elapsed<51:return
            distance=p.get_actor_location().distance(prop(a,'destination').get_actor_location())
            movement=p.get_component_by_class(u.CharacterMovementComponent).get_editor_property('movement_mode')
            if movement==u.MovementMode.MOVE_NONE and elapsed<65:return
            report={'elapsed':elapsed,'ride_seconds':prop(a,'ride_seconds'),'distance_cm':distance,'movement':str(movement),'passed':distance<350 and movement in (u.MovementMode.MOVE_WALKING,u.MovementMode.MOVE_FALLING)}
            (Path(u.Paths.project_dir()).resolve()/'Saved/MarsCageProbe.json').write_text(json.dumps(report),encoding='utf8')
            u.GameplayStatics.set_global_time_dilation(w,1);u.GameplayStatics.set_game_paused(w,True)
            u.unregister_slate_post_tick_callback(handles[0])
        handles.append(u.register_slate_post_tick_callback(sample_after_ride))
        result['scheduled']=True
    elif action=='corediag':
        result['cores']=[{'name':a.get_name(),'position':str(a.get_actor_location())} for a in aa if isinstance(a,u.JTSBossCoreDecoy)]
    elif action=='rewarddiag':
        result['rewards']=[{'name':a.get_name(),'item':str(prop(a,'important_item_id')),'position':str(a.get_actor_location())} for a in aa if isinstance(a,u.JTSExpeditionKeyPickup)]
        result['boss_positions']=[(a.get_name(),str(a.get_actor_location())) for a in aa if isinstance(a,u.JTSPlanetBoss)]
    elif action=='badgediag':
        table=u.load_asset('/Game/Space/Environment/MarsSystem/Chapter/Refined/SM_MP_Lab_Bench')
        table.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        result['important']=[s for s in ('Mars_Badge_B1','Mars_Badge_B2','Mars_Badge_B4') if gs.has_important_item(s)]
        result['badges']=[]
        for s in (1,2,4):
            a=by['MC_Badge_B'+str(s)];move(a,200)
            start=p.get_actor_location()+p.get_actor_up_vector()*64;end=a.get_actor_location()
            hit=u.SystemLibrary.line_trace_single(w,start,end,u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[p,a],u.DrawDebugTrace.NONE,True)
            result['badges'].append({'name':a.get_actor_label(),'hit':hit.to_tuple()[9].get_actor_label() if hit.to_tuple()[9] else None,'position':str(end),'view':str(start),'prompt':str(a.get_interaction_prompt(p))})
            a.interact(p)
    elif action=='transitdiag':
        a=by['MC_Noctis_Cage'];dest=prop(a,'destination')
        result['cage']={'can_use':a.can_interact(p),'destination':str(dest.get_actor_location()),'player':str(p.get_actor_location()),'movement':str(p.get_component_by_class(u.CharacterMovementComponent).get_editor_property('movement_mode')),'time':u.GameplayStatics.get_time_seconds(w),'ride_seconds':prop(a,'ride_seconds'),'distance_to_destination':p.get_actor_location().distance(dest.get_actor_location())}
        result['key_settings']=str(u.get_default_object(u.load_class(None,'/Script/space.JTSCoordinateSettings')).get_editor_property('KeyCoordinateGrants'))
        save=u.GameplayStatics.load_game_from_slot('MarsValidation_04',0)
        result['saved']=bool(save)
    elif action=='floordiag':
        result['zones']=[{'label':a.get_actor_label(),'transform':str(a.get_actor_transform()),'extent':str(a.bounds.get_unscaled_box_extent()),'local_player':str(u.MathLibrary.inverse_transform_location(a.get_actor_transform(),p.get_actor_location()))} for a in aa if isinstance(a,u.JTSInteriorZone)][:4]
    elif action=='landingdiag':
        result['arrivals']=[{'name':a.get_name(),'planet':str(prop(a,'planet_id')),'bound':str(prop(a,'planet_anchor')),'ship':str(a.get_spacecraft_arrival_transform())} for a in aa if isinstance(a,u.JTSPlanetArrivalAnchor)]
        result['ship_class']=ship.get_class().get_path_name()
    elif action=='travel':
        pc=u.GameplayStatics.get_player_controller(w,0)
        pc.call_method('ServerRequestDebugPlanet',('Mars',False))
        result['travel_requested']=True
    elif action=='finish_power_test':
        a=by['MC_Sector_C'];move(a,150);a.interact(p)
        for s in 'ABC':by['MC_DustSweep_'+s].set_editor_property('warning_seconds',.1)
    elif action=='repair':
        ship.deposit_resources([u.JTSResourceType.ORE]*40+[u.JTSResourceType.CRYSTAL]*20)
        for s in 'ABC':
            a=by['MC_Sector_'+s];move(a,150);result[s+'_usable']=a.can_interact(p);a.interact(p)
            by['MC_DustSweep_'+s].set_editor_property('warning_seconds',.1)
    elif action=='repair_race':
        ship.deposit_resources([u.JTSResourceType.ORE]*20+[u.JTSResourceType.CRYSTAL]*10)
        a=by['MC_Sector_A']
        for i,player in enumerate(players[:2]):move(a,140+i*80,player)
        result['initial_can_use']=[a.can_interact(player) for player in players[:2]]
        before=ship.get_resource_amount(u.JTSResourceType.ORE)
        for player in players[:2]:a.interact(player)
        result['ore_consumed']=before-ship.get_resource_amount(u.JTSResourceType.ORE)
    elif action=='recovery_race':
        a=by['MC_R_Recovery_ServiceParts']
        for i,player in enumerate(players):move(a,120+i*30,player)
        before={str(t):ship.get_resource_amount(t) for t in (u.JTSResourceType.ORE,u.JTSResourceType.CRYSTAL)}
        for player in players:a.interact(player)
        result['player_count']=len(players)
        result['granted']={str(t):ship.get_resource_amount(t)-before[str(t)] for t in (u.JTSResourceType.ORE,u.JTSResourceType.CRYSTAL)}
    elif action=='coop_badges':
        for i,s in enumerate((1,2,4)):
            a=by['MC_Badge_B'+str(s)];player=players[i%len(players)];move(a,110,player);a.interact(player)
        a=by['MC_SeedVault_Door'];player=players[-1];move(a,130,player);result['shared_gate_can_open']=a.can_interact(player);a.interact(player)
    elif action in ('ice2','ice3'):
        ice=by['MC_B3_IceLoad'];count=int(action[-1])
        for i,player in enumerate(players):
            if i<count:
                move(ice,0,player);player.set_actor_location(player.get_actor_location()+ice.get_actor_right_vector()*(i-1)*160,False,False)
            else:move(by['MC_Hub_Zone'],-100,player)
    elif action=='ice_status':
        ice=by['MC_B3_IceLoad'];floor=by['MC_B3_BrittleIce']
        result['ice']={'triggered':prop(ice,'triggered'),'warning':prop(ice,'warning'),'collision':floor.get_actor_enable_collision()}
    elif action=='transit':
        a=by['MC_Station_Elevator'];move(a,130);result['can_use']=a.can_interact(p);a.interact(p)
    elif action=='badges':
        for s in (1,2,4):
            a=by['MC_Badge_B'+str(s)];move(a,110);result['badge_'+str(s)]=a.can_interact(p);a.interact(p)
        a=by['MC_SeedVault_Door'];move(a,130);result['gate_can_open']=a.can_interact(p);a.interact(p)
    elif action=='tuantuan':move(by['MC_Encounter_Tuantuan'],150)
    elif action in ('boss_phase2','boss_phase3','boss_kill'):
        bosses=[a for a in aa if isinstance(a,u.JTSPlanetBoss) and not a.health.call_method('IsDead',())]
        if not bosses:return {'error':'No live boss'}
        b=bosses[0];health=b.health.get_health();maximum=b.health.get_max_health()
        damage=max(0,health-maximum*(.60 if action=='boss_phase2' else .28)) if action!='boss_kill' else maximum*20
        targets=[a for a in aa if isinstance(a,u.JTSBossCoreDecoy)] if action=='boss_kill' else []
        result['before']=health
        for target in targets or [b]:u.GameplayStatics.apply_damage(target,damage,u.GameplayStatics.get_player_controller(w,0),p,None)
        result['after']=b.health.get_health()
    elif action=='collect_reward':
        for a in aa:
            if isinstance(a,u.JTSExpeditionKeyPickup) and str(prop(a,'important_item_id')) in ('Mars_TrackingCollar','Mars_LogisticsChip'):
                item=str(prop(a,'important_item_id'));move(a,110);result['reward_usable']=a.can_interact(p);a.interact(p);result['collected']=item;result['granted']=gs.has_important_item(item)
    elif action=='nests':
        for s in ('IceVein','Camp'):
            a=by['MC_Nest_'+s];move(a,100)
            for _ in range(20):u.GameplayStatics.apply_damage(a,25,u.GameplayStatics.get_player_controller(w,0),p,None)
            result[s]=a.is_entrance_blocked()
    elif action=='fabricator':move(by['MC_Encounter_Fabricator'],150)
    elif action=='merchant':
        a=by['MC_Jummt'];move(a,130);a.interact(p);result['gift_attempted']=True
    elif action=='hull':
        h=ship.get_component_by_class(u.JTSSpacecraftDurabilityComponent);move(ship,500)
        u.GameplayStatics.apply_damage(ship,9999,u.GameplayStatics.get_player_controller(w,0),p,None)
        result['disabled']=prop(h,'disabled');result['hull_after_damage']=prop(h,'hull')
        result['without_materials']=h.reconstruct(p)
        ship.deposit_resources([u.JTSResourceType.ROCK]*20+[u.JTSResourceType.ORE]*10)
        result['with_materials']=h.reconstruct(p);result['hull_after_repair']=prop(h,'hull')
    elif action=='split':
        for i,player in enumerate(players):move(by['MC_B2_Zone'] if i%2 else by['MC_Station_Arrival'],-100 if i%2 else 140,player)
    elif action=='all_inside':
        for i,player in enumerate(players):move(by['MC_'+('Hub','B1','B2','B4')[i%4]+'_Zone'],-100,player)
    result['server']={'planet':gs.get_current_planet_id(),'coordinates':list(map(str,prop(gs,'loaded_coordinate_ids'))),'flags':list(map(str,prop(gs,'facility_flag_ids'))),'blocked':list(map(str,prop(gs,'blocked_entrance_ids'))),'defeated':list(map(str,prop(gs,'defeated_encounter_ids'))),'bosses':[(a.get_name(),a.health.get_health(),prop(a.get_component_by_class(u.JTSBossPhaseComponent),'phase_index')) for a in aa if isinstance(a,u.JTSPlanetBoss)]}
    result['worlds']=[]
    for world in worlds:
        state=u.GameplayStatics.get_game_state(world)
        if not state:continue
        if not isinstance(state,u.JTSGameState):
            result['worlds'].append({'world':world.get_name(),'state_class':state.get_class().get_name()});continue
        shared_ship=state.get_active_spacecraft()
        result['worlds'].append({'world':world.get_name(),'authority':state.has_authority(),'flags':list(map(str,prop(state,'facility_flag_ids'))),'storage':{str(t):shared_ship.get_resource_amount(t) for t in (u.JTSResourceType.ORE,u.JTSResourceType.CRYSTAL)} if shared_ship else {},'players':[(a.get_name(),str(prop(a.get_component_by_class(u.JTSInteriorStateComponent),'zone')),str(a.get_actor_location())) for a in u.GameplayStatics.get_all_actors_of_class(world,u.JTSCharacter)]})
    return result




