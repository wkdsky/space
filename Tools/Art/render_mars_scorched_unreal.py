"""Render the isolated Mars cinematic with Movie Render Queue, one frame per 1/60s.

Start UnrealEditor with -EnablePlugins=MovieRenderPipeline -RenderOffscreen and
-ExecutePythonScript=<this file>. Optional Saved/MarsDestruction/
cinematic_render_request.json restricts the range for lighting review.
"""
import json
import time
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
ART=ROOT/'SourceArt/MarsDestruction/ScorchedFractureV3'
DATA=json.loads((ART/'manifest.json').read_text())
FPS=DATA['video_fps']
request_path=ROOT/'Saved/MarsDestruction/cinematic_render_request.json'
request=json.loads(request_path.read_text()) if request_path.exists() else {}
OUT=ROOT/request.get('output',DATA.get('cinematic_frames_directory','Saved/MarsDestruction/offline_r8'))
OUT.mkdir(parents=True,exist_ok=True)
FOLDER='/Game/Space/Environment/MarsDestructionPreview/ScorchedFractureV3'
LEVEL=FOLDER+'/L_MarsScorchedFracture_Preview'
SEQUENCE=FOLDER+'/LS_MarsScorchedDestruction'
unreal.EditorLevelLibrary.load_level(LEVEL)
# MRQ owns the sequence clock. Disable the preview's autoplay only in this
# isolated process; the saved playable level retains its normal autoplay.
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor,unreal.LevelSequenceActor):
        playback=actor.get_editor_property('playback_settings')
        playback.set_editor_property('auto_play',False)
        actor.set_editor_property('playback_settings',playback)

subsystem=unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
queue=subsystem.get_queue()
queue.delete_all_jobs()
job=queue.allocate_new_job(unreal.MoviePipelineExecutorJob)
job.job_name=f'Mars_R{DATA["revision"]}_Offline_{FPS}fps'
job.map=unreal.SoftObjectPath(LEVEL)
job.sequence=unreal.SoftObjectPath(SEQUENCE)
config=job.get_configuration()
output=config.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
output.output_directory=unreal.DirectoryPath(str(OUT))
output.file_name_format='Mars_{frame_number}'
output.output_resolution=unreal.IntPoint(request.get('width',1920),request.get('height',1080))
output.use_custom_frame_rate=True
output.output_frame_rate=unreal.FrameRate(FPS,1)
output.output_frame_step=1
output.use_custom_playback_range=True
output.custom_start_frame=request.get('start',0)
# MRQ custom ranges use the sequence display rate (30), even when rendering 60.
output.custom_end_frame=request.get('end',DATA['frames'][1])
output.zero_pad_frame_numbers=4
output.override_existing_output=True
config.find_or_add_setting_by_class(unreal.MoviePipelineImageSequenceOutput_PNG)
config.find_or_add_setting_by_class(unreal.MoviePipelineDeferredPassBase)
aa=config.find_or_add_setting_by_class(unreal.MoviePipelineAntiAliasingSetting)
aa.spatial_sample_count=1
aa.temporal_sample_count=1
aa.engine_warm_up_count=32
aa.render_warm_up_count=8
aa.use_camera_cut_for_warm_up=False
game=config.find_or_add_setting_by_class(unreal.MoviePipelineGameOverrideSetting)
game.soft_game_mode_override=unreal.GameModeBase
game.cinematic_quality_settings=True
state=dict(start=time.monotonic(),finished=0.,errors=[])


def errored(executor,pipeline,fatal,message):
    state['errors'].append(dict(fatal=bool(fatal),message=str(message)))
    unreal.log_error('MARS_MRQ_ERROR '+str(message))


def finished(executor,success):
    state.update(finished=time.monotonic(),success=bool(success))


def tick(delta):
    now=time.monotonic()
    if state['finished'] and now-state['finished']>3.:
        files=sorted(OUT.glob('Mars_*.png'))
        factor=FPS/DATA['fps']
        expected=round((output.custom_end_frame-output.custom_start_frame)*factor)
        result=dict(revision=DATA['revision'],result='PASS' if state['success'] and len(files)==expected else 'FAIL',
            errors=state['errors'],frames=len(files),expected_frames=expected,fps=FPS,
            resolution=[output.output_resolution.x,output.output_resolution.y],
            first_frame=round(output.custom_start_frame*factor),end_frame_exclusive=round(output.custom_end_frame*factor),
            output_frame_step=1,source='Movie Render Queue / deferred render / fixed sequence time',
            starfield='/Game/Space/Environment/M_SolarSystemStarfield',
            elapsed_seconds=now-state['start'])
        (OUT/'render_report.json').write_text(json.dumps(result,indent=2))
        unreal.log('MARS_MRQ_COMPLETE '+json.dumps(result))
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
    elif now-state['start']>1800:
        unreal.log_error('MARS_MRQ_TIMEOUT')
        executor.cancel_all_jobs()


executor=unreal.MoviePipelinePIEExecutor()
executor.set_is_rendering_offscreen(True)
executor.set_allow_using_unsaved_levels(True)
executor.on_executor_finished_delegate.add_callable_unique(finished)
executor.on_executor_errored_delegate.add_callable_unique(errored)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
subsystem.render_queue_with_executor_instance(executor)
