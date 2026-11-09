"""Original low-poly soldier revision and permanent organic colony shells, via live Blender MCP."""
import bpy
import json
import math
import random
import sys
from pathlib import Path
from mathutils import Vector

ROOT = Path('D:/projects/space')
OUT = ROOT/'SourceArt/SpireColony'
OUT.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(ROOT/'Tools/Art'))


def load_creatures():
    if not bpy.data.scenes.get('JTS_MoonCreatures_V3'):
        with bpy.data.libraries.load(str(ROOT/'SourceArt/MoonCreaturesV3/MoonCreaturesV3.blend'), link=False) as (source, target):
            target.scenes = ['JTS_MoonCreatures_V3']
    bpy.context.window.scene = bpy.data.scenes['JTS_MoonCreatures_V3']


def soldier():
    load_creatures()
    import importlib
    import moon_creatures_geometry as geometry
    import moon_creatures_motion as motion
    import moon_creatures_delivery as delivery
    importlib.reload(geometry)
    importlib.reload(motion)
    importlib.reload(delivery)
    old = bpy.data.collections.get('CREATURE_SpireSoldier')
    if old:
        for obj in list(old.objects):
            bpy.data.objects.remove(obj, do_unlink=True)
        bpy.data.collections.remove(old)
    for action in list(bpy.data.actions):
        if action.name.startswith('A_SpireSoldier_'):
            bpy.data.actions.remove(action)
    built = geometry.build('SpireSoldier')
    # The old UV textures were painted for the previous topology. Keep this revision's broad matte colour blocks.
    colours = {'Shell':(.34,.38,.40), 'Wear':(.22,.25,.27), 'Joint':(.065,.08,.09), 'Crown':(.66,.61,.50)}
    for mat in bpy.data.objects['SK_SpireSoldier'].data.materials:
        colour=colours[mat.name.rsplit('_',1)[-1]]
        nodes=mat.node_tree.nodes;nodes.clear()
        output=nodes.new('ShaderNodeOutputMaterial');shader=nodes.new('ShaderNodeBsdfPrincipled')
        shader.inputs['Base Color'].default_value=(*colour,1)
        shader.inputs['Roughness'].default_value=.82
        mat.node_tree.links.new(shader.outputs[0],output.inputs[0])
        mat.diffuse_color=(*colour,1)
    animated = motion.bake('SpireSoldier')
    # Only this species' source files are replaced; other creatures retain their authored animation.
    exported = delivery.export_fbx_centimetres('SpireSoldier')
    # Other open Blender scenes may contain selections. Restrict the portable preview to this scene and rig.
    scene,rig=delivery.show('SpireSoldier')
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True);bpy.data.objects['SK_SpireSoldier'].select_set(True)
    bpy.ops.export_scene.gltf(filepath=str(ROOT/'SourceArt/MoonCreaturesV3/SpireSoldier/SpireSoldier.glb'),
        export_format='GLB',use_selection=True,use_active_scene=True,export_animations=True,
        export_animation_mode='ACTIONS',export_skins=True,export_anim_single_armature=False,
        export_force_sampling=True,export_frame_range=False,export_yup=True,export_def_bones=True)
    validation = delivery.validate('SpireSoldier')
    assert validation['passed'], validation['errors']
    manifest_path = ROOT/'SourceArt/MoonCreaturesV3/manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    rig = bpy.data.objects['RIG_SpireSoldier']
    mesh = bpy.data.objects['SK_SpireSoldier']
    row = next(r for r in manifest['creatures'] if r['species'] == 'SpireSoldier')
    row.update(bones=len(rig.data.bones), triangles=len(mesh.data.polygons), clips=json.loads(rig['clips']))
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    return dict(built=built, animation=animated, exported=exported, passed=validation['passed'])


