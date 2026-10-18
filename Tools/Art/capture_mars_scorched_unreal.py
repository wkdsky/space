"""Capture and validate the V3 sequence in an isolated offscreen Unreal PIE.

Launch UnrealEditor -RenderOffscreen -ExecutePythonScript=<this file>. This never
saves or changes the user's current Editor session, and ends PIE before shutdown.
"""
import json
import time
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
DATA = json.loads((ROOT/'SourceArt/MarsDestruction/ScorchedFractureV3/manifest.json').read_text())
OUT = ROOT / 'Saved/MarsDestruction/unreal_scorched_frames'
OUT.mkdir(parents=True, exist_ok=True)
FOLDER = '/Game/Space/Environment/MarsDestructionPreview/ScorchedFractureV3'
unreal.EditorLevelLibrary.load_level(FOLDER + '/L_MarsScorchedFracture_Preview')
times = [0., 1.5, 3., 4.8, 6., 7.5, 9., 11., 13., 16., 19., 23., 31., 32., 33.25, 33.75, 36., 39.9]
state = dict(start=time.monotonic(), index=0, player=None, task=None, changed=0., ending=0., records=[])


def finish(error=None):
    if error:
        state['error'] = str(error)
        unreal.log_error('MARS_SCORCHED_CAPTURE_FAILED ' + str(error))
    state['ending'] = time.monotonic()
    for key in ('player', 'first', 'second', 'cameras'):
        state[key] = None
    unreal.EditorLevelLibrary.editor_end_play()


