import sys, json
from pathlib import Path
import unreal, toolset_registry
from toolset_registry.registration import Registration
sys.path.insert(0,str(Path(unreal.Paths.project_dir()).resolve()/"Tools/Art"))
@unreal.uclass()
class PickaxeCombatRevisionTools(unreal.ToolsetDefinition):
    @toolset_registry.tool_call
    @staticmethod
    def run(stage:str)->str:
        """Inspect or configure pickaxe combat, refuge art and the two foragers; run scoped PIE checks."""
        try:
            import pickaxe_combat_actions, importlib
            return json.dumps(importlib.reload(pickaxe_combat_actions).main(stage),ensure_ascii=False)
        except Exception:
            import traceback
            return json.dumps(dict(error=traceback.format_exc()))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
_pickaxe_revision_registration=Registration([PickaxeCombatRevisionTools])
_pickaxe_revision_registration.register()
unreal.log('JTS_PICKAXE_COMBAT_READY')

