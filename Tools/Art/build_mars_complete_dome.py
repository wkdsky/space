"""Complete hemisphere and a shallow solid footing, with only the authored airlock aperture."""
import bpy,math,json,ast,bmesh
from pathlib import Path
from mathutils import Vector
OUT=Path('D:/projects/space/SourceArt/MarsStation/Refined')
source=Path('D:/projects/space/Tools/Art/build_mars_refinement.py').read_text(encoding='utf8')
exec(compile(ast.Module(body=[n for n in ast.parse(source).body if isinstance(n,ast.FunctionDef)],type_ignores=[]),'<geometry-helpers>','exec'))
manifest=json.loads((OUT/'manifest.json').read_text(encoding='utf8'));parts=[]
MATS={n:bpy.data.materials.get('MP_'+n) for n in manifest['palette']}
for key in ('Flow_DomeRibs','Flow_DomeGlazing','Flow_DomeSeal','Flow_DomeFooting','Flow_ConcreteFoundation','Flow_EntryRamp'):
 old=bpy.data.objects.get('SM_MP_'+key)
 if old:bpy.data.objects.remove(old,do_unlink=True)

def clip(poly,axis,bound,sign):
 inside=[];outside=[]
 for a,b in zip(poly,poly[1:]+poly[:1]):
  da=(a[axis]-bound)*sign;db=(b[axis]-bound)*sign
  (inside if da<=1e-7 else outside).append(a)
  if (da<0<db) or (db<0<da):
   q=a+(b-a)*(da/(da-db));inside.append(q);outside.append(q)
 return inside,outside
def aperture(poly):
 remaining=poly;pieces=[]
 for axis,bound,sign in ((0,-2.55,-1),(0,2.55,1),(1,16,-1),(2,4.1,1)):
  if len(remaining)<3:return pieces
  remaining,side=clip(remaining,axis,bound,sign)
  if len(side)>=3:pieces.append(side)
 return pieces

# Exact equator clipping preserves the complete 19 m hemisphere.
bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3,radius=19)
ico=bpy.context.object;v=[x.co.copy() for x in ico.data.vertices];faces=[list(f.vertices) for f in ico.data.polygons];bpy.data.objects.remove(ico,do_unlink=True)
polys=[]
for face in faces:
 poly,_=clip([v[i] for i in face],2,0,-1)
 if len(poly)>=3:polys.extend(aperture(poly))
vertices=[];panels=[]
for p in polys:
 start=len(vertices);vertices.extend(p);panels.append(tuple(start+i for i in range(len(p))))
mesh('Continuous ETFE hemisphere',vertices,panels,'Glass');finish('Flow_DomeGlazing','none')
mesh('Independent hemisphere collider',vertices,panels,'Ceramic');finish('Flow_DomeSeal','complex')
edges=set()
for p in polys:
 for a,b in zip(p,p[1:]+p[:1]):
  key=tuple(sorted((tuple(round(x,4) for x in a),tuple(round(x,4) for x in b))))
  if key in edges or (b-a).length<.03:continue
  beam(a,b,.055,'Ceramic',8);edges.add(key)
finish('Flow_DomeRibs','complex')
# Continuous sealed base ring, no breach or grille.
for i in range(96):
 a=(i+.5)*math.tau/96;x,y=19*math.cos(a),19*math.sin(a)
 if y>18 and abs(x)<2.7:continue
 box((x,y,.12),(.30,1.26,.24),'Graphite',.012,(0,0,a))
 box((x,y,.27),(.16,1.25,.08),'Orange',.008,(0,0,a))
finish('Flow_DomeFooting','complex')
# Solid slab: 1.1 m deep, most embedded in the gentle real-mesh slope.
bpy.ops.mesh.primitive_cylinder_add(vertices=96,radius=20.3,depth=1.1,location=(0,0,-.63));add(bpy.context.object,'Ceramic')
finish('Flow_ConcreteFoundation','complex')
# A solid sloped entrance apron; no hanging steel stairs or piers.
mesh('Solid concrete entry apron',[(-2.9,18.5,-.08),(2.9,18.5,-.08),(2.9,24,-.60),(-2.9,24,-.60),(-2.9,18.5,-1.2),(2.9,18.5,-1.2),(2.9,24,-1.2),(-2.9,24,-1.2)],[tuple(reversed(f)) for f in [(0,3,2,1),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7),(4,5,6,7)]],'Ceramic')
finish('Flow_EntryRamp','complex')
for key in ('Flow_DomeRibs','Flow_DomeGlazing','Flow_DomeSeal','Flow_DomeFooting','Flow_ConcreteFoundation','Flow_EntryRamp'):
 o=bpy.data.objects['SM_MP_'+key];names=list(manifest['palette']);old=[m.name.removeprefix('MP_').split('.')[0] for m in o.data.materials]
 uv=o.data.uv_layers.new(name='PaletteUV');colors=[old[f.material_index] for f in o.data.polygons];slots=[]
 for f,n in zip(o.data.polygons,colors):
  slot='MP_Glass' if n=='Glass' else 'MP_Palette'
  if slot not in slots:slots.append(slot)
  f.material_index=slots.index(slot)
  for loop in f.loop_indices:uv.data[loop].uv=((names.index(n)+.5)/len(names),.5)
 o.data.materials.clear()
 for slot in slots:o.data.materials.append(bpy.data.materials[slot])
 o.hide_set(False);bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 bpy.ops.export_scene.fbx(filepath=str(OUT/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False)
 manifest[key]['material_slots']=len(slots);o.hide_set(True)
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Mars_Chapter_Refined.blend'))
print({'complete_hemisphere':True,'radius_m':19,'opening':'front airlock only','foundation':'solid concrete','piers':False})
