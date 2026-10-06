"""Run in the rebuilt Unreal editor through MCP's console.

Reimport the twelve orb-and-staff meshes and update only their catalog configuration.
Animation Blueprint pose defaults are configured separately through MCP ObjectTools.
"""
from pathlib import Path
import runpy
import unreal

art = Path(unreal.Paths.project_dir()).resolve() / "Tools" / "Art"
runpy.run_path(str(art / "import_stellar_held_models.py"), run_name="__main__")
runpy.run_path(str(art / "configure_stellar_held_models.py"), run_name="__main__")
print("STELLAR_SCEPTERS_APPLIED")
