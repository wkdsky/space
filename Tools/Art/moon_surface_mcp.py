"""Explicit opt-in MCP entry points for the Moon V3 surface authoring pass."""
import sys
from pathlib import Path
import importlib
import unreal
import toolset_registry
from toolset_registry.registration import Registration

# This file is an interactive MCP bootstrap, not a one-shot headless batch.
unreal.EditorPythonScripting.set_keep_python_script_alive(True)

_art_path = str(Path(unreal.Paths.project_dir()).resolve() / "Tools/Art")
if _art_path not in sys.path:
    sys.path.insert(0, _art_path)

def _surface():
    import moon_surface_unreal
    return importlib.reload(moon_surface_unreal)

@unreal.uclass()
class MoonSurfaceV3Tools(unreal.ToolsetDefinition):
    """Import original Moon art, fit it to the existing mesh, and inspect the result."""
    @toolset_registry.tool_call
    @staticmethod
    def import_species(species: str) -> str:
        """Import one of MirrorSkimmer, Riftlip, SpireWorker, SpireSoldier, its rig and all authored clips."""
        return _surface().import_species(species)
    @toolset_registry.tool_call
    @staticmethod
    def import_surface(batch: int) -> str:
        """Import Moon attachments (batches 0-2), or reimport the separate refuges with correct units (batch 3)."""
        return _surface().import_surface(batch)
    @toolset_registry.tool_call
    @staticmethod
    def author_level() -> str:
        """Apply the Moon V3 recipe in L_SpaceWorld, preserving the corpse and nine entrance locations."""
        return _surface().author_level()
    @toolset_registry.tool_call
    @staticmethod
    def inspect_surface() -> str:
        """Inspect the active level, protected landmarks, new native classes and authored coverage."""
        return _surface().inspect_surface()
    @toolset_registry.tool_call
    @staticmethod
    def capture_scene(view: str) -> str:
        """Capture overview, crater, mirror, rift or tower views using a temporary scene capture."""
        return _surface().capture_scene(view)
    @toolset_registry.tool_call
    @staticmethod
    def validate_play(stage: str) -> str:
        """Validate initial, worker, guard or behavior states in an active server PIE world."""
        return _surface().validate_play(stage)

_moon_surface_registration = Registration([MoonSurfaceV3Tools])
_moon_surface_registration.register()
unreal.log("MOON_SURFACE_V3_TOOLS_REGISTERED")
