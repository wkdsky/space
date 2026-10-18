"""Verify uninterrupted real PIE playback without changing the camera or seeking.

Run in an isolated UnrealEditor -RenderOffscreen -ExecutePythonScript process.
"""
import json
import time
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
DATA = json.loads((ROOT/'SourceArt/MarsDestruction/ScorchedFractureV3/manifest.json').read_text())
MOTION = json.loads((ROOT/'SourceArt/MarsDestruction/ScorchedFractureV3/fragment_motion.json').read_text())
BONE_NAMES = [r['name'] for r in MOTION]
INNER_NAMES = [r['name'] for r in MOTION if r['layer']=='inner']
FAULT_NAMES = [r['name'] for r in MOTION if r.get('fault_shard')]
OUT = ROOT / 'Saved/MarsDestruction/unreal_scorched_runtime'
OUT.mkdir(parents=True, exist_ok=True)
FOLDER = '/Game/Space/Environment/MarsDestructionPreview/ScorchedFractureV3'
unreal.EditorLevelLibrary.load_level(FOLDER + '/L_MarsScorchedFracture_Preview')
samples = [.5, 3., 3.6, 4., 4.5, 5., 6., 6.5, 7., 7.5, 7.8, 8.1, 8.5, 9., 11., 14., 19., 26., 31., 32., 32.6, 33.2, 33.7, 36., 39., 41.]
state = dict(start=time.monotonic(), index=0, records=[], ending=0.)


def finish(error=None):
    state['ending'] = time.monotonic()
    state['error'] = str(error) if error else None
    if error:
        unreal.log_error('MARS_CONTINUOUS_PLAY_FAILED ' + str(error))
    state['player'] = state['actors'] = state['world'] = state['task'] = None
    unreal.EditorLevelLibrary.editor_end_play()


