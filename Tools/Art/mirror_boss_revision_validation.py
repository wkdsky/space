"""PIE-only checks of grounded pivots, local tremor and shared important-item progression."""
import unreal, math, json
from pathlib import Path
from mirror_boss_validation import worlds, get, server, position_players, summary
from pickaxe_combat_validation import spawn
GS=unreal.GameplayStatics
OUT=Path(unreal.Paths.project_dir()).resolve()/'Saved/MirrorBoss'

def navigation():
    rows=[]
    for w in worlds():
        manager=get(w,unreal.JTSSpaceWorldManager)[0]
        ships=get(w,unreal.JTSSpacecraftActor)
        if not ships:
            ship=spawn(w,unreal.JTSSpacecraftActor,manager.get_actor_location(),unreal.Vector(1,0,0),unreal.Vector(0,0,1))
        else:ship=ships[0]
        bodies=unreal.JTSCelestialNavigationLibrary.gather_bodies(manager,ship)
        mars=next(b for b in bodies if b.planet and str(b.planet.get_planet_id())=='Mars')
        state=GS.get_game_state(w)
        rows.append(dict(world=w.get_path_name(),key=state.has_important_item('MarsEcologyAccess'),chart=mars.show_on_star_chart,marker=mars.show_screen_marker))
    return rows