def tick(delta):
    try:
        now = time.monotonic()
        if state['ending']:
            if now - state['ending'] > 3:
                (OUT / 'runtime_validation.json').write_text(json.dumps(
                    dict(revision=DATA['revision'], result='FAIL' if state.get('error') else 'PASS', error=state.get('error'), frames=state['records']), indent=2))
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.SystemLibrary.quit_editor()
            return
        if state['task'] == 'pending':
            return
        if now - state['start'] > 180:
            raise RuntimeError('PIE capture timed out')
        if state['player'] is None:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if not world:
                return
            actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            sequence_actor = next((a for a in actors if a.get_actor_label() == 'Mars_DestructionSequence'), None)
            if not sequence_actor:
                return
            player = sequence_actor.get_editor_property('sequence_player')
            if not player:
                return
            player.pause()
            state['player'], state['world'] = player, world
            state['first'] = next(a for a in actors if a.get_actor_label() == 'Mars_FirstModel_TrueFracture')
            state['second'] = next(a for a in actors if a.get_actor_label() == 'Mars_SecondModel_Exploration')
            state['central'] = next(a for a in actors if a.get_actor_label() == 'Mars_CentralPressureLight')
            state['core'] = next(a for a in actors if a.get_actor_label() == 'Mars_InteriorEnergy')
            state['cameras'] = {a.get_actor_label(): a for a in actors if isinstance(a, unreal.CameraActor)}
            energy_material=unreal.load_asset(FOLDER+'/M_MarsInteriorCore')
            assert energy_material.get_editor_property('blend_mode')==unreal.BlendMode.BLEND_ADDITIVE
            flow=unreal.load_asset(FOLDER+'/T_MarsThermalFlow')
            assert not flow.get_editor_property('srgb')
        if state['task']:
            if not state['task'].is_task_done():
                return
            state['task'] = None
            state['index'] += 1
            state['changed'] = 0.
            if state['index'] == len(times):
                finish()
                return
        t = times[state['index']]
        params = unreal.MovieSceneSequencePlaybackParams()
        params.set_editor_property('position_type', unreal.MovieScenePositionType.TIME)
        params.set_editor_property('time', t)
        params.set_editor_property('update_method', unreal.UpdatePositionMethod.JUMP)
        state['player'].set_playback_position(params)
        if not state['changed']:
            state['changed'] = now
        elif now - state['changed'] > 3:
            first, second = state['first'], state['second']
            expected = t >= 33.5
            record = dict(seconds=t, first_hidden=first.get_editor_property('hidden'), second_hidden=second.get_editor_property('hidden'),
                          collision=second.get_actor_enable_collision())
            if hasattr(unreal, 'MaterialLibrary'):
                record['burn_progress'] = unreal.MaterialLibrary.get_scalar_parameter_value(
                    state['world'], unreal.load_asset(FOLDER + '/MPC_MarsScorch'), 'BurnProgress')
                record['blackout'] = unreal.MaterialLibrary.get_scalar_parameter_value(
                    state['world'], unreal.load_asset(FOLDER + '/MPC_MarsScorch'), 'Blackout')
                record['internal_pressure'] = unreal.MaterialLibrary.get_scalar_parameter_value(
                    state['world'], unreal.load_asset(FOLDER + '/MPC_MarsScorch'), 'InternalPressure')
                record['ray_strength'] = unreal.MaterialLibrary.get_scalar_parameter_value(
                    state['world'], unreal.load_asset(FOLDER + '/MPC_MarsScorch'), 'RayStrength')
                record['ray_reach'] = unreal.MaterialLibrary.get_scalar_parameter_value(
                    state['world'], unreal.load_asset(FOLDER + '/MPC_MarsScorch'), 'RayReach')
                record['edge_glow'] = unreal.MaterialLibrary.get_scalar_parameter_value(
                    state['world'], unreal.load_asset(FOLDER + '/MPC_MarsScorch'), 'EdgeGlow')
            record['rim_morph_weight'] = first.skeletal_mesh_component.get_anim_instance().get_curve_value('ScorchedRim')
            record['rift_morph_weight'] = first.skeletal_mesh_component.get_anim_instance().get_curve_value('TectonicRift')
            record['opening_morph_weight'] = first.skeletal_mesh_component.get_anim_instance().get_curve_value('RiftOpening')
            if 1.5<=t<=5.:
                assert record['internal_pressure']>.2 and record['opening_morph_weight']>.04,record
            light = state['central']
            record['central_light_position'] = list(light.get_actor_location().to_tuple())
            record['central_light_intensity'] = light.point_light_component.get_editor_property('intensity')
            record['energy_geometry_hidden'] = state['core'].get_editor_property('hidden')
            record['energy_geometry_scale'] = state['core'].get_actor_scale3d().x
            record['energy_visible_surface']=False
            record['thermal_mode']='radial branching transport in undeformed rock space'
            assert record['energy_geometry_hidden'] == (t>=DATA['energy_core_hide_seconds']), record
            assert abs(record['energy_geometry_scale']-40.)<.001, record
            assert light.get_actor_location().length() < .001, record
            assert abs(record['central_light_intensity'] - record['internal_pressure']*DATA['central_light_intensity_scale']) < .01, record
            if t >= DATA['energy_core_hide_seconds']:
                assert record['central_light_intensity'] < .001, record
            if t <= 8.:
                assert record['rim_morph_weight'] < .001, record
            if 20 <= t < 33.5:
                assert record['rim_morph_weight'] > .99, record
            if 3 <= t < 5:
                assert record['rift_morph_weight'] > .25,record
            if 6 <= t < 33.5:
                assert record['rift_morph_weight'] > .99,record
            controller = unreal.GameplayStatics.get_player_controller(state['world'], 0)
            record['view_target'] = controller.get_view_target().get_actor_label()
            record['camera_position'] = str(controller.player_camera_manager.get_camera_location())
            trace = unreal.SystemLibrary.line_trace_single(
                state['world'], unreal.Vector(0, 20000, 0), unreal.Vector(0, -20000, 0),
                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [first], unreal.DrawDebugTrace.NONE, True)
            record['physics_trace_hit'] = trace is not None
            state['records'].append(record)
            unreal.log('MARS_SCORCHED_RUNTIME ' + json.dumps(record))
            assert record['first_hidden'] == expected, record
            assert record['second_hidden'] != expected, record
            assert record['collision'] == expected, record
            assert record['physics_trace_hit'] == expected, record
            camera = state['cameras']['Mars_ShipView_Preview' if expected else 'Mars_CinematicCamera']
            path = OUT / f'MarsV3_{round(t*100):04d}.png'
            state['task'] = 'pending'
            state['task'] = unreal.AutomationLibrary.take_high_res_screenshot(1280, 720, str(path), camera)
            assert state['task'].is_valid_task()
    except Exception as exc:
        finish(exc)


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
unreal.EditorLevelLibrary.editor_play_simulate()
