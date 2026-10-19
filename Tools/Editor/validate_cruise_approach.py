"""MCP approach tuning and PIE checks; production ship Tick/camera stay active."""
import json
import math
from pathlib import Path
import unreal

# Concrete scene tuning belongs to level authoring, never native flight code.
APPROACH_TUNING = {
    'Moon': {'range_km': 200000, 'exponent': .1, 'expansion_deg_s': 0},
    'MarII': {'range_km': 2400, 'exponent': .1, 'expansion_deg_s': 0},
    'EarthFragments': {'range_km': 620000, 'exponent': .1, 'expansion_deg_s': 0},
}


def xyz(v): return [v.x,v.y,v.z]


def heading(radial):
    forward=radial*-1
    right=unreal.MathLibrary.cross_vector_vector(unreal.Vector(0,0,1),forward).normal()
    if right.length()<.1:right=unreal.Vector(0,1,0)
    up=unreal.MathLibrary.cross_vector_vector(forward,right).normal()
    return unreal.MathLibrary.make_rotation_from_axes(forward,right,up)


def run(operation,world,actors):
    if operation=='authoring_inspect':
        rows=[]
        for a in actors:
            if not isinstance(a,unreal.JTSPlanetAnchor):continue
            visual=a.get_gameplay_surface_actor()
            envelope=a.get_editor_property('fragment_envelope')
            data=envelope.get_editor_property('data')
            rows.append({'id':str(a.get_planet_id()),'center':xyz(a.get_planet_center()),
                'radius':a.get_approximate_radius(),'visual_radius':a.get_visual_radius(),
                'visual_bounds':[xyz(v) for v in visual.get_actor_bounds(False)],
                'visual_scale':xyz(visual.get_actor_scale3d()),
                'fragment_rotation':str(envelope.get_editor_property('geometry_rotation')),
                'fragment_radii':xyz(data.get_editor_property('region_radii')) if data else None,
                'fragment_vertices':sum(len(h.vertices) for h in data.fragments) if data else 0})
        return json.dumps(rows)
    if operation=='configure':
        assert not world, 'Stop PIE before saving flight defaults'
        editor_world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        assert editor_world.get_path_name().startswith('/Game/Space/Maps/L_SpaceWorld.'),editor_world.get_path_name()
        level_file=Path(unreal.Paths.project_content_dir())/'Space/Maps/L_SpaceWorld.umap'
        backup=Path(unreal.Paths.project_saved_dir())/'SpaceDebug/L_SpaceWorld_before_approach_tuning.umap'
        if not backup.exists():backup.write_bytes(level_file.read_bytes())
        configured={}
        for planet_id,tuning in APPROACH_TUNING.items():
            anchor=next(a for a in actors if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())==planet_id)
            anchor.modify()
            anchor.set_editor_property('navigation_marker_range_kilometers',tuning['range_km'])
            anchor.set_editor_property('cruise_approach_exponent_override',tuning['exponent'])
            anchor.set_editor_property('cruise_approach_expansion_degrees_per_second',tuning['expansion_deg_s'])
            configured[planet_id]=dict(tuning)
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        bp=unreal.load_asset('/Game/Space/Ships/TwinforkR1/BP_Twinfork_R1')
        cls=unreal.EditorAssetLibrary.load_blueprint_class(bp.get_path_name())
        movement=unreal.get_default_object(cls).get_flight_movement_component()
        stats=movement.get_base_stats()
        stats.cruise_approach_near_speed_multiplier=3
        stats.cruise_approach_exponent=4
        movement.set_editor_property('base_stats',stats)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
        return json.dumps({'blueprint':bp.get_path_name(),'near_display_speed_multiplier':3,'fallback_exponent':4,'planets':configured})
    assert world, 'PIE required'
    ship=next(a for a in actors if isinstance(a,unreal.JTSSpacecraftActor))
    manager=next(a for a in actors if isinstance(a,unreal.JTSSpaceWorldManager))
    c=manager.get_editor_property('compressed_space')
    movement=ship.get_flight_movement_component()
    pc=unreal.GameplayStatics.get_player_controller(world,0)
    if operation.startswith('timescale_'):
        scale=float(operation[len('timescale_'):])
        unreal.GameplayStatics.set_global_time_dilation(world,scale)
        return 'PIE-only game clock sampling scale: '+str(scale)
    if operation.startswith('setup_'):
        # Only the initial probe pose is authored here. Native movement drives every subsequent frame.
        key=operation[len('setup_'):]
        departure=key.endswith('_depart')
        if departure:key=key[:-len('_depart')]
        preview=key.endswith('_preview')
        if preview:key=key[:-len('_preview')]
        source=None
        if '_from_' in key:key,source=key.split('_from_')
        axis=None
        if '_axis_' in key:key,axis=key.split('_axis_')
        target=next(a for a in actors if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())==key)
        if preview:
            target.set_editor_property('navigation_marker_range_kilometers',APPROACH_TUNING[key]['range_km'])
        pawn=pc.get_controlled_pawn()
        if pawn!=ship:
            pawn.set_actor_location(ship.get_boarding_interaction_center(),False,True)
            assert ship.try_board_player(pawn)
        if ship.is_landed(): assert ship.begin_surface_takeoff()
        center=target.get_planet_center()
        radial=unreal.Vector(0,0,1)
        if axis:
            radial={'Xneg':unreal.Vector(-1,0,0),'Yneg':unreal.Vector(0,-1,0),'Zneg':unreal.Vector(0,0,-1)}[axis]
        if source:
            source_anchor=next(a for a in actors if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())==source)
            radial=(source_anchor.get_planet_center()-center).normal()
        inner=c.bubble_radius(target)
        gate=target.get_editor_property('navigation_marker_range_kilometers')
        target_nav=c.navigation_position(center)
        def radius_range(radius):return (c.navigation_position(center+radial*radius)-target_nav).length()
        low,high=inner,inner*2
        while radius_range(high)<gate:high*=2
        for _ in range(60):
            mid=(low+high)*.5
            if radius_range(mid)<gate:low=mid
            else:high=mid
        outer=(low+high)*.5
        unreal._cruise_approach_context={'target':target,'center':center,'radial':radial,'inner':inner,'outer':outer,'gate':gate}
        start=center+radial*(outer+1000)
        if departure:
            assert source,'A departure probe needs its source planet'
            start=source_anchor.get_planet_center()-radial*(c.bubble_radius(source_anchor)+1000)
        ship.set_actor_location(start,False,True)
        ship.set_actor_rotation(heading(radial),False)
        assert ship.is_actor_tick_enabled()
        return json.dumps({'planet':key,'inner_cm':inner,'outer_cm':outer,'gate_km':gate,'departure':departure,
                           'start':xyz(ship.get_actor_location()),'forward':xyz(ship.get_actor_forward_vector())})
    context=unreal._cruise_approach_context
    target=context['target'];center=context['center'];radial=context['radial']
    stats=movement.get_effective_stats()
    if operation=='curve_sweep':
        saved=target.get_editor_property('cruise_approach_exponent_override')
        target.set_editor_property('cruise_approach_exponent_override',0)
        rows=[]
        command=radial*-stats.max_move_speed*stats.boost_multiplier
        count=1200
        width=context['outer']-context['inner']
        for exponent in [2,3,4,6,8,10,12]:
            seconds=0
            milestones=[]
            for i in range(count):
                progress=(i+.5)/count
                radius=context['outer']-width*progress
                position=center+radial*radius
                multiplier=c.cruise_approach_multiplier(position,command,stats.cruise_speed_multiplier,3,exponent)
                seconds+=width/count/(command.length()*multiplier)
                angle=math.degrees(2*math.asin(min(.999999,c.authored_radius(target)*c.visual_scale(position,target)/radius)))
                if len(milestones)<4 and angle>=[5,10,30,50][len(milestones)]:
                    milestones.append({'angle_degrees':angle,'seconds':seconds,'nav_km_s':c.navigation_velocity(position,command*multiplier).length()/100000})
            rows.append({'exponent':exponent,'estimated_seconds':seconds,'milestones':milestones})
        target.set_editor_property('cruise_approach_exponent_override',saved)
        return json.dumps({'planet':str(target.get_planet_id()),'rows':rows})
    if operation=='directions':
        rows=[]
        directions={'X+':unreal.Vector(1,0,0),'X-':unreal.Vector(-1,0,0),'Y+':unreal.Vector(0,1,0),
                    'Y-':unreal.Vector(0,-1,0),'Z+':unreal.Vector(0,0,1),'Z-':unreal.Vector(0,0,-1)}
        for a in actors:
            if isinstance(a,unreal.JTSPlanetAnchor) and a!=target:
                directions['from_'+str(a.get_planet_id())]=(a.get_planet_center()-center).normal()
        gate=context['gate'];inner=context['inner'];target_nav=c.navigation_position(center)
        for name,direction in directions.items():
            low,high=inner,inner*2
            def nav_range(r):return (c.navigation_position(center+direction*r)-target_nav).length()
            while nav_range(high)<gate:high*=2
            for _ in range(50):
                mid=(low+high)*.5
                if nav_range(mid)<gate:low=mid
                else:high=mid
            outer=(low+high)*.5;seconds=0;milestones=[];count=1200;other_bubbles=set()
            command=direction*-stats.max_move_speed*stats.boost_multiplier
            for i in range(count):
                radius=outer-(outer-inner)*(i+.5)/count
                position=center+direction*radius
                containing=c.containing_planet(position)
                if containing and containing!=target:other_bubbles.add(str(containing.get_planet_id()))
                multiplier=c.cruise_approach_multiplier(position,command,stats.cruise_speed_multiplier,3,stats.cruise_approach_exponent)
                seconds+=(outer-inner)/count/(command.length()*multiplier)
                angle=math.degrees(2*math.asin(min(.999999,c.authored_radius(target)*c.visual_scale(position,target)/radius)))
                if len(milestones)<4 and angle>=[5,10,30,50][len(milestones)]:
                    milestones.append({'angle_degrees':angle,'seconds':seconds})
            rows.append({'direction':name,'estimated_boost_seconds':seconds,'outer_cm':outer,
                         'crosses_other_bubbles':sorted(other_bubbles),'milestones':milestones})
        return json.dumps({'planet':str(target.get_planet_id()),'rows':rows})
    if operation=='fly':
        state=unreal.JTSSpacecraftInputState()
        assert state.import_text('(MoveForward=1.0,bFlightAssistEnabled=True,bBoosting=True,SpeedLimit=1.0)')
        stop=unreal.JTSSpacecraftInputState()
        assert stop.import_text('(bFlightAssistEnabled=True,SpeedLimit=1.0)')
        probe={'status':'running','samples':[],'started':unreal.GameplayStatics.get_time_seconds(world)}
        unreal._cruise_approach_flight=probe
        ship.call_method('ServerSetFlightInput',(state,))
        last=[probe['started']]
        def tick(delta):
            if not unreal.SystemLibrary.is_valid(ship):
                unreal.unregister_slate_post_tick_callback(unreal._cruise_approach_handle)
                probe['status']='world_closed';return
            now=unreal.GameplayStatics.get_time_seconds(world)
            if now>last[0]:
                last[0]=now
                row=json.loads(run('inspect',world,actors))
                probe['samples'].append(row)
                if not row['cruise'] or now-probe['started']>90:
                    ship.call_method('ServerSetFlightInput',(stop,))
                    probe['status']='complete' if not row['cruise'] else 'timeout'
                    unreal.unregister_slate_post_tick_callback(unreal._cruise_approach_handle)
                    return
            ship.call_method('ServerSetFlightInput',(state,))
        unreal._cruise_approach_handle=unreal.register_slate_post_tick_callback(tick)
        return 'Native authoritative flight started; per-game-frame samples, Actor Tick and camera active'
    if operation in ['status','result']:
        probe=unreal._cruise_approach_flight
        return json.dumps(probe if operation=='result' else {'status':probe['status'],'samples':len(probe['samples']),'last':probe['samples'][-1:]})
    if operation=='aim':
        assert manager.is_cruise_presentation_active(ship)
        ship.set_actor_rotation(heading(radial),False)
        return 'Initial native-flight heading; no subsequent position or rotation writes'
    if operation=='calibrate':
        command=radial*-stats.max_move_speed
        near=c.navigation_velocity(center,command).length()
        rows=[]
        for progress in [-.01,0,.1,.25,.5,.75,.9,.99,.99999,1.00001]:
            radius=context['outer']-(context['outer']-context['inner'])*progress
            position=center+radial*radius
            multiplier=c.cruise_approach_multiplier(position,command,stats.cruise_speed_multiplier,
                stats.cruise_approach_near_speed_multiplier,stats.cruise_approach_exponent)
            rows.append({'progress':progress,'radius_cm':radius,'range_km':(c.navigation_position(position)-c.navigation_position(center)).length(),
                         'other_near_bubble':str(c.containing_planet(position).get_planet_id()) if c.containing_planet(position) and c.containing_planet(position)!=target else None,
                         'world_multiplier':multiplier,'nav_speed_cm_s':c.navigation_velocity(position,command*multiplier).length(),
                         'near_nav_speed_cm_s':near})
        return json.dumps({'planet':str(target.get_planet_id()),'rows':rows})
    if operation=='inspect':
        position=ship.get_actor_location()
        velocity=ship.get_flight_velocity()
        camera=ship.get_flight_camera();boom=ship.get_flight_camera_boom()
        pivot=camera.get_world_location()+camera.get_forward_vector()*boom.target_arm_length-unreal.MathLibrary.quat_rotate_vector(camera.get_world_rotation().quaternion(),boom.socket_offset)
        def angle_at(p):
            return math.degrees(2*math.asin(min(.999999,c.authored_radius(target)*c.visual_scale(p,target)/(p-center).length())))
        direction=velocity.normal() if velocity.length()>0 else radial*-1
        growth=max(0,(angle_at(position+direction)-angle_at(position-direction))*.5)
        metric=c.navigation_velocity(position,direction).length()
        floor_world_speed=c.navigation_velocity(center,radial*stats.max_move_speed).length()*stats.boost_multiplier*3/max(metric,.000001)
        return json.dumps({'seconds':unreal.GameplayStatics.get_time_seconds(world),'planet':str(target.get_planet_id()),
            'position':xyz(position),'velocity_cm_s':xyz(velocity),'world_speed_cm_s':velocity.length(),
            'nav_speed_cm_s':c.navigation_velocity(position,velocity).length(),
            'near_nav_speed_cm_s':c.navigation_velocity(center,radial*stats.max_move_speed).length(),
            'boost':stats.boost_multiplier,'multiplier':movement.get_flight_speed_multiplier(),
            'configured_boundary_multiplier':stats.cruise_approach_near_speed_multiplier,'configured_exponent':stats.cruise_approach_exponent,
            'planet_exponent':target.get_editor_property('cruise_approach_exponent_override'),
            'expansion_limit_deg_s':target.get_editor_property('cruise_approach_expansion_degrees_per_second'),
            'angular_diameter_degrees':angle_at(position),
            'geometric_growth_rate_deg_s':growth*velocity.length(),'floor_growth_rate_deg_s':growth*floor_world_speed,
            'radius_cm':(position-center).length(),'range_km':(c.navigation_position(position)-c.navigation_position(center)).length(),
            'gate_km':context['gate'],'cruise':manager.is_cruise_presentation_active(ship),'ship_tick_enabled':ship.is_actor_tick_enabled(),
            'position_lag':boom.get_editor_property('enable_camera_lag'),'camera_pivot_error_cm':(pivot-boom.get_world_location()-boom.target_offset).length(),
            'current_planet':str(manager.get_current_planet().get_planet_id()) if manager.get_current_planet() else None})
    raise ValueError(operation)
