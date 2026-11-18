"""Physical mission props. Metres, X reader face, Z up. Uses the established flat-colour palette."""
import ast,bpy,json,math
from pathlib import Path
from mathutils import Vector
OUT=Path('D:/projects/space/SourceArt/MarsStation/Refined')
helpers=[n for n in ast.parse(Path('D:/projects/space/Tools/Art/build_mars_refinement.py').read_text(encoding='utf8')).body if isinstance(n,ast.FunctionDef)]
exec(compile(ast.Module(body=helpers,type_ignores=[]),'<prop-helpers>','exec'))
manifest=json.loads((OUT/'manifest.json').read_text(encoding='utf8'));parts=[];MATS={}
for key,color in manifest['palette'].items():
 m=bpy.data.materials.get('MP_'+key) or bpy.data.materials.new('MP_'+key);m.use_nodes=True
 bs=next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED');bs.inputs['Base Color'].default_value=color;bs.inputs['Roughness'].default_value=.8
 MATS[key]=m
for o in list(bpy.data.objects):
 if o.name.startswith('SM_MP_Flow_'):bpy.data.objects.remove(o,do_unlink=True)
def face(s,p,size=.022,mat='Ceramic'):return txt(s,p,size,mat,(math.pi/2,0,math.pi/2))
# Large readable silhouette: rubber perimeter, inset display, dedicated scan key and real grip.
box((0,0,.19),(.064,.175,.235),'Graphite',.016)
box((.031,0,.19),(.018,.150,.208),'Ceramic',.012)
box((.043,0,.23),(.010,.121,.116),'Graphite',.008)
box((.05,0,.233),(.007,.103,.092),'Cell',.004)
face('LINK',(.055,0,.266),.013,'Glow');face('PERMIT',(.055,0,.24),.010,'Ceramic')
for i in range(4):box((.055,-.034+i*.021,.212),(.002,.013,.007),'Glow',.001)
box((.048,0,.143),(.016,.069,.027),'Orange',.006)
face('SCAN',(.058,0,.14),.012,'Graphite')
for y in (-.048,.048):
 for z in (.121,.151):box((.049,y,z),(.008,.021,.017),'Metal',.003)
for side in (-1,1):
 for z in (.102,.279):box((.002,side*.083,z),(.078,.023,.044),'Orange',.006)
 for z in (.18,.196,.212):box((-.004,side*.09,z),(.054,.007,.006),'Metal',.001)
# Handle fits a closed glove. A void remains between the two side rails.
for y in (-.049,.049):beam((-.025,y,.10),(-.027,y,-.007),.011,'Graphite',8)
beam((-.027,-.049,-.007),(-.027,.049,-.007),.013,'Orange',8)
for y in (-.029,-.01,.01,.029):box((-.027,y,-.010),(.025,.006,.021),'Graphite',.002)
beam((-.013,-.057,.305),(-.013,-.065,.414),.007,'Graphite',8)
uv((-.013,-.065,.414),(.009,.009,.013),'Orange',8,4)
face('FIELD / 01',(.05,0,.103),.009,'Graphite')
finish('Flow_Communicator','simple')
# Detachable charging cradle and scan head, leaving a clear empty silhouette after taking it.
box((-.054,0,.149),(.042,.221,.316),'Graphite',.015)
box((-.078,0,.15),(.013,.25,.36),'Ceramic',.01)
box((0,0,-.03),(.145,.24,.049),'Graphite',.01)
for y in (-.106,.106):box((.004,y,.066),(.11,.024,.18),'Orange',.005)
for y in (-.035,0,.035):box((-.019,y,-.002),(.028,.014,.006),'Metal',.001)
box((.025,.17,.07),(.13,.09,.16),'Ceramic',.008)
box((.096,.17,.09),(.015,.066,.065),'Cell',.004)
face('SYNC',(.106,.17,.087),.012,'Glow')
face('TAKE / RETURN',(.08,0,-.027),.011,'Ceramic')
finish('Flow_CommsDock','simple')
box((0,0,0),(.004,.08,.012),'Glow',.002);finish('Flow_Status','none')
# A thick paper service folder, designed to read from the entry route.
box((0,0,.012),(.43,.55,.024),'Orange',.012)
box((-.21,0,.025),(.018,.54,.022),'Graphite',.004)
for i in range(5):box((.001*i,-.001*i,.028+i*.0018),(.393,.504,.002),'Ceramic',.001)
box((.19,.14,.016),(.05,.14,.013),'Graphite',.004)
txt('POWER FAILURE',(0,.16,.042),.032,'Graphite',(0,0,0))
txt('ENGINEERING / 017',(0,.115,.042),.018,'Graphite',(0,0,0))
for i,s in enumerate(('A / COMBINER OFFLINE','B / COMBINER OFFLINE','C / COMBINER OFFLINE')):
 txt(s,(0,.056-i*.055,.043),.014,'Graphite',(0,0,0))
