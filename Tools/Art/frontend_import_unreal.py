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
Path(unreal.Paths.project_dir(),'SourceArt/FrontEnd/unreal_import_report.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
