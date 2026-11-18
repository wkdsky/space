"""Project-scoped authoring and validation entry point for the Mars chapter."""
import json
import runpy
import traceback
from pathlib import Path
import unreal
import toolset_registry
from toolset_registry.registration import Registration

unreal.EditorPythonScripting.set_keep_python_script_alive(True)

@unreal.uclass()
class MarsContentTools(unreal.ToolsetDefinition):
    @toolset_registry.tool_call
    @staticmethod
    def run(step: str) -> str:
        """Inspect, author, or validate the Mars chapter using its project-owned script."""
        if step not in {'inspect', 'author', 'validate', 'spike', 'runtime_probe', 'refine', 'review'}:
            raise ValueError('Unsupported Mars step: ' + step)
        try:
            script = Path(unreal.Paths.project_dir()).resolve() / 'Tools/Editor/configure_mars_chapter.py'
            if step in {'refine', 'review'}:
                script=script.with_name('refine_mars_chapter.py')
            result = runpy.run_path(str(script))[step]()
            return json.dumps({'success': True, 'result': result}, ensure_ascii=False)
        except Exception:
            return json.dumps({'success': False, 'traceback': traceback.format_exc()})

_registration = Registration([MarsContentTools])
_registration.register()
unreal.log('JTS_MARS_CONTENT_MCP_READY')
