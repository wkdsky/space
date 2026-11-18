"""Project-scoped station workflow authoring and isolated PIE probes."""
import json,runpy,traceback
from pathlib import Path
import unreal as u
import toolset_registry
from toolset_registry.registration import Registration
u.EditorPythonScripting.set_keep_python_script_alive(True)
@u.uclass()
class MarsStationFlowTools(u.ToolsetDefinition):
 @toolset_registry.tool_call
 @staticmethod
 def run(step:str)->str:
  if step not in ('inspect','author','audit','finish','prompts','debug_menu','reader_asset','view_entry','view_lift','view_folder','probe','prepare','close'):raise ValueError(step)
  try:
   result=runpy.run_path(str(Path(u.Paths.project_dir())/'Tools/Editor/rebuild_mars_station_flow.py'))[step]()
   return json.dumps({'success':True,'result':result},ensure_ascii=False)
  except Exception:return json.dumps({'success':False,'error':traceback.format_exc()},ensure_ascii=False)
_registration=Registration([MarsStationFlowTools]);_registration.register()
u.log('MARS_STATION_FLOW_MCP_READY')
