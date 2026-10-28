"""Return actionable errors over MCP (Unreal's generic script error otherwise hides tracebacks)."""
import json
from pathlib import Path
import runpy
import traceback
import unreal
try:
    REPORT=runpy.run_path(str(Path(unreal.Paths.project_dir())/'Tools/Art/frontend_import_impl.py'))['REPORT']
    REPORT['success']=True
except Exception:
    REPORT={'success':False,'traceback':traceback.format_exc()}
report=Path(unreal.Paths.project_dir(),'Saved/Art/FrontEnd/unreal_import_report.json')
report.parent.mkdir(parents=True,exist_ok=True)
report.write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
