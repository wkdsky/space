"""PIE-only regression probe: native owner flight input from Deimos to Mars and back."""
import json
import math
from pathlib import Path
import unreal


class MarsLocalFlightProbe:
    def __init__(self, output_name='local_flight.json', one_way=False):
        self.world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        assert self.world
        self.ship = unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.JTSSpacecraftActor)[0]
        self.manager = unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.JTSSpaceWorldManager)[0]
        self.planets = unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.JTSPlanetAnchor)
        self.deimos = next(a for a in self.planets if str(a.get_planet_id()) == 'MarII')
        self.mars = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.StaticMeshActor) if a.actor_has_tag('Mars'))
        self.mars_center, extent = self.mars.get_actor_bounds(False)
        self.mars_radius = max(extent.x, extent.y, extent.z)
        placement = json.loads((Path(unreal.Paths.project_saved_dir()) / 'MarsSystem' / 'placement.json').read_text(encoding='utf8'))
        self.mars_radius = next(a['radius_cm'] for a in placement['actors'] if a['label'] == 'Mars_Background')
        self.phobos = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.StaticMeshActor) if a.actor_has_tag('MarI'))
        self.phobos_radius = next(a['radius_cm'] for a in placement['actors'] if a['label'] == 'MarI_Background')
        self.phobos_scale = self.phobos.get_actor_scale3d().x
        self.mars_scale = self.mars.get_actor_scale3d().x
        self.deimos_scale = self.deimos.get_gameplay_surface_actor().get_actor_scale3d().x
        self.movement = self.ship.get_flight_movement_component()
        self.output = Path(unreal.Paths.project_saved_dir()) / 'MarsSystem' / output_name
        self.one_way = one_way
        self.result = {'status': 'running', 'method': 'Native flight component and owner ServerSetFlightInput RPC; no trajectory teleport.', 'samples': [], 'legs': []}
        player = unreal.GameplayStatics.get_player_pawn(self.world, 0)
        player.set_actor_location(self.ship.get_boarding_interaction_center(), False, False)
        assert self.ship.try_board_player(player)
        # Prevent the hidden viewport's empty input from overwriting probe input.
        self.ship.set_actor_tick_enabled(False)
        assert self.ship.begin_surface_takeoff()
        self.stage = 'Lift'
        self.started = unreal.GameplayStatics.get_time_seconds(self.world)
        self.next_sample = 0
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        self.save()

    def intent(self, forward=0, lift=0):
        state = unreal.JTSSpacecraftInputState()
        assert state.import_text('(MoveForward=%f,Lift=%f,bFlightAssistEnabled=True,SpeedLimit=0.5)' % (forward, lift))
        self.ship.call_method('ServerSetFlightInput', (state,))

    def begin_leg(self, stage, target):
        self.stage = stage
        forward = (target - self.ship.get_actor_location()).normal()
        old_up = self.ship.get_actor_up_vector()
        up = (old_up - forward * unreal.MathLibrary.dot_vector_vector(old_up, forward)).normal()
        right = unreal.MathLibrary.cross_vector_vector(up, forward)
        self.ship.set_actor_rotation(unreal.MathLibrary.make_rotation_from_axes(forward, right, up), False)
        self.movement.stop_movement_immediately()
        self.movement.activate(True)
        self.started = unreal.GameplayStatics.get_time_seconds(self.world)
        self.next_sample = 0

    def save(self):
        self.output.write_text(json.dumps(self.result, indent=2), encoding='utf8')

    def sample(self, elapsed):
        position = self.ship.get_actor_location()
        center, extent = self.mars.get_actor_bounds(False)
        radius = self.mars_radius*self.mars.get_actor_scale3d().x/self.mars_scale
        distance = (center-position).length()
        phobos_distance = (self.phobos.get_actor_bounds(False)[0]-position).length()
        phobos_ratio = self.phobos.get_actor_scale3d().x/self.phobos_scale
        self.result['samples'].append(dict(leg=self.stage, t=elapsed, location=[position.x, position.y, position.z],
            mars_distance_cm=distance, mars_angle_deg=math.degrees(2*math.atan(radius/distance)),
            mars_scale_ratio=self.mars.get_actor_scale3d().x/self.mars_scale,
            phobos_distance_cm=phobos_distance, phobos_scale_ratio=phobos_ratio,
            phobos_diameter_pixels_1920_fov90=1920*self.phobos_radius*phobos_ratio/phobos_distance,
            phobos_hidden=self.phobos.get_editor_property('hidden'), phobos_collision=self.phobos.get_actor_enable_collision(),
            deimos_scale_ratio=self.deimos.get_gameplay_surface_actor().get_actor_scale3d().x/self.deimos_scale,
            cruise=self.manager.is_cruise_presentation_active(self.ship), speed=self.movement.get_current_speed(),
            background_collision=self.mars.get_actor_enable_collision(),
            foreign_visible=[str(a.get_planet_id()) for a in self.planets if a != self.deimos and not a.get_gameplay_surface_actor().get_editor_property('hidden')]))
        self.save()

    def tick(self, delta):
        try:
            now = unreal.GameplayStatics.get_time_seconds(self.world)
            elapsed = now-self.started
            if elapsed >= self.next_sample:
                self.next_sample = elapsed + 0.1
                self.sample(elapsed)
            if self.stage == 'Lift':
                self.intent(lift=1)
                if not self.ship.get_surface_envelope_component().is_following() and elapsed > 1:
                    self.begin_leg('DeimosToMars', self.mars_center)
                elif elapsed > 30:
                    self.finish('lift_timeout')
                return
            distance = (self.ship.get_actor_location()-self.mars_center).length()
            if self.stage == 'DeimosToMars' and distance <= self.mars_radius+8000:
                self.result['legs'].append({'leg': self.stage, 'seconds': elapsed})
                if self.one_way:
                    self.finish('complete')
                    return
                self.begin_leg('MarsToDeimos', self.deimos.get_planet_center())
            elif self.stage == 'MarsToDeimos' and (self.ship.get_actor_location()-self.deimos.get_planet_center()).length() < 20000:
                self.result['legs'].append({'leg': self.stage, 'seconds': elapsed})
                self.sample(0)
                self.finish('complete')
                return
            elif elapsed > 180:
                self.finish('flight_timeout')
                return
            self.intent(forward=1)
        except Exception as error:
            self.result['error'] = str(error)
            self.finish('error')

    def finish(self, status):
        self.intent()
        self.movement.stop_movement_immediately()
        self.ship.set_actor_tick_enabled(True)
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.result['status'] = status
        self.save()


def run(output_name='local_flight.json', one_way=False):
    unreal._mars_local_flight_probe = MarsLocalFlightProbe(output_name, one_way)
    return json.dumps({'started': True})
