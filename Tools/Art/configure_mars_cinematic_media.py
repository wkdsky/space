"""Create reusable media assets and check the rendered movie in Unreal's decoder.

Run in an isolated full Editor process with -ExecutePythonScript, after encoding.
This configures assets only; the expedition's gameplay trigger remains separate.
"""
import json
import time
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
ART = ROOT / 'SourceArt/MarsDestruction/ScorchedFractureV3'
FOLDER = '/Game/Space/Environment/MarsDestructionPreview/ScorchedFractureV3'
data = json.loads((ART / 'manifest.json').read_text())
revision = data['revision']
tools = unreal.AssetToolsHelpers.get_asset_tools()


def asset(name, cls, factory):
    existing = unreal.load_asset(FOLDER + '/' + name)
    result = existing or tools.create_asset(name, FOLDER, cls, factory())
    assert isinstance(result, cls), name
    return result


source = asset('FMS_MarsDestruction', unreal.FileMediaSource, unreal.FileMediaSourceFactoryNew)
source.set_file_path(str(ROOT / 'Content/Movies/MarsDestruction.mp4'))
source.set_editor_property('precache_file', True)
assert source.validate() and source.get_editor_property('file_path') == './Movies/MarsDestruction.mp4'
player = asset('MP_MarsDestruction', unreal.MediaPlayer, unreal.MediaPlayerFactoryNew)
player.set_editor_property('play_on_open', True)
player.set_looping(False)
texture = asset('MT_MarsDestruction', unreal.MediaTexture, unreal.MediaTextureFactoryNew)
texture.set_editor_property('new_style_output', True)
texture.set_editor_property('auto_clear', True)
texture.set_media_player(player)
material = asset('M_MarsDestruction_UI', unreal.Material, unreal.MaterialFactoryNew)
material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
sample = unreal.MaterialEditingLibrary.create_material_expression(
    material, unreal.MaterialExpressionTextureSample, -250, 0)
sample.set_editor_property('texture', texture)
sample.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
unreal.MaterialEditingLibrary.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
unreal.MaterialEditingLibrary.recompile_material(material)
for item in (source, player, texture, material):
    unreal.EditorAssetLibrary.save_loaded_asset(item)

state = dict(start=time.monotonic())


def finish(error=None):
    size = player.get_video_track_dimensions(0, 0)
    report = dict(revision=revision, result='FAIL' if error else 'PASS', error=error,
                  relative_path=source.get_editor_property('file_path'),
                  player=str(player.get_player_name()), width=size.x, height=size.y,
                  fps=player.get_video_track_frame_rate(0, 0),
                  duration_seconds=unreal.MathLibrary.get_total_seconds(player.get_duration()),
                  played_seconds=unreal.MathLibrary.get_total_seconds(player.get_time()),
                  assets=[item.get_path_name() for item in (source, player, texture, material)])
    (ART / 'validation_unreal_media.json').write_text(json.dumps(report, indent=2))
    unreal.log('MARS_MEDIA_VALIDATION ' + json.dumps(report))
    player.close()
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    try:
        if player.has_error():
            finish('Media backend reported an error')
        elif player.is_playing() and unreal.MathLibrary.get_total_seconds(player.get_time()) > 1.:
            size = player.get_video_track_dimensions(0, 0)
            valid = ((size.x, size.y) == (1920, 1080)
                     and abs(player.get_video_track_frame_rate(0,0)-data['video_fps'])<.01
                     and abs(unreal.MathLibrary.get_total_seconds(player.get_duration()) - 40.) < .01)
            finish(None if valid else 'Decoded dimensions or duration differ from the render')
        elif time.monotonic() - state['start'] > 30.:
            finish('Media playback did not begin within 30 seconds')
    except Exception as exc:
        finish(str(exc))


assert player.open_source(source), 'Unreal refused the FileMediaSource'
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
