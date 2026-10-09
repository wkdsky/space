"""Compatibility entry point for the current terrain-envelope flight and landing validation.

Run in a server/standalone SpaceWorld PIE. The report is written to
Docs/Validation/spacecraft_surface_pie.json.
"""
from pathlib import Path
import unreal

_validation_script = Path(unreal.Paths.project_dir()).resolve() / 'Tools/Validation/spacecraft_surface_pie.py'
exec(compile(_validation_script.read_text(encoding='utf-8'), str(_validation_script), 'exec'), globals())
