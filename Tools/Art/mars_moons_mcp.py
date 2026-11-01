"""Project-scoped Unreal MCP entry point for the Mars moon replacement."""
import json
from pathlib import Path
import runpy
import traceback
import unreal
import toolset_registry
from toolset_registry.registration import Registration

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
SCRIPT = Path(unreal.Paths.project_dir()).resolve() / 'Tools/Art/replace_mars_moon_models.py'


@unreal.uclass()
class MarsMoonModelTools(unreal.ToolsetDefinition):
    @toolset_registry.tool_call
    @staticmethod
    def run(step: str) -> str:
        """Inspect, apply, clean up or validate the supplied Mars moon models."""
        allowed = {'scan', 'apply', 'validate', 'cleanup', 'pie_probe', 'pie_status', 'diagnose', 'close_editor'}
        if step not in allowed:
            raise ValueError('Unsupported moon model step: ' + step)
        try:
            result = runpy.run_path(str(SCRIPT))[step]()
            result['success'] = True
        except Exception:
            result = dict(success=False, traceback=traceback.format_exc())
        out = Path(unreal.Paths.project_saved_dir()).resolve() / 'MoonModels'
        out.mkdir(parents=True, exist_ok=True)
        (out / (step + '.json')).write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
        return json.dumps(result, ensure_ascii=False)


_registration = Registration([MarsMoonModelTools])
_registration.register()
unreal.log('JTS_MARS_MOON_MCP_READY')
