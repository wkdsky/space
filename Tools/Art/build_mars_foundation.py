"""Adjustable braced greenhouse foundation supports, measured in metres."""
import bpy,ast,json,math
from pathlib import Path
from mathutils import Vector
source=Path('D:/projects/space/Tools/Art/build_mars_refinement.py').read_text(encoding='utf8')
exec(compile(ast.Module(body=[n for n in ast.parse(source).body if isinstance(n,ast.FunctionDef)],type_ignores=[]),'<mars-helpers>','exec'))
OUT=Path('D:/projects/space/SourceArt/MarsStation/Refined');manifest=json.loads((OUT/'manifest.json').read_text());MATS={n:bpy.data.materials.get('MP_'+n) for n in manifest['palette']};parts=[]
for o in list(bpy.data.objects):
 if o.name.startswith('SM_MP_Foundation_Support'):bpy.data.objects.remove(o,do_unlink=True)
box((0,0,.05),(1.5,1.5,.1),'Graphite');box((0,0,.95),(1.5,1.5,.1),'Metal')
for x in (-.55,.55):
 for y in (-.55,.55):beam((x,y,.1),(x,y,.9),.065,'Metal');cyl((x,y,.12),.10,.04,'Orange',6)
 beam((x,-.55,.15),(x,.55,.85),.035,'Orange');beam((x,.55,.15),(x,-.55,.85),.035,'Orange')
for y in (-.55,.55):beam((-.55,y,.15),(.55,y,.85),.035,'Orange')
finish('Foundation_Support');(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2))
