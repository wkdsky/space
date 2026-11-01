"""Project-scoped navigation map authoring and PIE validation through Unreal MCP."""
import json
from pathlib import Path
import runpy
import traceback
import unreal
import toolset_registry
from toolset_registry.registration import Registration

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
SCRIPT = Path(unreal.Paths.project_dir()).resolve() / 'Tools/Editor/navigation_map_authoring.py'


@unreal.uclass()
class NavigationMapTools(unreal.ToolsetDefinition):
    @toolset_registry.tool_call
    @staticmethod
    def run(step: str) -> str:
        """Configure, inspect, capture or validate the three-stage navigation map."""
        allowed = {'inspect_editor', 'configure', 'close_editor', 'inspect_dirty', 'start_qa', 'qa_status', 'capture'}
        if step not in allowed:
            raise ValueError('Unsupported navigation map step: ' + step)
        try:
            result = runpy.run_path(str(SCRIPT))[step]()
            result['success'] = True
        except Exception:
            result = dict(success=False, traceback=traceback.format_exc())
        out = Path(unreal.Paths.project_saved_dir()).resolve() / 'NavigationMap'
        out.mkdir(parents=True, exist_ok=True)
        (out / (step + '.json')).write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
        return json.dumps(result, ensure_ascii=False)


_registration = Registration([NavigationMapTools])
_registration.register()
unreal.log('JTS_NAVIGATION_MAP_MCP_READY')