def tick(delta):
    try:
        now = time.monotonic()
        if state['ending']:
            if now-state['ending'] > 3:
                (OUT / 'validation.json').write_text(json.dumps(dict(
                    revision=DATA['revision'], result='FAIL' if state['error'] else 'PASS', error=state['error'],
                    frames=state['records']), indent=2))
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.SystemLibrary.quit_editor()
            return
        if state.get('capturing'):
            return
        if state.get('task'):
            if not state['task'].is_task_done():
                return
            state['task'] = None
        if state.get('complete'):
            if now-state['complete'] > 1:
                finish()
            return
        if now-state['start'] > 160:
            raise RuntimeError('Continuous PIE test timed out')
        if 'world' not in state:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if not world:
                return
            actors = {a.get_actor_label(): a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)}
            actor = actors.get('Mars_DestructionSequence')
            if not actor or not actor.get_editor_property('sequence_player'):
                return
            state.update(world=world, actors=actors, player=actor.get_editor_property('sequence_player'),
                         game_start=unreal.GameplayStatics.get_time_seconds(world))
            component = actors['Mars_FirstModel_TrueFracture'].skeletal_mesh_component
            assert all(component.does_socket_exist(name) for name in BONE_NAMES)
            state['initial_bone_radii'] = [component.get_socket_location(name).length() for name in BONE_NAMES]
        world, actors = state['world'], state['actors']
        elapsed = unreal.GameplayStatics.get_time_seconds(world)-state['game_start']
        if elapsed < samples[state['index']]:
            return
        qualified = state['player'].get_current_time()
        frame = qualified.time.frame_number.value + qualified.time.sub_frame
        t = frame * qualified.rate.denominator / qualified.rate.numerator
        first, second = actors['Mars_FirstModel_TrueFracture'], actors['Mars_SecondModel_Exploration']
        bone_position=first.skeletal_mesh_component.get_socket_location('MarsPlate_01')-first.get_actor_location()
        outward_distance=bone_position.length()-state['initial_bone_radii'][BONE_NAMES.index('MarsPlate_01')]
        radii = [first.skeletal_mesh_component.get_socket_location(name).length() for name in BONE_NAMES]
        central = actors['Mars_CentralPressureLight']
        controller = unreal.GameplayStatics.get_player_controller(world, 0)
        hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(0, 20000, 0),
            unreal.Vector(0, -20000, 0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
            True, [first], unreal.DrawDebugTrace.NONE, True)
        # Python returns None for a nonblocking trace, HitResult for a hit.
        collision_hit = hit is not None
        record = dict(elapsed=elapsed, sequence_seconds=t,
            first_hidden=first.get_editor_property('hidden'), second_hidden=second.get_editor_property('hidden'),
            collision_enabled=second.get_actor_enable_collision(), physics_trace_hit=collision_hit,
            core_collision_enabled=actors['Mars_InteriorEnergy'].get_actor_enable_collision(),
            energy_geometry_hidden=actors['Mars_InteriorEnergy'].get_editor_property('hidden'),
            energy_geometry_scale=actors['Mars_InteriorEnergy'].get_actor_scale3d().x,
            inner_fragments=len(INNER_NAMES),
            minimum_inner_outward_cm=min(radii[BONE_NAMES.index(n)]-state['initial_bone_radii'][BONE_NAMES.index(n)] for n in INNER_NAMES),
            view_target=controller.get_view_target().get_actor_label(),
            front_plate_outward_cm=outward_distance,
            central_light_position=list(central.get_actor_location().to_tuple()),
            central_light_intensity=central.point_light_component.get_editor_property('intensity'),
            internal_pressure=unreal.MaterialLibrary.get_scalar_parameter_value(world, unreal.load_asset(FOLDER+'/MPC_MarsScorch'), 'InternalPressure'),
            blackout=unreal.MaterialLibrary.get_scalar_parameter_value(world, unreal.load_asset(FOLDER+'/MPC_MarsScorch'), 'Blackout'))
        state['records'].append(record)
        mpc = unreal.load_asset(FOLDER+'/MPC_MarsScorch')
        record['crack_leakage'] = unreal.MaterialLibrary.get_scalar_parameter_value(world, mpc, 'CrackLeakage')
        frame = min(t*DATA['fps'], DATA['leakage_keys'][-1][0])
        for (a,x),(b,y) in zip(DATA['leakage_keys'],DATA['leakage_keys'][1:]):
            if a<=frame<=b:
                expected=x+(y-x)*(frame-a)/(b-a)
                assert abs(record['crack_leakage']-expected)<.002,record
                break
        if 4.<=t<=8.:
            if 'previous_leakage' in state:
                assert record['crack_leakage']>state['previous_leakage'],record
            state['previous_leakage']=record['crack_leakage']
        if 8.<t<19.:
            if 'decaying_leakage' in state:
                assert record['crack_leakage']<state['decaying_leakage'],record
            state['decaying_leakage']=record['crack_leakage']
        if t>=19.:
            assert record['crack_leakage']<.001,record
        record['ray_reach'] = unreal.MaterialLibrary.get_scalar_parameter_value(world, mpc, 'RayReach')
        record['edge_glow'] = unreal.MaterialLibrary.get_scalar_parameter_value(world, mpc, 'EdgeGlow')
        record['rim_morph_weight'] = first.skeletal_mesh_component.get_anim_instance().get_curve_value('ScorchedRim')
        record['rift_morph_weight'] = first.skeletal_mesh_component.get_anim_instance().get_curve_value('TectonicRift')
        record['opening_morph_weight'] = first.skeletal_mesh_component.get_anim_instance().get_curve_value('RiftOpening')
        fault_offsets=[radii[BONE_NAMES.index(n)]-state['initial_bone_radii'][BONE_NAMES.index(n)] for n in FAULT_NAMES]
        record['falling_fault_shards']=len(FAULT_NAMES)
        record['maximum_fault_inward_cm']=-min(fault_offsets)
        record['starfield_visible']=not actors['Mars_CinematicStarfield'].get_editor_property('hidden')
        assert record['starfield_visible'],record
        if 2.5<t<5.:
            assert record['internal_pressure']>.2 and record['opening_morph_weight']>.2,record
            assert record['maximum_fault_inward_cm']>200.,record
        assert central.get_actor_location().length() < .001, record
        assert abs(record['central_light_intensity']-record['internal_pressure']*DATA['central_light_intensity_scale']) < .02, record
        if t >= DATA['energy_core_hide_seconds']:
            assert record['central_light_intensity'] < .001, record
            assert record['energy_geometry_hidden'], record
        elif t >= 4:
            assert not record['energy_geometry_hidden'] and abs(record['energy_geometry_scale']-40.)<.001, record
        if 20 <= t < 33.5:
            assert record['inner_fragments']==DATA['interior_fragments'] and record['minimum_inner_outward_cm']>100., record
        if t <= 8.:
            assert record['rim_morph_weight'] < .001, record
        if 20.2 <= t < 33.5:
            assert record['rim_morph_weight'] > .99, record
        if t > 18.1:
            assert record['ray_reach'] < .001, record
        if 24 <= t < 33.5:
            assert record['edge_glow'] < .021, record
        unreal.log('MARS_CONTINUOUS_PLAY ' + json.dumps(record))
        switched = t >= 33.5
        assert record['first_hidden'] == switched, record
        assert record['second_hidden'] != switched, record
        assert record['collision_enabled'] == switched, record
        assert collision_hit == switched, record
        assert not record['core_collision_enabled'], record
        assert record['view_target'] == ('Mars_ShipView_Preview' if switched else 'Mars_CinematicCamera'), record
        if 20 <= t < 33.5:
            assert outward_distance > 1200, record
        if 1 <= t < DATA['main_drift_seconds']:
            assert abs(outward_distance)<.01 and record['rift_morph_weight']>.05,record
        if 8.5 <= t <= 12.:
            assert record['internal_pressure'] > .25 and outward_distance > 1, record
        if 30 <= t < 33.5:
            if 'late_radii' in state:
                dt = t-state['late_t']
                speeds = [(a-b)/dt for a,b in zip(radii,state['late_radii'])]
                record['minimum_late_outward_speed_cm_per_second'] = min(speeds)
                assert min(speeds) > 1., record
            state['late_radii'], state['late_t'] = radii, t
        if 33 <= t <= 34:
            assert record['blackout'] > .999, record
        # No explicit Camera argument: leave the real player's view target alone.
        state['capturing'] = True
        state['task'] = unreal.AutomationLibrary.take_high_res_screenshot(1280, 720, str(OUT/f'Play_{state["index"]:02d}.png'))
        state['capturing'] = False
        state['index'] += 1
        if state['index'] == len(samples):
            state['complete'] = now
    except Exception as exc:
        finish(exc)


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