def local_cameras():
    enc=get(server(),unreal.JTSNestBossEncounter)[0]
    result=[]
    for w in worlds():
        for pc in get(w,unreal.PlayerController):
            if not pc.is_local_controller():continue
            pawn=pc.get_controlled_pawn()
            p=next(a for a in get(w,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
            center=enc.get_actor_location();up=p.get_radial_up_vector(center);f=enc.get_actor_forward_vector()
            pos=center+up*450+f*1100+up.cross(f)*400
            camera=spawn(w,unreal.CameraActor,pos,center+up*95-pos,up)
            pc.set_view_target_with_blend(camera,0)
            result.append((pc,camera))
    return result

def run(stage):
    if stage=='boss_qa_revision_positions':
        return [dict(world=w.get_path_name(),players=[dict(name=p.get_name(),location=str(p.get_actor_location()),controlled=bool(p.get_controller())) for p in get(w,unreal.JTSCharacter)],controllers=[dict(name=pc.get_name(),pawn=str(pc.get_controlled_pawn()),local=pc.is_local_controller()) for pc in get(w,unreal.PlayerController)],keys=len(get(w,unreal.JTSExpeditionKeyPickup))) for w in worlds()]
    if stage=='boss_qa_revision_nav':return navigation()
    if stage=='boss_qa_revision_ship_to_moon':
        enc=get(server(),unreal.JTSNestBossEncounter)[0];p=enc.get_editor_property('planet');point=enc.get_actor_location()+p.get_radial_up_vector(enc.get_actor_location())*1000
        for w in worlds():
            for ship in get(w,unreal.JTSSpacecraftActor):
                ship.set_actor_location(point,False,True);ship.set_actor_tick_enabled(False)
                for comp in ship.get_components_by_class(unreal.ActorComponent):comp.set_component_tick_enabled(False)
            for manager in get(w,unreal.JTSSpaceWorldManager):
                moon=next(a for a in get(w,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
                manager.set_current_planet(moon)
        return dict(ship_at_moon=True)
    if stage=='boss_qa_revision_key_flags':
        result=[]
        for a in get(server(),unreal.JTSExpeditionKeyPickup):
            row={p:str(a.get_editor_property(p)) for p in ['replicates','always_relevant','only_relevant_to_owner','net_use_owner_relevancy','net_cull_distance_squared','net_dormancy','hidden']}
            row['default_hidden']=str(unreal.get_default_object(a.get_class()).get_editor_property('hidden'))
            result.append(row);a.set_actor_hidden_in_game(False);a.set_actor_enable_collision(True);a.force_net_update()
        return result
    if stage=='boss_qa_revision_emerge_again':
        w=server();old=get(w,unreal.JTSNestBossEncounter)[0]
        enc=spawn(w,unreal.JTSNestBossEncounter,old.get_actor_location(),old.get_actor_forward_vector(),old.get_actor_up_vector())
        for p in ['planet','nests','boss_class']:enc.set_editor_property(p,old.get_editor_property(p))
        enc.set_editor_property('wake_delay',.1)
        return run('boss_qa_revision_emerge')
    if stage=='boss_qa_revision_emerge':
        enc=get(server(),unreal.JTSNestBossEncounter)[0];p=enc.get_editor_property('planet')
        for i,pawn in enumerate(get(server(),unreal.JTSCharacter)):
            center=enc.get_actor_location();up=p.get_radial_up_vector(center)
            hit=p.project_point_to_surface(center+enc.get_actor_forward_vector()*750+up.cross(enc.get_actor_forward_vector())*i*220)
            pawn.set_actor_location(hit.impact_point+up*96,False,True)
        cameras=local_cameras();samples=[]
        start=GS.get_time_seconds(server())
        def tick(dt):
            age=GS.get_time_seconds(server())-start
            for i,(pc,camera) in enumerate(cameras):
                delta=unreal.MathLibrary.normalized_delta_rotator(pc.player_camera_manager.get_camera_rotation(),camera.get_actor_rotation())
                samples.append(dict(age=age,player=i,rotation=max(abs(delta.pitch),abs(delta.yaw),abs(delta.roll)),distance=pc.player_camera_manager.get_camera_location().distance(camera.get_actor_location())))
            if age>6:
                unreal.unregister_slate_post_tick_callback(unreal._boss_revision_tick)
                (OUT/'EmergenceCameraSamples.json').write_text(json.dumps(samples))
        unreal._boss_revision_tick=unreal.register_slate_post_tick_callback(tick)
        enc=get(server(),unreal.JTSNestBossEncounter)[0]
        for n in enc.get_editor_property('nests'):n.get_component_by_class(unreal.JTSHealthComponent).apply_damage(100000,None,None,True)
        return dict(recording=True,cameras=len(cameras))
    if stage=='boss_qa_revision_turn':
        w=server();enc=get(w,unreal.JTSNestBossEncounter)[0];boss=enc.active_boss;p=enc.get_editor_property('planet')
        center=boss.get_actor_location();up=p.get_radial_up_vector(center);forward=boss.get_actor_forward_vector()
        for i,pawn in enumerate(get(w,unreal.JTSCharacter)):
            hit=p.project_point_to_surface(center-forward*290+up.cross(forward)*i*30)
            pawn.set_actor_location(hit.impact_point+up*96,False,True)
        rows=[];start=GS.get_time_seconds(w)
        def tick(dt):
            if not boss:return
            ground=p.project_point_to_surface(boss.get_actor_location())
            rows.append(dict(time=GS.get_time_seconds(w)-start,up=boss.get_actor_up_vector().dot(ground.impact_normal),height=boss.get_actor_location().distance(ground.impact_point),action=str(boss.combat.get_action())))
            if GS.get_time_seconds(w)-start>8:
                unreal.unregister_slate_post_tick_callback(unreal._boss_revision_tick)
                (OUT/'TurnSurfaceSamples.json').write_text(json.dumps(rows))
        unreal._boss_revision_tick=unreal.register_slate_post_tick_callback(tick)
        return dict(recording=True)
    if stage=='boss_qa_revision_loot':
        keys=get(server(),unreal.JTSExpeditionKeyPickup)
        return dict(keys=[dict(name=str(a.get_editor_property('display_name')),id=str(a.get_editor_property('important_item_id')),position=str(a.get_actor_location())) for a in keys],navigation=navigation())
    if stage=='boss_qa_revision_saved':
        return dict(inventories=[dict(used=p.get_inventory_component().get_used_slot_count(),capacity=p.get_inventory_component().get_inventory_capacity()) for p in get(server(),unreal.JTSCharacter)])
    if stage=='boss_qa_revision_prepare_pickup':
        w=server();key=get(w,unreal.JTSExpeditionKeyPickup)[0]
        pawn=next(p for p in get(w,unreal.JTSCharacter) if p.get_controller() and not p.get_controller().is_local_controller())
        p=next(p for p in get(w,unreal.JTSPlanetAnchor) if str(p.get_planet_id())=='Moon')
        hit=p.project_point_to_surface(key.get_actor_location()-key.get_actor_forward_vector()*130)
        pawn.set_actor_location(hit.impact_point+hit.impact_normal*96,False,True)
        pawn.set_actor_rotation(p.make_surface_aligned_rotation(hit.impact_point,key.get_actor_location()-hit.impact_point),False)
        for other in get(w,unreal.JTSCharacter):
            if other!=pawn:
                ground=p.project_point_to_surface(key.get_actor_location()+key.get_actor_right_vector()*700)
                other.set_actor_location(ground.impact_point+ground.impact_normal*96,False,True)
        inventory=pawn.get_inventory_component()
        for _ in range(inventory.get_inventory_capacity()+1):inventory.try_add_item_by_id(unreal.JTSItemId.POWER_HAMMER,1)
        return dict(used=inventory.get_used_slot_count(),capacity=inventory.get_inventory_capacity())
    if stage=='boss_qa_revision_client_pickup':
        w=next(w for w in worlds() if not GS.get_game_mode(w));key=get(w,unreal.JTSExpeditionKeyPickup)[0]
        pawn=next(pc.get_controlled_pawn() for pc in get(w,unreal.PlayerController) if pc.is_local_controller())
        inventory=pawn.get_inventory_component()
        assert inventory.get_used_slot_count()==inventory.get_inventory_capacity()
        comp=pawn.get_component_by_class(unreal.InteractionComponent)
        comp.refresh_interactable()
        assert comp.get_current_interactable()==key,str(comp.get_current_interactable())
        return dict(client_intent_sent=comp.try_interact())
    if stage=='boss_qa_revision_client_focus':
        w=next(w for w in worlds() if not GS.get_game_mode(w));key=get(w,unreal.JTSExpeditionKeyPickup)[0]
        pc=next(pc for pc in get(w,unreal.PlayerController) if pc.is_local_controller());pawn=pc.get_controlled_pawn()
        up=pawn.get_actor_up_vector();loc=pawn.get_actor_location()+up*140-key.get_actor_forward_vector()*180
        camera=spawn(w,unreal.CameraActor,loc,key.get_actor_location()-loc,up)
        pc.set_view_target_with_blend(camera,0)
        return dict(can_interact=key.can_interact(pawn),distance=pawn.get_actor_location().distance(key.get_actor_location()))
    if stage=='boss_qa_revision_pickup':
        w=server();key=get(w,unreal.JTSExpeditionKeyPickup)[0]
        pawn=get(w,unreal.JTSCharacter)[0]
        planet=next(p for p in get(w,unreal.JTSPlanetAnchor) if str(p.get_planet_id())=='Moon')
        hit=planet.project_point_to_surface(key.get_actor_location()-key.get_actor_forward_vector()*130)
        pawn.set_actor_location(hit.impact_point+hit.impact_normal*96,False,True)
        pawn.set_actor_rotation(planet.make_surface_aligned_rotation(hit.impact_point,key.get_actor_location()-hit.impact_point),False)
        inventory=pawn.get_inventory_component()
        for _ in range(inventory.get_inventory_capacity()+1):inventory.try_add_item_by_id(unreal.JTSItemId.POWER_HAMMER,1)
        before=inventory.get_used_slot_count()
        assert before==inventory.get_inventory_capacity(),(before,inventory.get_inventory_capacity())
        assert key.can_interact(pawn)
        key.interact(pawn)
        assert not unreal.SystemLibrary.is_valid(key),'Pickup was not collected'
        assert inventory.get_used_slot_count()==before
        return dict(full_inventory=before,slots_unchanged=True,navigation=navigation())
    raise ValueError(stage)