box((.09,-.17,.043),(.13,.045,.002),'Orange',.003)
txt('LIFT LOCKED',(.09,-.18,.046),.014,'Graphite',(0,0,0))
for i in range(3):box((-.035,-.12-i*.025,.043),(.22,.002,.001),'Metal',0)
finish('Flow_ServiceFolder','simple')
# Portal follows the existing dome opening. No edits to planet or dome vertices.
for y in (-2.66,2.66):
 box((-.95,y,2.15),(2.25,.36,4.3),'Ceramic',.06)
 for x in (-1.8,-.1):box((x,y,2.15),(.20,.5,4.3),'Graphite',.025)
 box((-.9,y,1.15),(1.5,.39,.13),'Orange',.012)
box((-.95,0,4.29),(2.4,5.65,.32),'Graphite',.05)
for y in (-2.64,2.64):box((-.95,y,3.85),(1.9,.12,.12),'Glow',.015)
box((-.95,0,-.04),(2.4,5.65,.08),'Metal',.015)
for y in (-2.25,2.25):box((-.95,y,.016),(2.2,.16,.025),'Orange',.006)
face('RED PLAINS / ECOLOGY',(.29,0,4.31),.20,'Ceramic')
finish('Flow_EntryVestibule','complex')
# Reader: side-mounted, large matching silhouette and a local battery, independent of the dead mains.
box((0,0,1.42),(.22,.46,.74),'Graphite',.04)
box((.13,0,1.46),(.07,.40,.62),'Ceramic',.025)
box((.177,0,1.57),(.025,.31,.22),'Cell',.009)
face('LINK',(.195,0,1.59),.033,'Glow')
face('ACCESS',(.195,0,1.53),.028,'Ceramic')
for y in (-.10,.1):beam((.197,y,1.27),(.197,y,1.41),.008,'Orange',8)
beam((.197,-.1,1.27),(.197,.1,1.27),.008,'Orange',8)
box((-.05,0,.84),(.22,.30,.26),'Metal',.018)
face('AUX BATT',(.07,0,.84),.028,'Graphite')
beam((-.06,0,.94),(-.06,0,1.06),.023,'Graphite',8)
finish('Flow_GateReader','simple')
# Three distinct repairable combiner cabinets; open service bay exposes missing contact bridges.
box((0,0,.78),(.76,1.06,1.45),'Ceramic',.055)
for y in (-.55,.55):box((0,y,.77),(.84,.08,1.46),'Graphite',.018)
box((.405,0,.92),(.10,.88,.87),'Graphite',.02)
for y in (-.28,0,.28):
 box((.47,y,.96),(.08,.15,.47),'Metal',.015)
 for z in (.69,1.23):box((.52,y,z),(.07,.16,.095),'Orange',.008)
 box((.53,y,.94),(.03,.09,.10),'Red',.004)
box((.46,0,1.43),(.08,.85,.16),'Cell',.012)
face('ISOLATED / REPAIR',(.51,0,1.44),.045,'Amber')
box((.1,0,.08),(.96,1.25,.16),'Graphite',.025)
for y in (-.4,.4):beam((.03,y,.13),(.03,y,.34),.07,'Graphite',10)
box((.42,0,.38),(.14,.68,.12),'Orange',.02)
face('POWER COMBINER',(.5,0,.38),.038,'Graphite')
finish('Flow_Combiner','simple')
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
exec(compile(Path('D:/projects/space/Tools/Art/finalize_mars_palette.py').read_text(encoding='utf8'),'<palette>','exec'))
print('Flow props exported')
