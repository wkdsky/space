"""Opt-in PIE checks. Uses isolated expedition data and ordinary movement input."""
import json
import runpy
from pathlib import Path
import unreal as u

SAVED=Path(u.Paths.project_saved_dir())

def walking_route(aa,planet):
    recipe=runpy.run_path(str(Path(__file__).with_name('rebuild_mars_station_structure.py')))
    hub,rooms=recipe['layout'](aa,planet)
    authored=json.loads((SAVED/'MarsStationInteriorAuthoring.json').read_text())['corridors']
    route=[]
    for c in authored:
        points=[u.Vector(*q) for q in c['points']]
        path=[hub.translation]+points[::3]+[points[-1]]
        name=c['name'][5:]
        if name=='SeedVault':
            forward=rooms[name]['t'].rotation.get_axis_y();end=points[-1]
            # Do not include a sampled endpoint on the locked door before the approach point.
            path=[hub.translation]+[q for q in points[::3] if (q-end).dot(forward)<-240]+[end-forward*240]
        else:path += [rooms[name]['t'].transform_location(u.Vector(0,-2940,0)),rooms[name]['t'].transform_location(u.Vector(0,-2250,0))]
        route += [(q,name) for q in path+list(reversed(path[:-1]))]
    return hub,route

def run(action):
    worlds=u.EditorLevelLibrary.get_pie_worlds(True)
    server=next(w for w in worlds if isinstance(u.GameplayStatics.get_game_state(w),u.JTSGameState) and u.GameplayStatics.get_game_state(w).has_authority())
    aa={a.get_actor_label():a for a in u.GameplayStatics.get_all_actors_of_class(server,u.Actor)}
    planet=next(a for a in aa.values() if isinstance(a,u.JTSPlanetAnchor) and str(a.get_planet_id())=='Mars')
    players=[a for a in aa.values() if isinstance(a,u.JTSCharacter) and a.get_controller()]
    if action=='asset_geometry':
        out={}
        for key in ('Tracker','Canopy'):
            mesh=u.load_asset('/Game/Space/Environment/MarsSystem/Chapter/Refined/SM_MP_Solar_'+key);desc=mesh.get_static_mesh_description(0)
            out[key]=dict(vertices=[str(desc.get_vertex_position(u.VertexID(i))) for i in range(8)],api=[n for n in dir(desc) if 'count' in n or 'triangle' in n],mesh_api=[n for n in dir(mesh) if 'triangle' in n or 'vertex' in n])
        return out
    if action=='film_debug':
        a=aa['MC_S25_Film_A_0_0'];m=a.static_mesh_component.static_mesh;desc=m.get_static_mesh_description(0)
        return dict(mesh=m.get_path_name(),bounds=str(m.get_bounding_box()),vertices=[str(desc.get_vertex_position(u.VertexID(i))) for i in range(8)],materials=[str(s.material_slot_name)+':'+s.material_interface.get_name() for s in m.get_editor_property('static_materials')],collision=str(m.get_editor_property('body_setup').get_editor_property('collision_trace_flag')))
    if action=='walk_blocker':
        data=json.loads((SAVED/'MarsStationInteriorAuthoring.json').read_text())['corridors'][2]
        target=u.Vector(*data['points'][0]);pawn=players[0];up=planet.get_radial_up_vector(pawn.get_actor_location());target+=up*88
        hit=u.SystemLibrary.sphere_trace_single(server,pawn.get_actor_location(),target,34,u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,players,u.DrawDebugTrace.NONE,True)
        row=hit.to_tuple() if hit else None
        return dict(blocker=row[9].get_actor_label() if row and row[9] else None,impact=str(row[4]) if row else None)
    if action=='lift2':
        lift=aa['MC_Station_Elevator'];out=[]
        for player in players:
            player.set_gameplay_planet(planet)
            player.set_actor_location_and_rotation(lift.get_actor_transform().transform_location(u.Vector(0,120,98)),lift.get_actor_rotation(),False,False)
            player.initialize_planet_frame();player.get_component_by_class(u.CharacterMovementComponent).stop_movement_immediately()
            out.append(lift.can_execute_interaction(player));lift.interact(player)
        return dict(transit_requested=out)
    if action=='return2':
        lift=aa['MC_Hub_Elevator'];out=[]
        for i,player in enumerate(players):
            player.set_actor_location_and_rotation(lift.get_actor_location()+lift.get_actor_forward_vector()*150+lift.get_actor_up_vector()*98,lift.get_actor_rotation(),False,False)
            player.get_component_by_class(u.CharacterMovementComponent).stop_movement_immediately()
            out.append(lift.can_execute_interaction(player));lift.interact(player)
        return dict(return_requested=out)
    if action=='position':
        return dict(players=[dict(name=p.get_name(),position=str(p.get_actor_location()),indoors=p.get_component_by_class(u.JTSInteriorStateComponent).is_indoors(),zone=str(p.get_component_by_class(u.JTSInteriorStateComponent).zone.zone_id) if p.get_component_by_class(u.JTSInteriorStateComponent).zone else None) for p in players])
    if action=='walk_status':
        return json.loads((SAVED/'MarsStructureWalk.json').read_text()) if (SAVED/'MarsStructureWalk.json').exists() else dict(started=False)
    if action=='walk_reset':
        hub,_=walking_route(aa,planet)
        for i,player in enumerate(players):
            player.set_actor_location_and_rotation(hub.transform_location(u.Vector(-60 if i==0 else 60,0,98)),hub.rotation.rotator(),False,False)
            player.get_component_by_class(u.CharacterMovementComponent).stop_movement_immediately()
        return dict(reset_players=len(players))
    if action=='walk_debug':
        hub,route=walking_route(aa,planet)
        record=json.loads((SAVED/'MarsStructureWalk.json').read_text());out=[]
        for world in worlds:
            pc=u.GameplayStatics.get_player_controller(world,0);pawn=next((a for a in u.GameplayStatics.get_all_actors_of_class(world,u.JTSCharacter) if a.get_controller()==pc),None) if pc else None
            if not isinstance(pawn,u.JTSCharacter) or not pawn.is_locally_controlled():continue
            state=next(s for s in record['players'] if s['authority']==pawn.has_authority())
            if not state['blocked']:continue
            floor,name=route[state['waypoint']];up=planet.get_radial_up_vector(pawn.get_actor_location())
            target=floor+hub.rotation.get_axis_x()*60+up*pawn.capsule_component.get_scaled_capsule_half_height()
            hit=u.SystemLibrary.sphere_trace_single(world,pawn.get_actor_location(),target,34,u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[pawn],u.DrawDebugTrace.NONE,True)
            row=hit.to_tuple() if hit else None
            out.append(dict(index=state['waypoint'],room=name,position=str(pawn.get_actor_location()),target=str(target),blocker=row[9].get_actor_label() if row and row[9] else None,impact=str(row[4]) if row else None))
        return out
    assert action=='walk_start'
    hub,route=walking_route(aa,planet)
    # The two local pawns issue movement in their own worlds, exercising normal client movement replication.
    controlled=[]
    for world in worlds:
        pc=u.GameplayStatics.get_player_controller(world,0);pawn=next((a for a in u.GameplayStatics.get_all_actors_of_class(world,u.JTSCharacter) if a.get_controller()==pc),None) if pc else None
        if not isinstance(pawn,u.JTSCharacter) or not pawn.is_locally_controlled():continue
        pc.set_view_target_with_blend(pawn,0);pc.set_ignore_move_input(False)
        controlled.append(dict(pawn=pawn,world=world,index=0,lane=hub.rotation.get_axis_x()*(60 if len(controlled) else -60),last_distance=1e9,last_progress=u.GameplayStatics.get_real_time_seconds(world),visited=[],blocked=None))
    assert len(controlled)==2,'Expected two local PIE pawns'
    record=dict(running=True,players=[])
    def flush():
        record['players']=[dict(name=s['pawn'].get_name(),authority=s['pawn'].has_authority(),waypoint=s['index'],total=len(route),visited=s['visited'],blocked=s['blocked'],position=str(s['pawn'].get_actor_location())) for s in controlled]
        (SAVED/'MarsStructureWalk.json').write_text(json.dumps(record,indent=2),encoding='utf8')
    def tick(delta):
        complete=True
        for state in controlled:
            pawn,world=state['pawn'],state['world']
            if state['blocked'] or state['index']>=len(route):continue
            complete=False;floor,name=route[state['index']]
            up=(pawn.get_actor_location()-planet.get_planet_center()).normal()
            target=floor+state['lane']+up*pawn.capsule_component.get_scaled_capsule_half_height()
            diff=target-pawn.get_actor_location();flat=diff-up*diff.dot(up);distance=flat.length()
            now=u.GameplayStatics.get_real_time_seconds(world)
            if distance<80:
                state['index']+=1;state['last_distance']=1e9;state['last_progress']=now
                if name not in state['visited'] and state['index']>1:state['visited'].append(name)
            else:
                if distance<state['last_distance']-10:state['last_progress']=now;state['last_distance']=distance
                if now-state['last_progress']>8:state['blocked']=dict(room=name,distance_m=distance/100)
                else:pawn.add_movement_input(flat.normal(),1,True)
        if complete:
            record['running']=False;u.unregister_slate_post_tick_callback(handle)
        flush()
    handle=u.register_slate_post_tick_callback(tick)
    flush()
    return dict(started=True,players=len(controlled),waypoints=len(route))
