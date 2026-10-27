"""Opt-in, project-scoped Unreal MCP tools for front-end asset production."""
import json
from pathlib import Path
import runpy
import unreal
import toolset_registry
from toolset_registry.registration import Registration

ROOT = Path(unreal.Paths.project_dir()).resolve()
SCRIPTS = {
    'import': 'frontend_import_unreal.py',
    'lab': 'frontend_lab_unreal.py',
    'configure': 'frontend_configure_unreal.py',
    'validate': 'frontend_validate_unreal.py',
}


@unreal.uclass()
class FrontEndArtTools(unreal.ToolsetDefinition):
    @toolset_registry.tool_call
    @staticmethod
    def run_frontend_step(step: str) -> str:
        """Run import, lab, configure or validate from the project's checked-in front-end scripts."""
        if step not in SCRIPTS:
            raise ValueError('Unknown front-end production step: ' + step)
        namespace = runpy.run_path(str(ROOT / 'Tools' / 'Art' / SCRIPTS[step]))
        return json.dumps(namespace.get('REPORT', {}))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_frontend_editor() -> str:
        """Inspect project, loaded world, dirty packages and front-end assets without modifying them."""
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        return json.dumps({
            'project': str(ROOT), 'world': world.get_path_name() if world else None,
            'dirty_content': [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()],
            'dirty_maps': [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()],
            'assets': list(unreal.EditorAssetLibrary.list_assets('/Game/Space/FrontEnd', recursive=True)),
        })

    @toolset_registry.tool_call
    @staticmethod
    def save_and_close_frontend_editor() -> str:
        """Save dirty packages and close the editor for a normal C++ build (no Live Coding)."""
        assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        unreal.SystemLibrary.quit_editor()
        return 'Saved packages; editor shutdown requested.'


_registration = Registration([FrontEndArtTools])
_registration.register()
unreal.log('JTS_FRONTEND_MCP_REGISTERED')
