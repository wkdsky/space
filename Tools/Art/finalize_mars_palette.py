"""Consolidate authored colour blocks to <=3 material slots without textures of lighting."""
import bpy,json
from pathlib import Path
OUT=Path('D:/projects/space/SourceArt/MarsStation/Refined')
manifest=json.loads((OUT/'manifest.json').read_text(encoding='utf8'));palette=manifest['palette'];names=list(palette)
image=bpy.data.images.new('T_MP_Palette',width=len(names)*8,height=8,alpha=True)
image.colorspace_settings.name='Non-Color'
image.pixels=[v for y in range(8) for n in names for x in range(8) for v in palette[n]]
image.filepath_raw=str(OUT/'T_MP_Palette.png');image.file_format='PNG';image.save()
materials={}
for key in ('MP_Palette','MP_Luminous','MP_Glass'):
 m=bpy.data.materials.get(key) or bpy.data.materials.new(key);m.use_nodes=True
 bs=next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED');bs.inputs['Roughness'].default_value=.85
 tex=m.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image;tex.interpolation='Closest';m.node_tree.links.new(tex.outputs['Color'],bs.inputs['Base Color'])
 if key=='MP_Luminous':m.node_tree.links.new(tex.outputs['Color'],bs.inputs['Emission Color']);bs.inputs['Emission Strength'].default_value=2
 if key=='MP_Glass':bs.inputs['Alpha'].default_value=.13
 materials[key]=m
for key,v in manifest.items():
 if key=='palette':continue
 o=bpy.data.objects[v['mesh']];o.hide_set(False);bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 old=[m.name.removeprefix('MP_').split('.')[0] for m in o.data.materials]
 if all(n in ('Palette','Luminous','Glass') for n in old):
  v['material_slots']=len(old);o.hide_set(True);continue
 # Single source palette keeps each mesh's exact authored part colours.
 for old_uv in list(o.data.uv_layers):o.data.uv_layers.remove(old_uv)
 uv=o.data.uv_layers.new(name='PaletteUV')
 polycolors=[old[p.material_index] for p in o.data.polygons]
 slots=[]
 for p,n in zip(o.data.polygons,polycolors):
  k='MP_Glass' if n=='Glass' else 'MP_Luminous' if n in ('Glow','Amber','Red') else 'MP_Palette'
  if k not in slots:slots.append(k)
  p.material_index=slots.index(k)
  for loop in p.loop_indices:uv.data[loop].uv=((names.index(n)+.5)/len(names),.5)
 o.data.uv_layers.active=uv;o.data.materials.clear()
 for k in slots:o.data.materials.append(materials[k])
 bpy.ops.export_scene.fbx(filepath=str(OUT/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,add_leaf_bones=False)
 v['material_slots']=len(slots);o.hide_set(True)
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Mars_Chapter_Refined.blend'))
print('Palette finalized: '+str(len(manifest)-1)+' modules; max slots '+str(max(v['material_slots'] for k,v in manifest.items() if k!='palette')))
