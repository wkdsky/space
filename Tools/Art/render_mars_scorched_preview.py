"""Render the saved V3 Blender scene without rebuilding or changing assets."""
import json
from pathlib import Path
import bpy

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / 'SourceArt/MarsDestruction/ScorchedFractureV3'
data = json.loads((ART / 'manifest.json').read_text())
bpy.ops.wm.open_mainfile(filepath=str(ART / 'Mars_Scorched_Fracture_V3.blend'))
scene = bpy.context.scene
# Preview sampling; source .blend retains the higher quality still settings.
scene.eevee.volumetric_tile_size = '4'
scene.eevee.volumetric_samples = 64
scene.eevee.volumetric_shadow_samples = 32
frames = ROOT / data['render_frames_directory']
for frame in range(0, data['frames'][1] + 1, data['render_step']):
    scene.frame_set(frame)
    scene.render.filepath = str(frames / f'{frame:04d}.png')
    bpy.ops.render.render(write_still=True)
