"""Compact freestanding lift for the centre of the existing ecology dome."""
import ast, bpy, json, math
from pathlib import Path
from mathutils import Vector

OUT=Path('D:/projects/space/SourceArt/MarsStation/Refined')
helpers=[n for n in ast.parse(Path('D:/projects/space/Tools/Art/build_mars_refinement.py').read_text(encoding='utf8')).body if isinstance(n,ast.FunctionDef)]
exec(compile(ast.Module(body=helpers,type_ignores=[]),'<mars-model-helpers>','exec'))
manifest=json.loads((OUT/'manifest.json').read_text(encoding='utf8'))
NAMES=('Station_LiftShaft','Station_LiftCab','Station_LiftDoorFrame','Station_LiftDoorLeaf','Station_LiftFeed')
RETIRED=('Station_Shell','Station_Deck','Station_LiftCore','Station_PowerRoute','Station_ServiceCabinet','Station_Ramp','Lift_Housing')
MATS={};parts=[]
for name,color in manifest['palette'].items():
 m=bpy.data.materials.get('MP_'+name) or bpy.data.materials.new('MP_'+name);m.use_nodes=True
 bs=next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
 bs.inputs['Base Color'].default_value=color;bs.inputs['Roughness'].default_value=.82
 MATS[name]=m
for old in list(bpy.data.objects):
 if old.name in {'SM_MP_'+n for n in NAMES+RETIRED}:bpy.data.objects.remove(old,do_unlink=True)
for name in RETIRED:manifest.pop(name,None)

def point(p):return Vector((p[0],-p[1],p[2]))
def b(p,s,mat='Ceramic',edge=.02):return box(point(p),s,mat,edge)
def bar(a,c,r=.025,mat='Metal',vertices=8):return beam(point(a),point(c),r,mat,vertices)
def label(s,p,size=.1,mat='Ceramic',side='front'):
 rotations={'front':(math.pi/2,0,0),'right':(math.pi/2,0,math.pi/2),'up':(0,0,0)}
 return txt(s,point(p),size,mat,rotations[side])

# Only the shaft envelope: 4.8 x 5.0 metres. No surrounding lobby or platform.
for x in (-2.30,2.30):
 for y in (-2.40,2.40):
  b((x,y,2.40),(.20,.20,4.80),'Graphite',.025)
  b((x,y,.10),(.20,.20,.20),'Metal')
 for y in (-1.58,0,1.58):
  b((x,y,.55),(.16,1.51,1.10),'Ceramic')
  b((x,y,2.22),(.045,1.44,2.12),'Glass',.004)
  b((x,y,3.49),(.18,1.52,.28),'Graphite')
  b((x,y,4.17),(.16,1.52,1.05),'Ceramic')
  b((x,y,.19),(.18,1.46,.13),'Orange',.006)
 for y in (-.80,.80):b((x,y,2.30),(.19,.065,4.50),'Metal',.008)
 b((x*.96,-1.60,2.33),(.07,.11,4.45),'Metal',.008)
for x in (-1.53,0,1.53):
 b((x,-2.40,.55),(1.46,.16,1.10))
 b((x,-2.40,2.22),(1.39,.045,2.12),'Glass',.004)
 b((x,-2.40,3.49),(1.46,.18,.28),'Graphite')
 b((x,-2.40,4.17),(1.46,.16,1.05))
 b((x,-2.40,.19),(1.46,.18,.13),'Orange',.006)
for x in (-.77,.77):b((x,-2.40,2.30),(.065,.19,4.50),'Metal',.008)
b((0,0,4.74),(4.80,5,.12),'Graphite',.035)
for x in (-1.2,1.2):
 b((x,0,4.83),(2.32,4.82,.06),'Ceramic',.018)
 for y in (-2.15,2.15):b((x,y,4.87),(1.9,.035,.012),'Metal',.002)
for y in (-2.42,2.42):b((0,y,4.65),(4.60,.16,.14),'Metal')
finish('Station_LiftShaft','complex')

# Four-player cab, flush with the existing circular floor. Its door is independent.
b((0,0,-.055),(4.20,4.52,.11),'Graphite',.025)
for x in (-1.56,-.52,.52,1.56):b((x,0,.007),(1.00,4.36,.014),'Metal',.008)
for x in (-1.96,1.96):
 for y in (-2.08,2.08):b((x,y,1.75),(.11,.11,3.50),'Graphite',.012)
 b((x,-.04,.54),(.07,4.10,.96),'Ceramic',.012)
 bar((x*.96,-1.94,1.10),(x*.96,1.84,1.10),.035,'Metal')
 b((x,-.06,.12),(.09,4.04,.09),'Orange',.005)
