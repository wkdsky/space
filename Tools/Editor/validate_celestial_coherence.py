"""Inspect and validate shared celestial presentation through the project MCP bridge."""
import json
import math
from pathlib import Path
import unreal

OUT = Path(unreal.Paths.project_saved_dir()) / 'SpaceDebug'


def vec(value): return [value.x, value.y, value.z]


def key(actor):
    return str(actor.get_planet_id()) if isinstance(actor, unreal.JTSPlanetAnchor) else 'Mars'


def members(space):
    result = {}
    for entry in space.get_editor_property('bodies'):
        body = entry.actor
        visual = body.get_gameplay_surface_actor() if isinstance(body, unreal.JTSPlanetAnchor) else body
        result[key(body)] = dict(actor=body, visual=visual, center=space.authored_center(body), entry=entry)
    return result


def context():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor) if world else unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    manager = next(a for a in actors if isinstance(a, unreal.JTSSpaceWorldManager))
    space = manager.get_editor_property('compressed_space')
    if not world: space.capture_authored_bodies(False)
    return world, actors, manager, space


def run(operation):
    world, actors, manager, space = context()
    if operation == 'configure':
        assert not world, 'Stop PIE before authoring'
        assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().startswith('/Game/Space/Maps/L_SpaceWorld.')
        entries = space.get_editor_property('bodies')
        space.modify()
        for index, entry in enumerate(entries):
            body_key = str(entry.actor.get_planet_id()) if isinstance(entry.actor, unreal.JTSPlanetAnchor) else next(k for k in ['Mars'] if entry.actor.actor_has_tag(k))
            entry.presentation_group = {'EarthFragments':'EarthMoon', 'Moon':'EarthMoon',
                'Mars':'MarsSystem', 'MarI':'MarsSystem', 'MarII':'MarsSystem'}[body_key]
            entries[index] = entry
        space.set_editor_property('bodies', entries)
        space.capture_authored_bodies(False)
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        return run('inspect')
    if operation == 'inspect':
        ship = next((a for a in actors if isinstance(a, unreal.JTSSpacecraftActor)), None)
        position = ship.get_actor_location() if ship else unreal.Vector()
        result = dict(pie=bool(world), ship_position=vec(position),
            ship_forward=vec(ship.get_actor_forward_vector()) if ship else None, bodies=[])
        for entry in space.get_editor_property('bodies'):
            body = entry.actor
            visual = body.get_gameplay_surface_actor() if isinstance(body, unreal.JTSPlanetAnchor) else body
            center, extent = visual.get_actor_bounds(False)
            result['bodies'].append(dict(actor=body.get_actor_label(),
                center=vec(center), authored_radius=space.authored_radius(body),
                key=key(body), authored_location=vec(visual.get_actor_location()), extent=vec(extent),
                visual_radius=max(extent.x, extent.y, extent.z), scale=vec(visual.get_actor_scale3d()),
                visual_ratio=space.visual_scale(position, body),
                group=str(entry.presentation_group),
                nav=vec(entry.navigation_position_kilometers), radius_km=entry.radius_kilometers,
                multiplier=entry.visual_radius_multiplier, component_visible=visual.static_mesh_component.is_visible()))
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT/'celestial_coherence_inspect.json').write_text(json.dumps(result, indent=2), encoding='utf8')
        return json.dumps(result)
    if operation == 'matrix':
        data = members(space)
        routes = local_routes(space, data)
        result = dict(status='complete', routes=[], errors=[])
        for name, start, end, group in routes:
            minimum, maximum = 1., 0.
            for i in range(301):
                p = start + (end-start)*(i/300)
                for body in data.values():
                    if str(body['entry'].presentation_group) != group: continue
                    scale = space.visual_scale(p, body['actor'])
                    minimum, maximum = min(minimum,scale), max(maximum,scale)
                    assert abs(scale-1) < 1e-10, (name,i,key(body['actor']),scale)
                    assert (space.presentation_center(p, body['actor'])-body['center']).length()<.001
            result['routes'].append(dict(name=name, samples=301, scale_min=minimum, scale_max=maximum))
        (OUT/'celestial_coherence_matrix.json').write_text(json.dumps(result, indent=2), encoding='utf8')
        return json.dumps(result)
    if operation == 'probe':
        settings = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
        unreal._celestial_coherence_throttle = settings.get_editor_property('bThrottleCPUWhenNotForeground')
        settings.set_editor_property('bThrottleCPUWhenNotForeground',False)
        unreal._celestial_coherence_probe = CoherenceProbe(world, actors, manager, space)
        return 'Production PIE transform sweep started; temporary observer poses only'
    if operation == 'status':
        result = unreal._celestial_coherence_probe.result
        return json.dumps({**result, 'samples':len(result['samples']), 'last':result['samples'][-1:]})
    if operation == 'forward_probe':
        run('view_a')
        unreal._celestial_forward_probe = ForwardProbe(world, actors, space)
        return 'Forward-only server input probe started'
    if operation == 'forward_status':
        result = unreal._celestial_forward_probe.result
        return json.dumps({**result, 'samples':len(result['samples']), 'last':result['samples'][-1:]})
    if operation == 'restore':
        if hasattr(unreal,'_celestial_coherence_throttle'):
            unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings')).set_editor_property('bThrottleCPUWhenNotForeground',unreal._celestial_coherence_throttle)
            del unreal._celestial_coherence_throttle
        return 'Verification performance setting restored'
    if operation in ['view_a','view_b']:
        assert world
        ship = board(world, actors)
        data = members(space)
        center = data['MarII']['center']
        start = center + unreal.Vector(0,0,300000)
        forward = (data['MarI']['center']-start).normal()
        p = start + forward*(100000 if operation=='view_b' else 0)
        ship.get_flight_movement_component().stop_movement_immediately()
        ship.set_actor_location(p,False,True)
        ship.set_actor_rotation(unreal.MathLibrary.make_rot_from_xz(forward,unreal.Vector(0,0,1)),True)
        return json.dumps(dict(position=vec(p),forward=vec(forward),mars_distance=(p-data['Mars']['center']).length(),mars_scale=space.visual_scale(p,data['Mars']['actor'])))
    raise ValueError(operation)


