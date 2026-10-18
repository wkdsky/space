"""Capture the real player's uninterrupted PIE view for the art review movie.

Uses the existing preview level; does not save or modify Unreal assets.
Frame timestamps retain playback timing when screenshot throughput varies.
"""
import json
import time
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
DATA = json.loads((ROOT/'SourceArt/MarsDestruction/ScorchedFractureV3/manifest.json').read_text())
OUT = ROOT/'Saved/MarsDestruction/unreal_scorched_movie'
OUT.mkdir(parents=True, exist_ok=True)
FOLDER = '/Game/Space/Environment/MarsDestructionPreview/ScorchedFractureV3'
unreal.EditorLevelLibrary.load_level(FOLDER+'/L_MarsScorchedFracture_Preview')
state = dict(start=time.monotonic(), frames=[], next_time=0., ending=0.)


def finish(error=None):
    state['ending'] = time.monotonic()
    state['error'] = str(error) if error else None
    if error:
        unreal.log_error('MARS_MOVIE_FAILED '+str(error))
    state['player'] = state['world'] = None
    unreal.EditorLevelLibrary.editor_end_play()


def tick(delta):
    try:
        now = time.monotonic()
        if state['ending']:
            if now-state['ending'] > 3:
                (OUT/'capture.json').write_text(json.dumps(dict(
                    revision=DATA['revision'], result='FAIL' if state['error'] else 'PASS',
                    error=state['error'], frames=state['frames']), indent=2))
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.SystemLibrary.quit_editor()
            return
        if now-state['start'] > 180:
            raise RuntimeError('PIE movie capture timed out')
        if state.get('capturing'):
            return
        if state.get('pending'):
            if not state['pending'].is_file() or state['pending'].stat().st_size == 0:
                return
            state['pending'] = None
            if state.pop('first_capture', False):
                state['player'].play()
                state['next_time'] = .1
            if state.pop('last_capture', False):
                finish()
                return
        if 'player' not in state:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if not world:
                return
            actors = {a.get_actor_label(): a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)}
            actor = actors.get('Mars_DestructionSequence')
            if not actor or not actor.get_editor_property('sequence_player'):
                return
            player = actor.get_editor_property('sequence_player')
            player.pause()
            params = unreal.MovieSceneSequencePlaybackParams()
            params.set_editor_property('position_type', unreal.MovieScenePositionType.TIME)
            params.set_editor_property('time', 0.)
            params.set_editor_property('update_method', unreal.UpdatePositionMethod.JUMP)
            player.set_playback_position(params)
            state.update(player=player, world=world, ready=now+2.)
        if now < state['ready']:
            return
        q = state['player'].get_current_time()
        t = (q.time.frame_number.value+q.time.sub_frame)*q.rate.denominator/q.rate.numerator
        if t+1.e-5 < state['next_time'] and t < DATA['duration_seconds']-.001:
            return
        index = len(state['frames'])
        path = OUT/f'Viewport_{index:04d}.png'
        path.unlink(missing_ok=True)
        state['frames'].append(dict(seconds=t, file=path.name))
        state['first_capture'] = index == 0
        state['last_capture'] = t >= DATA['duration_seconds']-.001
        state['next_time'] = t+.1
        if index % 50 == 0:
            unreal.log(f'MARS_MOVIE_FRAME {index} time={t:.3f}')
        state['capturing'] = True
        state['pending'] = path
        unreal.SystemLibrary.execute_console_command(state['world'],
            f'Shot -filename="{path.as_posix()}" -nosuffix')
        state['capturing'] = False
    except Exception as exc:
        finish(exc)


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