b((0,-2.08,.55),(3.9,.07,1.0),'Ceramic')
bar((-1.82,-2,1.10),(1.82,-2,1.10),.035,'Metal')
b((0,0,3.52),(4.20,4.52,.12),'Graphite',.025)
b((0,0,3.445),(3.88,4.16,.03),'Ceramic',.01)
for x in (-1.48,1.48):b((x,0,3.416),(.15,2.8,.025),'Glow',.004)
for x in (-.68,.68):
 bar((x,-.8,3.58),(x,-.8,4.35),.035,'Metal')
 o=cyl(point((x,-.8,3.94)),.19,.32,'Metal',16);o.rotation_euler.y=math.pi/2
for x in (-1.85,1.85):b((x,1.8,.025),(.07,.62,.02),'Orange',.003)
label('L01',(0,-1.3,.021),.35,'Graphite','up')
finish('Station_LiftCab','complex')

# Local X is the passage axis, matching JTSAutomaticAccessDoor.
for side in (-1,1):
 y=side*1.80
 b((0,y,1.80),(.28,1.20,3.60),'Ceramic',.025)
 b((.154,y,1.8),(.035,1.04,3.30),'Graphite',.012)
 for z in (.22,3.1):b((.18,y,z),(.018,1.0,.075),'Orange',.005)
 for dy in (-.5,.5):b((.18,y+dy,1.82),(.025,.035,3.05),'Metal',.004)
b((0,0,4.15),(.28,4.80,1.30),'Ceramic',.025)
b((.17,0,3.88),(.055,4.36,.36),'Graphite',.01)
label('LIFT 01  /  B1 - B4',(.205,0,3.89),.135,'Ceramic','right')
b((0,0,.007),(.32,2.40,.014),'Metal',.005)
for y in (-1.19,1.19):b((.14,y,1.74),(.06,.04,3.42),'Metal',.005)
finish('Station_LiftDoorFrame','complex')

b((0,0,1.72),(.13,1.20,3.40),'Graphite',.018)
for x in (-.075,.075):
 b((x,0,1.72),(.025,1.12,3.24),'Ceramic',.015)
 b((x*1.22,0,2.15),(.018,1.03,.40),'Metal',.005)
 b((x*1.22,0,.42),(.018,1.03,.095),'Orange',.005)
 for y in (-.48,.48):b((x*1.22,y,1.30),(.018,.025,.42),'Graphite',.003)
finish('Station_LiftDoorLeaf','simple')

# Feed remains within the shaft footprint, without a hall-side riser or ceiling.
points=[(-2.34,-1.8,.03),(-2.34,-1.8,4.38),(0,-1.8,4.38),(0,-.8,4.38)]
for offset in (-.12,-.04,.04,.12):
 for a,c in zip(points,points[1:]):bar((a[0],a[1]+offset,a[2]),(c[0],c[1]+offset,c[2]),.025,'Orange' if offset<0 else 'Graphite')
for z in (.5,1.5,2.5,3.5,4.3):b((-2.34,-1.8,z),(.10,.42,.08),'Metal',.008)
finish('Station_LiftFeed','none')

# Reuse the established palette and only export the five replacement meshes.
palette=manifest['palette'];colors=list(palette)
for key in NAMES:
 o=bpy.data.objects['SM_MP_'+key];old=[m.name.removeprefix('MP_').split('.')[0] for m in o.data.materials]
 polycolors=[old[p.material_index] for p in o.data.polygons];slots=[]
 for layer in list(o.data.uv_layers):o.data.uv_layers.remove(layer)
 uv=o.data.uv_layers.new(name='PaletteUV')
 for p,n in zip(o.data.polygons,polycolors):
  slot='MP_Glass' if n=='Glass' else 'MP_Luminous' if n in ('Glow','Amber','Red') else 'MP_Palette'
  if slot not in slots:slots.append(slot)
  p.material_index=slots.index(slot)
  for loop in p.loop_indices:uv.data[loop].uv=((colors.index(n)+.5)/len(colors),.5)
 o.data.materials.clear()
 for name in slots:o.data.materials.append(bpy.data.materials[name])
 o.hide_set(False);bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 bpy.ops.export_scene.fbx(filepath=str(OUT/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False)
 manifest[key]['material_slots']=len(slots);o.hide_set(True)
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
bpy.ops.object.make_local(type='ALL')
for library in list(bpy.data.libraries):
 if Path(library.filepath).resolve()==(OUT/'Mars_Chapter_Refined.blend').resolve():
  assert not bpy.data.user_map(subset={library}).get(library,set()), 'Source objects must be local before saving'
  bpy.data.libraries.remove(library)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Mars_Chapter_Refined.blend'))
print('Compact dome lift exported: '+', '.join(NAMES))
