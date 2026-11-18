"""Opt-in editor MCP entry point for the M2.1/M2.2 station structure pass."""
import json
import runpy
import traceback
from pathlib import Path
import unreal as u
import toolset_registry
from toolset_registry.registration import Registration

u.EditorPythonScripting.set_keep_python_script_alive(True)

@u.uclass()
class MarsStationStructureTools(u.ToolsetDefinition):
    @toolset_registry.tool_call
    @staticmethod
    def run(step: str) -> str:
        """Inspect, rebuild or measure only the Mars station structure."""
        if step not in ('inspect', 'plan', 'assets', 'surface', 'interior', 'audit',
                        'prepare', 'probe', 'view', 'close', 'shutdown',
                        'power_inspect','power_assets','power_audit','power_probe'):
            raise ValueError(step)
        try:
            result = runpy.run_path(str(Path(u.Paths.project_dir()) /
                'Tools/Editor/rebuild_mars_station_structure.py'))[step]()
            return json.dumps({'success': True, 'result': result}, ensure_ascii=False)
        except Exception:
            return json.dumps({'success': False, 'error': traceback.format_exc()}, ensure_ascii=False)

_registration = Registration([MarsStationStructureTools])
_registration.register()
u.log('MARS_STATION_STRUCTURE_MCP_READY')
