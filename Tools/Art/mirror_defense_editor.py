"""Mirror defence asset authoring and visual QA exposed through project-scoped Unreal MCP."""
import json
import runpy
from pathlib import Path
import unreal
import toolset_registry
from toolset_registry.registration import Registration
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
SCRIPT = Path(unreal.Paths.project_dir()).resolve() / 'Tools/Art/mirror_defense_assets.py'
@unreal.uclass()
class MirrorDefenseTools(unreal.ToolsetDefinition):
    @toolset_registry.tool_call
    @staticmethod
    def run(step: str) -> str:
        """Configure or inspect the MirrorSkimmer defence assets; close saves editor state before rebuilding."""
        assert step in {'configure','tune','inspect','close','balance_start','balance_status','qa_setup','qa_status','qa_one','qa_two','qa_three','qa_clear','qa_side','qa_front','qa_back','qa_occluded','qa_unblock','qa_lookaway','qa_near','qa_death','qa_release','qa_nest','qa_nest_start','qa_nest_status','qa_nest_release'}
        try:
            namespace=runpy.run_path(str(SCRIPT))
            result=namespace[step if not step.startswith('qa_') else 'qa'](*([step] if step.startswith('qa_') else []))
            return json.dumps(result,ensure_ascii=False)
        except Exception:
            import traceback
            return json.dumps(dict(error=traceback.format_exc()))


registration=Registration([MirrorDefenseTools])
registration.register()
unreal.log('JTS_MIRROR_DEFENSE_TOOLS_READY')
