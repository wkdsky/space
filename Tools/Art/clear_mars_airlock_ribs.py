"""Clear structural dome bars from the independent airlock module's walking opening."""
import bpy,bmesh,json
from pathlib import Path
out=Path('D:/projects/space/SourceArt/MarsStation/Refined')
o=bpy.data.objects.get('SM_MP_Dome_Ribs') or bpy.data.objects['SM_MP_Flow_DomeRibs'];bm=bmesh.new();bm.from_mesh(o.data)
reflected=o.get('station_handedness_fixed',False)
seen=set();remove=[];count=0
for start in bm.verts:
 if start in seen:continue
 stack=[start];group=[];seen.add(start)
 while stack:
  v=stack.pop();group.append(v)
  for edge in v.link_edges:
   n=edge.other_vert(v)
   if n not in seen:seen.add(n);stack.append(n)
 # Each original beam and node is an independent connected shell.
 lo=[min(v.co[i] for v in group) for i in range(3)];hi=[max(v.co[i] for v in group) for i in range(3)]
 if (hi[1]>17.6 if reflected else lo[1]<-17.6) and lo[2]<3.7 and hi[2]>-.2 and lo[0]<1.85 and hi[0]>-1.85:
  remove.extend(group);count+=1
bmesh.ops.delete(bm,geom=remove,context='VERTS');bm.to_mesh(o.data);bm.free();o.data.update()
bpy.ops.object.select_all(action='DESELECT');o.hide_set(False);o.select_set(True);bpy.context.view_layer.objects.active=o
bpy.ops.export_scene.fbx(filepath=str(out/'SM_MP_Dome_Ribs.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False)
manifest=json.loads((out/'manifest.json').read_text(encoding='utf8'));manifest['Dome_Ribs']['triangles']=sum(len(p.vertices)-2 for p in o.data.polygons);(out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
o.hide_set(True);bpy.ops.wm.save_as_mainfile(filepath=str(out/'Mars_Chapter_Refined.blend'));print({'cleared_beams_and_nodes':count,'triangles':manifest['Dome_Ribs']['triangles']})
