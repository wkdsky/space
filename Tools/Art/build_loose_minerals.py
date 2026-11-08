"""Build only the separate mineral fragments, preserving the authored lunar terrain and rocks."""
from pathlib import Path
import json
import bpy

source=Path('D:/projects/space/Tools/Art/build_moon_resources.py').read_text(encoding='utf-8-sig')
scope={}
exec(compile(source.rsplit('\nbuild()',1)[0],'build_moon_resources.py','exec'),scope)
scene=bpy.data.scenes.new('JTS_LooseMinerals')
bpy.context.window.scene=scene
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
rows=[]
for kind_index,kind in enumerate(('Metal','Crystal')):
    for variant in range(6):
        name=f'SM_MR_{kind}_Loose_{variant+1:02d}'
        obj=scope['loose_mineral'](name,8530+kind_index*100+variant,kind,variant)
        rows.append(scope['export'](obj))
        obj.location=(variant*.5,kind_index*.7,0)
root=Path('D:/projects/space/SourceArt/MoonResources')
(root/'loose_minerals_manifest.json').write_text(json.dumps(dict(assets=rows),indent=2),encoding='utf-8')
bpy.data.libraries.write(str(root/'LooseMinerals.blend'),{scene},path_remap='RELATIVE_ALL',compress=True)
print(json.dumps(dict(created=len(rows),materials={r['name']:r['materials'] for r in rows})))
