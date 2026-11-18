import ast,bpy,json,math
from pathlib import Path
from mathutils import Vector
OUT=Path('D:/projects/space/SourceArt/MarsStation/Refined')
helpers=[n for n in ast.parse(Path('D:/projects/space/Tools/Art/build_mars_refinement.py').read_text(encoding='utf8')).body if isinstance(n,ast.FunctionDef)]
exec(compile(ast.Module(body=helpers,type_ignores=[]),'<helpers>','exec'))
manifest=json.loads((OUT/'manifest.json').read_text(encoding='utf8'));parts=[];MATS={k:bpy.data.materials['MP_'+k] for k in manifest['palette']}
for o in list(bpy.data.objects):
 o.hide_render=False
 if o.name.startswith('FlowReview') or o.name=='SM_MP_Flow_BreachBarrier':bpy.data.objects.remove(o,do_unlink=True)
for y in (-4.35,4.35):
 box((0,y,2.8),(.36,.32,5.6),'Graphite',.035)
 for z in (.35,5.25):box((.20,y,z),(.08,.47,.18),'Orange',.015)
for z in (.18,2.0,5.45):box((0,0,z),(.28,8.75,.25),'Metal',.025)
for i in range(15):box((0,-4.15+i*.59,2.8),(.20,.11,5.15),'Graphite',.012)
beam((.04,-4.2,.4),(.04,0,5.2),.06,'Metal',8);beam((.04,0,5.2),(.04,4.2,.4),.06,'Metal',8)
box((.16,0,2.1),(.12,2.65,.53),'Orange',.025)
txt('STRUCTURAL QUARANTINE',(.23,0,2.1),.13,'Graphite',(math.pi/2,0,math.pi/2))
finish('Flow_BreachBarrier','simple')
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
exec(compile(Path('D:/projects/space/Tools/Art/finalize_mars_palette.py').read_text(encoding='utf8'),'<palette>','exec'))