def board(world, actors):
    ship = next(a for a in actors if isinstance(a,unreal.JTSSpacecraftActor))
    pc = unreal.GameplayStatics.get_player_controller(world,0)
    if pc.get_controlled_pawn()!=ship:
        pawn=pc.get_controlled_pawn()
        pawn.set_actor_location(ship.get_boarding_interaction_center(),False,True)
        assert ship.try_board_player(pawn)
    if ship.is_landed(): assert ship.begin_surface_takeoff()
    ship.call_method('ServerSetFlightInput',(unreal.JTSSpacecraftInputState(),))
    return ship


def local_routes(space,data):
    routes=[]
    for keys,group in [(['Mars','MarI','MarII'],'MarsSystem'),(['EarthFragments','Moon'],'EarthMoon')]:
        for source in keys:
            for target in keys:
                if source==target: continue
                start=data[source]['center']+unreal.Vector(0,0,space.bubble_radius(data[source]['actor'])+30000)
                end=data[target]['center']+unreal.Vector(0,0,space.bubble_radius(data[target]['actor'])+30000)
                routes.append((source+' -> '+target,start,end,group))
    return routes


class CoherenceProbe:
    def __init__(self,world,actors,manager,space):
        assert world
        self.world,self.manager,self.space=world,manager,space
        self.ship=board(world,actors)
        self.movement=self.ship.get_flight_movement_component()
        self.data=members(space)
        authored=json.loads((OUT/'celestial_coherence_inspect.json').read_text(encoding='utf8'))
        self.authored={v['key']:v for v in authored['bodies']}
        self.routes=local_routes(space,self.data)
        for name in ['Mars','EarthFragments']:
            group=str(self.data[name]['entry'].presentation_group)
            center=self.data[name]['center']
            start=center+unreal.Vector(0,0,500000)
            end=center+unreal.Vector(0,0,16000000)
            self.routes += [(name+' system departure',start,end,group),(name+' system return',end,start,group)]
        self.result=dict(status='running',method='PIE spatial sweep through production movement/presentation ticks',routes=[],samples=[],errors=[])
        self.leg=-1
        self.step=121
        self.pending=False
        self.pose_time=-1
        self.ship.set_actor_tick_enabled(False)
        self.handle=unreal.register_slate_post_tick_callback(self.tick)

    def tick(self,_):
        try:
            now=unreal.GameplayStatics.get_time_seconds(self.world)
            if self.pending:
                if now<=self.pose_time: return
                self.sample()
                self.pending=False
            self.step+=1
            if self.step>120:
                self.leg+=1
                self.step=0
                if self.leg>=len(self.routes):
                    self.finish('complete')
                    return
                self.result['routes'].append(self.routes[self.leg][0])
            name,start,end,group=self.routes[self.leg]
            self.movement.stop_movement_immediately()
            self.ship.set_actor_location(start+(end-start)*(self.step/120),False,True)
            self.pose_time=now
            self.pending=True
        except Exception as exc:
            self.result['errors'].append(str(exc))
            self.finish('failed')

    def sample(self):
        p=self.ship.get_actor_location()
        name,start,end,group=self.routes[self.leg]
        primary=self.data['Mars' if group=='MarsSystem' else 'EarthFragments']
        center=primary['center']
        scale=self.space.visual_scale(p,primary['actor'])
        row=dict(route=name,step=self.step,position=vec(p),scale=scale,bodies={})
        for k,body in self.data.items():
            if str(body['entry'].presentation_group)!=group: continue
            source=self.authored[k]
            actual_center,extent=body['visual'].get_actor_bounds(False)
            actual_ratio=max(extent.x,extent.y,extent.z)/max(source['extent'])
            expected_location=center+(unreal.Vector(*source['authored_location'])-center)*scale
            assert abs(actual_ratio-scale)<1e-5,(name,self.step,k,'radius',actual_ratio,scale)
            assert (body['visual'].get_actor_location()-expected_location).length()<1,(name,self.step,k,'position')
            assert body['visual'].static_mesh_component.is_visible(),(name,self.step,k,'hidden')
            if self.leg<8: assert abs(scale-1)<1e-10
            row['bodies'][k]=dict(actual_ratio=actual_ratio,center=vec(actual_center),
                authored_distance=(p-body['center']).length(),collision=body['visual'].get_actor_enable_collision())
        self.result['samples'].append(row)

    def finish(self,status):
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.movement.stop_movement_immediately()
        self.ship.set_actor_tick_enabled(True)
        self.result['status']=status
        (OUT/'celestial_coherence_native.json').write_text(json.dumps(self.result,indent=2),encoding='utf8')