def select(obj):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def colony(variant):
    import moon_creatures_geometry as geometry
    scene = bpy.data.scenes.get('JTS_SpireColonies') or bpy.data.scenes.new('JTS_SpireColonies')
    bpy.context.window.scene = scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1
    name = 'SM_SpireColony_'+str(variant)
    if bpy.data.objects.get(name):
        bpy.data.objects.remove(bpy.data.objects[name], do_unlink=True)
    mats = [geometry.material('M_SpireColony_Shell', (.31,.24,.15), .94),
            geometry.material('M_SpireColony_Ridge', (.45,.36,.24), .94),
            geometry.material('M_SpireColony_Interior', (.0015,.0015,.0015), 1)]
    parts = []
    rng=random.Random(1900+variant)
    # Restore the old upright, blunt-topped tower family, with clearly separated lower satellites.
    # The main sealed column is four metres tall, rather than a broad conical mound.
    centers=((0,0,4.0,.72),(-1.38,.34,1.85,.42),(1.02,1.10,2.35,.43),(.64,-1.26,1.25,.34))
    pores=[]
    for index,(cx,cy,height,radius) in enumerate(centers):
        height *= (1,1.06,.96)[variant] if index==0 else rng.uniform(.9,1.1)
        cx += rng.uniform(-.10,.10) if index else 0
        cy += rng.uniform(-.10,.10) if index else 0
        n = 12
        lean=Vector((rng.uniform(-.065,.065),rng.uniform(-.055,.055),0))
        levels=[(-.16,1.13),(.04,1.08),(.12,1.0),(.32,.92),(.60,.81),(.85,.66),(1,.47)]
        phase=0 if index==0 else rng.uniform(0,math.tau)
        vertices = []
        for z,scale in levels:
            for i in range(n):
                angle = i*math.tau/n+phase
                flute = 1 + .075*math.cos(angle*5+variant*.7)
                vertices.append((cx+radius*scale*math.cos(angle)*flute+lean.x*z*height,
                                 cy+radius*scale*math.sin(angle)*flute+lean.y*z*height,
                                 z*height if z>=0 else z))
        faces = [tuple(reversed(range(n)))]
        for j in range(len(levels)-1):
            for i in range(n):
                faces.append((j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i))
        faces.append(tuple((len(levels)-1)*n+i for i in range(n)))
        data = bpy.data.meshes.new(name+'_spire_'+str(index))
        data.from_pydata(vertices,[],faces);data.update()
        obj = bpy.data.objects.new(name+'_spire_'+str(index),data)
        scene.collection.objects.link(obj)
        for mat in mats: data.materials.append(mat)
        for polygon in data.polygons:
            polygon.material_index = 1 if (polygon.index-1)%n in (2,7) else 0
        parts.append(obj)
        if index == 0:
            # Only tiny black pore eyes at the tower foot: no doorway, frame, arch, apron or separate entrance prop.
            for degrees in (15,105,225):
                angle=math.radians(degrees)
                outward=Vector((math.cos(angle),math.sin(angle),0))
                point=outward*.765+Vector((0,0,.14))
                bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=10,location=point)
                cutter=bpy.context.object;cutter.scale=(.30,.165,.135);cutter.rotation_euler.z=angle
                for mat in mats:cutter.data.materials.append(mat)
                for face in cutter.data.polygons:face.material_index=2
                select(cutter);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
                modifier=obj.modifiers.new('Small_black_foot_pore','BOOLEAN')
                modifier.operation='DIFFERENCE';modifier.solver='EXACT';modifier.material_mode='INDEX';modifier.object=cutter
                select(obj);bpy.ops.object.modifier_apply(modifier=modifier.name)
                bpy.data.objects.remove(cutter,do_unlink=True)
                # Black inset at the back of the shallow pore; it reads as a dark dot, never a window or door.
                side=Vector((-outward.y,outward.x,0));center=point-outward*.22
                ring=[center+side*(.14*math.cos(i*math.tau/12))+Vector((0,0,.117*math.sin(i*math.tau/12))) for i in range(12)]
                black=bpy.data.meshes.new(name+'_pore');black.from_pydata(ring,[],[tuple(range(12))]);black.materials.append(mats[2])
                inset=bpy.data.objects.new(name+'_pore',black);scene.collection.objects.link(inset);parts.append(inset)
                # FBX's Blender-to-UE conversion reflects Y, as in the existing Moon surface pipeline.
                pores.append([outward.x*91,-outward.y*91,0])
    select(parts[0])
    for obj in parts: obj.select_set(True)
    bpy.ops.object.join()
    obj=bpy.context.object;obj.name=name
    tri=obj.modifiers.new('GameTriangles','TRIANGULATE');bpy.ops.object.modifier_apply(modifier=tri.name)
    # Reuse the established mineral material family at the same texture density.
    uv=obj.data.uv_layers.get('UVMap') or obj.data.uv_layers.new(name='UVMap')
    for poly in obj.data.polygons:
        a=math.atan2(poly.normal.y,poly.normal.x)
        for i in poly.loop_indices:
            p=obj.data.vertices[obj.data.loops[i].vertex_index].co
            uv.data[i].uv=((p.x*.38+.5,p.y*.38+.5) if abs(poly.normal.z)>.6 else
                ((-p.x*math.sin(a)+p.y*math.cos(a))*.38+.5,p.z*.38+.5))
    path=OUT/(name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},
        axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',bake_anim=False)
    return dict(name=name,triangles=len(obj.data.polygons),height_cm=max(v.co.z for v in obj.data.vertices)*100,entrances_cm=pores,fbx=str(path))


def build_towers():
    rows=[colony(i) for i in range(3)]
    (OUT/'colony_layout.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'SpireColony.blend'),copy=True,compress=True)
    print(json.dumps(rows))
    return rows


def preview(kind='SpireSoldier',clip='Idle',frame=1):
    import moon_creatures_delivery as delivery
    delivery.stage()
    scene,rig=delivery.show(kind,clip,frame)
    scene.render.engine='CYCLES';scene.cycles.samples=32
    scene.camera.data.ortho_scale=.90
    scene.camera.location=(1.05,-1.25,.79)
    scene.camera.rotation_euler=(Vector((.04,0,.12))-scene.camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.resolution_x=1100;scene.render.resolution_y=900
    scene.render.filepath=str(OUT/(kind+'_'+clip+'.png'))
    bpy.ops.render.render(write_still=True)
    return scene.render.filepath


def build():
    result=dict(soldier=soldier(),colonies=build_towers())
    result['preview']=preview()
    # Save a dedicated editable source without replacing the currently open MirrorBoss file.
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'SpireColony.blend'),copy=True,compress=True)
    print(json.dumps(result))


if __name__=='__main__': build()
