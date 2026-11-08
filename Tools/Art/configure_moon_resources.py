"""Configure the current authored lunar resources in the full Unreal Editor."""
import sys
from pathlib import Path
import unreal

art = str(Path(unreal.Paths.project_dir()).resolve() / "Tools/Art")
if art not in sys.path:
    sys.path.insert(0, art)
import moon_surface_refinement


def configure():
    return moon_surface_refinement.configure()


configure()
