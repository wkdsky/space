"""Explicit level authoring and PIE inspection, invoked through the Unreal MCP bridge."""
import json
import math
from pathlib import Path
import unreal

OUT = Path(unreal.Paths.project_saved_dir()) / 'SpaceDebug'


def vec(v): return [v.x, v.y, v.z]


def run(operation):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor) if world else unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    if operation == 'inventory':
        result = []
        for a in actors:
            row = {'name': a.get_name(), 'class': a.get_class().get_name(), 'label': a.get_actor_label(), 'location': vec(a.get_actor_location())}
            if isinstance(a, unreal.JTSPlanetAnchor):
                row.update(id=str(a.get_planet_id()), center=vec(a.get_planet_center()), radius=a.get_visual_radius(), surface=str(a.get_gameplay_surface_actor()))
            for prop in ['planet_anchor', 'planet_id', 'center_actor', 'owning_planet', 'planet', 'parent_planet_id']:
                try: row[prop] = str(a.get_editor_property(prop))
                except Exception: pass
            result.append(row)
        (OUT/'independent_inventory.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf8')
        return json.dumps(result, ensure_ascii=False)
    if operation == 'configure':
        assert not world
        return configure(actors)
    if operation == 'landing_preview':
        return json.dumps(configure_deimos_landing(actors, False))
    if operation == 'landing_find':
        assert world
        planet=next(a for a in actors if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='MarII')
        mars=next(a for a in actors if a.actor_has_tag('Mars'))
        ship=next(a for a in actors if isinstance(a,unreal.JTSSpacecraftActor))
        center=planet.get_planet_center()
        toward=(mars.get_actor_bounds(False)[0]-center).normal()
        def dot(a,b): return a.x*b.x+a.y*b.y+a.z*b.z
        world_up=unreal.Vector(0,0,1)
        perpendicular=(world_up-toward*dot(world_up,toward)).normal()
        side=unreal.MathLibrary.cross_vector_vector(toward,perpendicular).normal()
        candidates=[]
        for degrees in range(0,51,5):
            for azimuth in range(0,360,15):
                a=math.radians(degrees);b=math.radians(azimuth)
                normal=(toward*math.sin(a)+(perpendicular*math.cos(b)+side*math.sin(b))*math.cos(a)).normal()
                forward=(toward-normal*dot(toward,normal)).normal()
                frame=planet.get_surface_frame_at(center+normal*(planet.get_visual_radius()*3),forward)
                if not frame: continue
                elevation=math.degrees(math.asin(max(-1,min(1,dot(frame.up,toward)))))
                if 10<elevation<35 and dot(frame.up,normal)>.9 and normal.z>.4 and frame.up.z>.4:
                    candidates.append((abs(elevation-20),frame,elevation,normal))
        ship.set_actor_tick_enabled(False)
        try:
            for _,frame,elevation,normal in sorted(candidates,key=lambda v:v[0]):
                if ship.snap_spacecraft_to_surface_transform(planet,frame.transform):
                    result={'direction':vec(normal),'surface':vec(frame.location),'forward':vec(frame.forward),'mars_elevation_degrees':elevation,'landed':ship.is_landed()}
                    (OUT/'independent_landing_fit.json').write_text(json.dumps(result,indent=2),encoding='utf8')
                    return json.dumps(result)
            raise AssertionError('No validated support among Mars-visible candidates')
        finally: ship.set_actor_tick_enabled(True)
    if operation == 'audit':
        manager=next(a for a in actors if isinstance(a,unreal.JTSSpaceWorldManager))
        c=manager.get_editor_property('compressed_space')
        rows=[]
        for entry in c.get_editor_property('bodies'):
            a=entry.actor
            planet=isinstance(a,unreal.JTSPlanetAnchor)
            center=a.get_planet_center() if planet else a.get_actor_bounds(False)[0]
            rows.append({'actor':a.get_name(),'planet':str(a.get_planet_id()) if planet else None,
                'world_center':vec(center),'navigation_center_km':vec(entry.navigation_position_kilometers),
                'authored_radius_cm':c.authored_radius(a),'bubble_radius_cm':c.bubble_radius(a),
                'collision':a.get_actor_enable_collision(),'tick':a.is_actor_tick_enabled()})
            if not planet: assert not a.get_actor_enable_collision() and not a.is_actor_tick_enabled()
            else: assert not a.get_editor_property('celestial_companion') and not a.get_editor_property('celestial_background_companion')
        anchors=[a for a in actors if isinstance(a,unreal.JTSPlanetAnchor)]
        assert len(anchors)==3
        for a in anchors:
            for entry in a.get_editor_property('background_navigation'):
                assert not entry.show_on_star_chart and not entry.show_screen_marker
        result={'bodies':rows,'registered_gameplay_count':len(anchors),'near_altitude_cm':c.get_editor_property('near_altitude_centimeters'),
            'marker_navigation_range_km':{str(a.get_planet_id()):a.get_editor_property('navigation_marker_range_kilometers') for a in anchors}}
        (OUT/'independent_audit.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
        return json.dumps(result,ensure_ascii=False)
    if operation == 'probe':
        unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings')).set_editor_property('bThrottleCPUWhenNotForeground',False)
        unreal.SystemLibrary.execute_console_command(world,'t.IdleWhenNotForeground 0')
        unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
        unreal._independent_space_probe = RouteProbe(world, actors)
        return 'production presentation/state route sampling started'
    if operation == 'native':
        unreal._independent_native_probe = NativeFlightProbe(world,actors)
        return 'native owner-RPC Moon/Deimos round trip started (PIE time dilation 8)'
    if operation == 'native_status':
        p=unreal._independent_native_probe
        (OUT/'independent_native_flight.json').write_text(json.dumps(p.result,ensure_ascii=False,indent=2),encoding='utf8')
        return json.dumps({'status':p.result['status'],'stage':p.stage,'samples':len(p.result['samples']),'errors':p.result['errors'][-8:],'last':p.result['samples'][-1:]})
    if operation == 'native_stop':
        unreal._independent_native_probe.finish('stopped')
        return 'stopped'
    if operation == 'status':
        probe=getattr(unreal,'_independent_space_probe',None)
        if probe: (OUT/'independent_routes.json').write_text(json.dumps(probe.result,ensure_ascii=False,indent=2),encoding='utf8')
        return json.dumps({'leg':probe.leg,'step':probe.i,'samples':len(probe.result['samples']),'errors':probe.result['errors'][-10:],'status':probe.result['status']}) if probe else 'no probe'
    if operation == 'stop_probe':
        unreal._independent_space_probe.finish('stopped')
        return 'stopped'
    if operation == 'unthrottle':
        unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings')).set_editor_property('bThrottleCPUWhenNotForeground',False)
        unreal.SystemLibrary.execute_console_command(world,'t.IdleWhenNotForeground 0')
        return 'background throttle disabled during validation'
    if operation == 'cleanup_validation':
        unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings')).set_editor_property('bThrottleCPUWhenNotForeground',True)
        unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 0')
        if world: unreal.GameplayStatics.set_global_time_dilation(world,1)
        return 'temporary validation settings restored'
    raise ValueError(operation)


def configure(actors):
    anchors = {str(a.get_planet_id()): a for a in actors if isinstance(a, unreal.JTSPlanetAnchor)}
    manager = next(a for a in actors if isinstance(a, unreal.JTSSpaceWorldManager))
    earth = anchors['EarthFragments'].get_planet_center()
    # These are compressed world centimetres; navigation coordinates below are kilometres.
    # Separate coordinate bands on all non-coincident axes. The Martian satellites use
    # one local spatial scale and authored orbital phases, with no shared gameplay bubble.
    mars_nav=78340000/math.sqrt(3)
    phobos_offset=(6800,4500,math.sqrt(9378**2-6800**2-4500**2))
    deimos_offset=(8500,21000,math.sqrt(23459**2-8500**2-21000**2))
    # Travel pacing for the ship's 100x anti-gravity cruise: local transfer ~3-6s,
    # interplanetary transfer ~27-48s, excluding near-space manoeuvring.
    offsets = {'EarthFragments': (0,0,0), 'Moon': (2500000,0,0), 'Mars': (12000000,12000000,12000000),
               'MarI': tuple(12000000+v*50 for v in phobos_offset),
               'MarII': tuple(12000000+v*50 for v in deimos_offset)}
    nav = {'EarthFragments': (0,0,0), 'Moon': (384400,0,0), 'Mars': (mars_nav,mars_nav,mars_nav),
           'MarI': tuple(mars_nav+v for v in phobos_offset),
           'MarII': tuple(mars_nav+v for v in deimos_offset)}
    radii = {'EarthFragments':6371,'Moon':1737.4,'Mars':3390,'MarI':11.1,'MarII':6.2}
    bodies = dict(anchors)
    bodies.update({key:next(a for a in actors if a.actor_has_tag(key)) for key in ['Mars','MarI']})
    moved = []
    for key in ['Moon','MarII']:
        anchor = anchors[key]
        old = anchor.get_planet_center()
        target = earth + unreal.Vector(*offsets[key])
        delta = target-old
        surface = anchor.get_gameplay_surface_actor()
        # Bindings are concrete level data; move all authored surface/arrival/nav actors with their planet.
        owned = {anchor, surface}
        center_actor = anchor.get_editor_property('center_actor')
        if center_actor: owned.add(center_actor)
        for a in actors:
            if a in owned or a in bodies.values() or isinstance(a, unreal.JTSSpaceWorldManager): continue
            bound = False
            for prop in ['planet_anchor', 'planet_id']:
                try:
                    value = a.get_editor_property(prop)
                    bound = bound or value == anchor or str(value) == key
                except Exception: pass
            # Authored local nav bounds/spawn helpers may only be associated spatially.
            if bound or (a.get_actor_location()-old).length() < anchor.get_visual_radius()+40000:
                owned.add(a)
        # Move actor hierarchies once, from their top-level attachment.
        for a in owned:
            if a.get_attach_parent_actor() in owned: continue
            before = a.get_actor_location()
            a.set_actor_location(before+delta, False, True)
            moved.append({'actor':a.get_name(),'planet':key,'before':vec(before),'after':vec(a.get_actor_location())})
    for key in ['Mars','MarI']:
        actor = bodies[key]
        _,extent=actor.get_actor_bounds(False)
        desired_radius=170000 if key=='Mars' else 3000
        actor.set_actor_scale3d(actor.get_actor_scale3d()*(desired_radius/max(extent.x,extent.y,extent.z)))
        center, _ = actor.get_actor_bounds(False)
        actor.set_actor_location(actor.get_actor_location()+earth+unreal.Vector(*offsets[key])-center, False, True)
        actor.set_actor_enable_collision(False)
    entries=[]
    for key in offsets:
        entry = unreal.JTSCompressedCelestialBody()
        entry.actor = bodies[key]
        entry.navigation_position_kilometers = unreal.Vector(*nav[key])
        entry.radius_kilometers = radii[key]
        entry.visual_radius_multiplier = 4.0 if key == 'MarI' else 1.0
        entries.append(entry)
    component = manager.get_editor_property('compressed_space')
    component.set_editor_property('bodies', entries)
    # Same physical boundary as the existing 14-km near-flight handoff.
    altitude = 8000*(math.exp(14/12)-1)
    component.set_editor_property('near_altitude_centimeters', altitude)
    for a in anchors.values():
        a.set_editor_property('celestial_companion', None)
        a.set_editor_property('celestial_background_companion', None)
        a.set_editor_property('companion_apparent_separation_centimeters', 0)
    # Calibrate a fixed ship-distance gate at roughly 5% of a 16:9, 90-degree view.
    component.capture_authored_bodies(False)
    # Refuse to save an ambiguous coordinate field or overlapping local transition regions.
    for axis in range(3):
        knots=sorted((offsets[k][axis],nav[k][axis],k) for k in offsets)
        for a,b in zip(knots,knots[1:]):
            if abs(b[0]-a[0])<0.1: assert abs(b[1]-a[1])<0.01, (a,b)
            else:
                assert (b[1]-a[1])/(b[0]-a[0]) >= 14/altitude, (a,b)
                assert b[0]-a[0]>component.bubble_radius(bodies[a[2]])+component.bubble_radius(bodies[b[2]]), (a,b)
    keys=list(bodies)
    for i,key in enumerate(keys):
        a=bodies[key]
        center=a.get_planet_center() if isinstance(a,unreal.JTSPlanetAnchor) else a.get_actor_bounds(False)[0]
        assert (component.navigation_position(center)-unreal.Vector(*nav[key])).length()<0.01, key
        for other_key in keys[i+1:]:
            b=bodies[other_key]
            other_center=b.get_planet_center() if isinstance(b,unreal.JTSPlanetAnchor) else b.get_actor_bounds(False)[0]
            assert (center-other_center).length()>2*(component.bubble_radius(a)+component.bubble_radius(b)), (key,other_key)
    gates={}
    for key,a in anchors.items():
        # One-time reference-camera calibration only. Runtime compares simulated
        # ship distance with this constant, independent of camera/FOV/direction.
        range_km=component.display_radius_kilometers(a)*16/9/0.05
        a.set_editor_property('navigation_marker_range_kilometers',range_km)
        gates[key]=range_km*100000
    landing=configure_deimos_landing(actors, True)
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    result={'world_offsets_cm':offsets,'navigation_positions_km':nav,'near_altitude_cm':altitude,'marker_distance_cm':gates,
        'marker_distance_units':'simulated centimetres from shared ship to body centre',
        'marker_navigation_range_km':{key:value/100000 for key,value in gates.items()},'moved':moved,'deimos_landing':landing}
    (OUT/'independent_configuration.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    return json.dumps(result,ensure_ascii=False)


def configure_deimos_landing(actors, apply):
    """Choose a real-mesh landing frame with Mars above the local horizon."""
    planet=next(a for a in actors if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='MarII')
    mars=next(a for a in actors if a.actor_has_tag('Mars'))
    center=planet.get_planet_center()
    toward=(mars.get_actor_bounds(False)[0]-center).normal()
    def dot(a,b): return a.x*b.x+a.y*b.y+a.z*b.z
    # Selected through the existing ship support fitter; no landing tolerances are changed.
    normal=unreal.Vector(0.08071219681487608,-0.47994286911701445,0.873579065493814)
    forward=(toward-normal*dot(toward,normal)).normal()
    frame=planet.get_surface_frame_at(center+normal*(planet.get_visual_radius()*3),forward)
    assert frame, 'Authored Deimos landing direction must resolve the real mesh'
    elevation=math.degrees(math.asin(max(-1,min(1,dot(frame.up,toward)))))
    arrival=next(a for a in actors if isinstance(a,unreal.JTSPlanetArrivalAnchor) and a.get_planet_anchor()==planet)
    landing=next(a for a in actors if isinstance(a,unreal.JTSPlanetSurfaceAnchor) and a.get_planet_anchor()==planet)
    clearance=150
    result={'surface':vec(frame.location),'up':vec(frame.up),'forward':vec(frame.forward),'mars_elevation_degrees':elevation,
        'arrival_clearance_cm':clearance,'player_relative':str(arrival.get_editor_property('player_arrival_point').get_relative_transform()),
        'ship_relative':str(arrival.get_editor_property('spacecraft_arrival_point').get_relative_transform())}
    if apply:
        landing.set_actor_transform(frame.transform,False,True)
        assert landing.snap_to_planet_surface()
        transform=frame.transform
        transform.translation=frame.location-frame.forward*900+frame.up*clearance
        arrival.set_actor_transform(transform,False,True)
        # The parking manager probes radially. Keep its ship query directly above
        # the validated support point even when the triangle normal is tilted.
        delta=(normal-frame.up)*600
        arrival.get_editor_property('spacecraft_arrival_point').set_relative_location(
            unreal.Vector(900+dot(delta,frame.forward),dot(delta,frame.right),450+dot(delta,frame.up)),False,True)
        result['ship_relative']=str(arrival.get_editor_property('spacecraft_arrival_point').get_relative_transform())
        assert 10<elevation<35, result
    return result


class RouteProbe:
    """PIE trajectory sampler, with production movement/state/presentation ticks between samples.

    This is a deterministic spatial sweep, not a claim of native piloting. Native input is
    exercised separately after the sweep. Never saves temporary observer poses to the map.
    """
    def __init__(self, world, actors):
        assert world
        self.world=world
        self.ship=next(a for a in actors if isinstance(a,unreal.JTSSpacecraftActor))
        self.manager=next(a for a in actors if isinstance(a,unreal.JTSSpaceWorldManager))
        self.c=self.manager.get_editor_property('compressed_space')
        self.anchors={str(a.get_planet_id()):a for a in actors if isinstance(a,unreal.JTSPlanetAnchor)}
        self.movement=self.ship.get_flight_movement_component()
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        pawn=pc.get_controlled_pawn()
        if pawn != self.ship:
            pawn.set_actor_location(self.ship.get_boarding_interaction_center(),False,True)
            assert self.ship.try_board_player(pawn)
        if self.ship.is_landed(): assert self.ship.begin_surface_takeoff()
        self.ship.set_actor_tick_enabled(False)
        self.result={'status':'running','method':'Deterministic PIE spatial sweep with production state/presentation ticks','legs':[],'samples':[],'errors':[]}
        self.legs=[('EarthFragments','Moon'),('Moon','EarthFragments'),('Moon','MarII'),('MarII','Moon')]
        self.leg=-1
        self.i=-1
        self.previous=None
        self.pending=None
        self.pose_time=-1
        self.handle=unreal.register_slate_post_tick_callback(self.tick)

    def tick(self, delta):
        try:
            if self.pending:
                # Slate can tick without a game-world tick while the editor is in the background.
                if unreal.GameplayStatics.get_time_seconds(self.world) <= self.pose_time: return
                key,p,d=self.pending
                target=self.anchors[key]
                bodies=unreal.JTSCelestialNavigationLibrary.gather_bodies(self.manager,self.ship)
                b=next(b for b in bodies if b.planet == target)
                dist=(self.ship.get_actor_location()-target.get_planet_center()).length()
                row={'leg':self.leg,'step':self.i,'target':key,'world_distance_cm':dist,'range_cm':b.range_centimeters,
                     'angular_radius':b.visual_radius/dist,'visible':b.visible,'cruise':self.manager.is_cruise_presentation_active(self.ship),
                     'current':str(self.manager.get_current_planet().get_planet_id()) if self.manager.get_current_planet() else None,
                     'reference':str(self.c.containing_planet(self.ship.get_actor_location())),
                     'background_collision':[a.actor.get_actor_enable_collision() for a in self.c.get_editor_property('bodies') if not isinstance(a.actor,unreal.JTSPlanetAnchor)]}
                if self.previous:
                    if row['range_cm'] > self.previous['range_cm']+100: self.result['errors'].append(['range_increased',self.leg,self.i])
                    if row['angular_radius']+1e-7 < self.previous['angular_radius']: self.result['errors'].append(['target_shrank',self.leg,self.i])
                if not row['visible']: self.result['errors'].append(['target_hidden',self.leg,self.i])
                if any(row['background_collision']): self.result['errors'].append(['background_collision',self.leg,self.i])
                self.result['samples'].append(row)
                self.previous=row
                self.pending=None
            self.i+=1
            if self.i>240 or self.leg<0:
                self.leg+=1
                self.i=0
                self.previous=None
                if self.leg>=len(self.legs):
                    self.finish('complete')
                    return
                source,key=self.legs[self.leg]
                start=self.anchors[source].get_planet_center()
                end=self.anchors[key].get_planet_center()
                direction=(end-start).normal()
                self.start=start+direction*(self.c.bubble_radius(self.anchors[source])+100)
                self.end=end-direction*(self.c.bubble_radius(self.anchors[key])-100)
                self.target=key
                self.result['legs'].append({'source':source,'target':key,'route_km':self.c.route_kilometers(self.anchors[source],self.anchors[key]),'physical_length_cm':(self.end-self.start).length()})
            p=self.start+(self.end-self.start)*(self.i/240)
            self.movement.stop_movement_immediately()
            self.movement.activate(True)
            self.ship.set_actor_location(p,False,True)
            self.pending=(self.target,p,0)
            self.pose_time=unreal.GameplayStatics.get_time_seconds(self.world)
        except Exception as ex:
            self.result['errors'].append(str(ex))
            self.finish('failed')

    def finish(self,status):
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.movement.stop_movement_immediately()
        self.ship.set_actor_tick_enabled(True)
        self.result['status']=status
        (OUT/'independent_routes.json').write_text(json.dumps(self.result,ensure_ascii=False,indent=2),encoding='utf8')


class NativeFlightProbe:
    def __init__(self,world,actors,time_dilation=8.0,output_name='independent_native_flight.json',targets=('Moon','MarII')):
        assert world
        self.world=world
        self.ship=next(a for a in actors if isinstance(a,unreal.JTSSpacecraftActor))
        self.manager=next(a for a in actors if isinstance(a,unreal.JTSSpaceWorldManager))
        self.c=self.manager.get_editor_property('compressed_space')
        self.anchors={str(a.get_planet_id()):a for a in actors if isinstance(a,unreal.JTSPlanetAnchor)}
        self.movement=self.ship.get_flight_movement_component()
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        pc.call_method('ServerRequestDebugPlanet',('MarII',False))
        pawn=pc.get_controlled_pawn()
        if pawn != self.ship:
            pawn.set_actor_location(self.ship.get_boarding_interaction_center(),False,True)
            assert self.ship.try_board_player(pawn)
        assert self.ship.begin_surface_takeoff()
        self.ship.set_actor_tick_enabled(False)
        unreal.GameplayStatics.set_global_time_dilation(world,time_dilation)
        self.output_name=output_name
        self.targets=list(targets)
        self.stage='Lift'; self.started=unreal.GameplayStatics.get_time_seconds(world)
        self.last_time=-1; self.previous=None; self.last_sample=-1
        self.result={'status':'running','method':'Native movement, owner ServerSetFlightInput RPC, production stats','time_dilation':time_dilation,'legs':[],'samples':[],'errors':[]}
        self.handle=unreal.register_slate_post_tick_callback(self.tick)

    def intent(self,forward=0,lift=0):
        state=unreal.JTSSpacecraftInputState()
        assert state.import_text('(MoveForward=%f,Lift=%f,bFlightAssistEnabled=True,bBoosting=True,SpeedLimit=1.0)'%(forward,lift))
        self.ship.call_method('ServerSetFlightInput',(state,))

    def leg(self,key):
        self.stage=key
        target=self.anchors[key].get_planet_center()
        forward=(target-self.ship.get_actor_location()).normal()
        old_up=self.ship.get_actor_up_vector()
        up=(old_up-forward*unreal.MathLibrary.dot_vector_vector(old_up,forward)).normal()
        right=unreal.MathLibrary.cross_vector_vector(up,forward).normal()
        self.ship.set_actor_rotation(unreal.MathLibrary.make_rotation_from_axes(forward,right,up),False)
        self.started=unreal.GameplayStatics.get_time_seconds(self.world)
        self.movement.stop_movement_immediately()
        self.previous=None

    def tick(self,delta):
        try:
            now=unreal.GameplayStatics.get_time_seconds(self.world)
            if now<=self.last_time: return
            self.last_time=now
            elapsed=now-self.started
            if self.stage=='Lift':
                self.intent(lift=1)
                # The Moon is below the default Deimos landing site's horizon. Leave
                # enough clearance for the straight test leg to miss the departure bubble.
                source=self.anchors['MarII']
                if (self.ship.get_actor_location()-source.get_planet_center()).length()>4*self.c.bubble_radius(source):
                    self.leg(self.targets.pop(0));self.intent(forward=1)
                elif elapsed>120: self.finish('lift_timeout')
                return
            target=self.anchors[self.stage]
            p=self.ship.get_actor_location()
            b=next(b for b in unreal.JTSCelestialNavigationLibrary.gather_bodies(self.manager,self.ship) if b.planet==target)
            velocity=self.movement.get_editor_property('velocity')
            nv=self.c.navigation_velocity(p,velocity)
            row={'leg':self.stage,'seconds':elapsed,'position':vec(p),'navigation_position_km':vec(self.c.navigation_position(p)),
                 'range_km':b.range_centimeters/100000,'angle':b.visual_radius/(p-target.get_planet_center()).length(),
                 'navigation_speed_km_s':nv.length()/100000,'physical_speed_cm_s':velocity.length(),
                 'visible':b.visible,'cruise':self.manager.is_cruise_presentation_active(self.ship),
                 'current':str(self.manager.get_current_planet().get_planet_id()) if self.manager.get_current_planet() else None}
            if self.previous:
                if row['range_km']>self.previous['range_km']+0.1: self.result['errors'].append(['range_increased',self.stage,elapsed])
                if row['angle']+1e-8<self.previous['angle']: self.result['errors'].append(['target_shrank',self.stage,elapsed])
            if not b.visible: self.result['errors'].append(['target_hidden',self.stage,elapsed])
            self.previous=row
            if now-self.last_sample>0.15:
                self.result['samples'].append(row); self.last_sample=now
            if (p-target.get_planet_center()).length()<self.c.bubble_radius(target)-200:
                self.result['legs'].append({'target':self.stage,'game_seconds':elapsed,'near':not row['cruise'],'current':row['current']})
                if self.targets: self.leg(self.targets.pop(0))
                else: self.finish('complete');return
            elif elapsed>1000:
                self.finish('flight_timeout');return
            self.intent(forward=1)
        except Exception as ex:
            self.result['errors'].append(str(ex));self.finish('failed')

    def finish(self,status):
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.intent()
        self.movement.stop_movement_immediately()
        self.ship.set_actor_tick_enabled(True)
        unreal.GameplayStatics.set_global_time_dilation(self.world,1.0)
        self.result['status']=status
        (OUT/self.output_name).write_text(json.dumps(self.result,ensure_ascii=False,indent=2),encoding='utf8')
