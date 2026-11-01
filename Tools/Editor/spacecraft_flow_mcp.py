"""Project-scoped Unreal MCP entry point for spacecraft flow inspection and repair."""
import json
from pathlib import Path
import runpy
import traceback
import unreal
import toolset_registry
from toolset_registry.registration import Registration

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
SCRIPT = Path(unreal.Paths.project_dir()).resolve() / 'Tools/Editor/repair_spacecraft_flow.py'


@unreal.uclass()
class SpacecraftFlowTools(unreal.ToolsetDefinition):
    @toolset_registry.tool_call
    @staticmethod
    def run(step: str) -> str:
        """Inspect, repair or validate the project's spacecraft flow, rig and placement."""
        if step not in {'inspect', 'apply', 'pie_probe', 'route_status', 'close_editor'}:
            raise ValueError('Unsupported spacecraft flow step: ' + step)
        try:
            result = runpy.run_path(str(SCRIPT))[step]()
            result['success'] = True
        except Exception:
            result = dict(success=False, traceback=traceback.format_exc())
        out = Path(unreal.Paths.project_saved_dir()).resolve() / 'SpacecraftFlow'
        out.mkdir(parents=True, exist_ok=True)
        (out / (step + '.json')).write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
        return json.dumps(result, ensure_ascii=False)


_registration = Registration([SpacecraftFlowTools])
_registration.register()
unreal.log('JTS_SPACECRAFT_FLOW_MCP_READY')