class ForwardProbe:
    """An initial observer pose followed only by real authoritative flight input."""
    def __init__(self, world, actors, space):
        self.world, self.space = world, space
        self.ship = board(world, actors)
        self.data = members(space)
        self.start = self.ship.get_actor_location()
        self.forward = self.ship.get_actor_forward_vector()
        self.time = unreal.GameplayStatics.get_time_seconds(world)
        self.result = dict(status='running', method='forward-only ServerSetFlightInput; no trajectory teleport', samples=[], errors=[])
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def tick(self, _):
        try:
            elapsed = unreal.GameplayStatics.get_time_seconds(self.world)-self.time
            p = self.ship.get_actor_location()
            distance = (p-self.start).length()
            row = dict(elapsed=elapsed, distance=distance, position=vec(p), forward_dot=unreal.MathLibrary.dot_vector_vector(self.forward,self.ship.get_actor_forward_vector()), bodies={})
            for k in ['Mars','MarI','MarII']:
                body = self.data[k]
                center, extent = body['visual'].get_actor_bounds(False)
                scale = self.space.visual_scale(p, body['actor'])
                row['bodies'][k] = dict(scale=scale, visual_radius=max(extent.x,extent.y,extent.z), distance=(p-center).length())
                assert abs(scale-1)<1e-10, (k,scale)
            assert row['forward_dot']>.99999, row['forward_dot']
            self.result['samples'].append(row)
            if distance>=100000:
                self.finish('complete')
                return
            assert elapsed<30, ('forward input timed out',distance)
            state = unreal.JTSSpacecraftInputState()
            assert state.import_text('(MoveForward=1)')
            self.ship.call_method('ServerSetFlightInput',(state,))
        except Exception as exc:
            self.result['errors'].append(str(exc))
            self.finish('failed')

    def finish(self, status):
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.ship.call_method('ServerSetFlightInput',(unreal.JTSSpacecraftInputState(),))
        self.ship.get_flight_movement_component().stop_movement_immediately()
        self.result['status']=status
        (OUT/'celestial_coherence_forward.json').write_text(json.dumps(self.result,indent=2),encoding='utf8')
