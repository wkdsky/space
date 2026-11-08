"""Sculpt independent, low side burrows from cohesive soil banks in the live Blender MCP scene."""
import bpy
import bmesh
import math
import random
from pathlib import Path
from mathutils import Vector, noise

ROOT = Path('D:/projects/space')
ART = ROOT / 'SourceArt/MoonSurfaceV3/EarthenEntrances'
ART.mkdir(parents=True, exist_ok=True)


def select(obj):
    if bpy.context.object and bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def n(x, y, z=0):
    return noise.noise_vector(Vector((x, y, z)))[0]


def soil_material():
    mat = bpy.data.materials.get('Burrow_ExcavatedRegolith') or bpy.data.materials.new('Burrow_ExcavatedRegolith')
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    nodes.clear()
    output = nodes.new('ShaderNodeOutputMaterial')
    shader = nodes.new('ShaderNodeBsdfPrincipled')
    shader.inputs['Roughness'].default_value = .97
    shader.inputs['Base Color'].default_value = (.25, .245, .23, 1)
    tex = nodes.new('ShaderNodeTexNoise')
    tex.inputs['Scale'].default_value = 32
    tex.inputs['Detail'].default_value = 5
    tex.inputs['Roughness'].default_value = .75
    ramp = nodes.new('ShaderNodeValToRGB')
    ramp.color_ramp.elements[0].position = .22
    ramp.color_ramp.elements[0].color = (.105, .102, .095, 1)
    ramp.color_ramp.elements[1].position = .8
    ramp.color_ramp.elements[1].color = (.36, .35, .325, 1)
    bump = nodes.new('ShaderNodeBump')
    bump.inputs['Strength'].default_value = .4
    bump.inputs['Distance'].default_value = .018
    mat.node_tree.links.new(tex.outputs['Fac'], ramp.inputs[0])
    mat.node_tree.links.new(ramp.outputs[0], shader.inputs['Base Color'])
    mat.node_tree.links.new(tex.outputs['Fac'], bump.inputs['Height'])
    mat.node_tree.links.new(bump.outputs[0], shader.inputs['Normal'])
    mat.node_tree.links.new(shader.outputs[0], output.inputs[0])
    return mat


def bank(variant, collapsed=False):
    rng = random.Random(17841 + variant)
    sx = (1.18, 1.33, 1.16, 1.30)[variant]
    sy = (1.00, .93, 1.12, 1.04)[variant]
    height = (.62, .57, .65, .60)[variant] * (.78 if collapsed else 1)
    cx, cy = (.15, -.04, .22, .06)[variant], (.06, -.12, .09, -.05)[variant]
    count_x, count_y = 61, 55
    vertices, faces = [], []
    for j in range(count_y):
        y = (j / (count_y - 1) * 2 - 1) * sy * 1.22
        for i in range(count_x):
            x = (i / (count_x - 1) * 2 - 1) * sx * 1.22
            irregular = .11 * n(x * 2.6 + variant * 9, y * 2.6, 7)
            r = ((x - cx) / sx)**2 + ((y - cy) / sy)**2
            shape = max(0, 1 - r + irregular)
            clods = .045 * n(x * 9, y * 9, variant * 3) + .018 * n(x * 28, y * 28, variant)
            z = -.035 + height * shape**1.30 + clods * min(1, shape * 7)
            # Uneven shoulders, compacted drag marks and a fan of excavated soil in front.
            z += .075 * math.exp(-((x + .52)**2 / .20 + (y - (.42 if variant % 2 else -.48))**2 / .11))
            z += .055 * math.exp(-((x + .95)**2 / .17 + y*y / .55))
            if x < -.40 and abs(y) < .55:
                grooves = math.exp(-((y - .14 * math.sin(x * 1.6)) / .035)**2)
                grooves += .6 * math.exp(-((y + .18 + .05 * math.sin(x * 2)) / .025)**2)
                z -= .02 * grooves * min(1, shape * 7)
            vertices.append((x, y, z))
    for j in range(count_y - 1):
        for i in range(count_x - 1):
            a = j * count_x + i
            faces.append((a, a + 1, a + 1 + count_x, a + count_x))
    boundary = list(range(count_x))
    boundary += [j * count_x + count_x - 1 for j in range(1, count_y)]
    boundary += list(range((count_y - 1) * count_x + count_x - 2, (count_y - 1) * count_x - 1, -1))
    boundary += [j * count_x for j in range(count_y - 2, 0, -1)]
    bottom = []
    for index in boundary:
        x, y, _ = vertices[index]
        bottom.append(len(vertices)); vertices.append((x, y, -.14))
    for k in range(len(boundary)):
        q = (k + 1) % len(boundary)
        faces.append((boundary[q], boundary[k], bottom[k], bottom[q]))
    faces.append(tuple(reversed(bottom)))
    mesh = bpy.data.meshes.new('ExcavatedSoilBank')
    mesh.from_pydata(vertices, [], faces); mesh.update()
    obj = bpy.data.objects.new('SoilBank', mesh)
    bpy.context.scene.collection.objects.link(obj)
    select(obj)
    bm = bmesh.new(); bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh); bm.free()
    if not collapsed:
        # One bent, low cavity is removed from a solid soil volume. No cylindrical rim is added.
        verts, fs = [], []
        sections = [(-1.80, 0, .16, .38, .25), (-.85, 0, .18, .34, .235),
                    (-.46, .01, .18, .31, .215), (-.08, .055, .17, .275, .19),
                    (.25, .13, .145, .19, .15), (.48, .19, .13, .008, .008)]
        steps = 64
        for ring, (x, yy, zz, ry, rz) in enumerate(sections):
            for k in range(steps):
                a = 2 * math.pi * k / steps
                wobble = 1 + .045 * math.sin(a * 5 + variant) + .025 * math.cos(a * 9 + ring * .4)
                verts.append((x + .02 * math.sin(a * 3), yy + ry * math.cos(a) * wobble,
                              zz + rz * math.sin(a) * wobble))
        for ring in range(len(sections) - 1):
            for k in range(steps):
                q = (k + 1) % steps
                fs.append((ring*steps+k, (ring+1)*steps+k, (ring+1)*steps+q, ring*steps+q))
        fs += [tuple(reversed(range(steps))), tuple(range((len(sections)-1)*steps, len(sections)*steps))]
        cutmesh = bpy.data.meshes.new('SideExcavation')
        cutmesh.from_pydata(verts, [], fs); cutmesh.update()
        cutter = bpy.data.objects.new('SideExcavation', cutmesh); bpy.context.scene.collection.objects.link(cutter)
        bm = bmesh.new(); bm.from_mesh(cutmesh)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces); bm.to_mesh(cutmesh); bm.free()
        boolean = obj.modifiers.new('Excavate the bank', 'BOOLEAN'); boolean.operation = 'DIFFERENCE'; boolean.object = cutter
        boolean.solver = 'FLOAT'
        bpy.ops.object.modifier_apply(modifier=boolean.name)
        bpy.data.objects.remove(cutter, do_unlink=True)
    # Round the broken soil without a smooth tubular lip; retain broad asymmetric dirt masses.
    remesh = obj.modifiers.new('Cohesive dirt', 'REMESH'); remesh.mode = 'VOXEL'; remesh.voxel_size = .018
    bpy.ops.object.modifier_apply(modifier=remesh.name)
    smooth = obj.modifiers.new('Soften packed earth', 'SMOOTH'); smooth.factor = .55; smooth.iterations = 2
    bpy.ops.object.modifier_apply(modifier=smooth.name)
    sculpt_normals = [vertex.normal.copy() for vertex in obj.data.vertices]
    for vertex, normal in zip(obj.data.vertices, sculpt_normals):
        p = vertex.co
        vertex.co += normal * (.006 * n(p.x*47 + variant*7, p.y*47, p.z*47))
    decimate = obj.modifiers.new('Game mesh', 'DECIMATE'); decimate.ratio = .34
    bpy.ops.object.modifier_apply(modifier=decimate.name)
    # Soil crumbs are irregular flattened clods of the same material, not a repeated decorative rock.
    pieces = [obj]
    for k in range(24 if collapsed else 18):
        x, y = rng.uniform(-1.35, -.62), rng.uniform(-.80, .80)
        if not collapsed and abs(y) < .22: continue
        size = rng.uniform(.022, .065)
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1, location=(x, y, .014))
        clod = bpy.context.object
        clod.scale = (size * rng.uniform(1, 1.6), size, size * rng.uniform(.4, .7))
        for v in clod.data.vertices:
            v.co *= rng.uniform(.75, 1.12)
        clod.rotation_euler = (rng.random()*.4, rng.random()*.4, rng.random()*6.28)
        pieces.append(clod)
    bpy.ops.object.select_all(action='DESELECT')
    for piece in pieces: piece.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.join()
    for poly in obj.data.polygons: poly.use_smooth = True
    obj.data.materials.clear(); obj.data.materials.append(soil_material())
    select(obj)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    # Triplanar-style ground UVs retain continuous grain across the lip and its adjacent bank.
    uv = obj.data.uv_layers.new(name='UVMap')
    for polygon in obj.data.polygons:
        for loop in polygon.loop_indices:
            p = obj.data.vertices[obj.data.loops[loop].vertex_index].co
            uv.data[loop].uv = (p.x * .95 + p.z * .4, p.y * .95 + p.z * .6)
    obj.name = ('SM_MS3_EntranceRubble_' if collapsed else 'SM_MS3_SafeSlopingRefuge_') + str(variant)
    return obj


def build():
    scene = bpy.data.scenes.get('MoonEarthenEntrances') or bpy.data.scenes.new('MoonEarthenEntrances')
    bpy.context.window.scene = scene
    for old in list(scene.objects): bpy.data.objects.remove(old, do_unlink=True)
    scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = 1
    rows = []
    for variant in range(4):
        for collapsed in (False, True):
            print(f'Building earthen entrance {variant}, collapsed={collapsed}')
            obj = bank(variant, collapsed)
            select(obj)
            file = ART / (obj.name + '.fbx')
            bpy.ops.export_scene.fbx(filepath=str(file), use_selection=True, object_types={'MESH'},
                                     apply_unit_scale=True, axis_forward='-Y', axis_up='Z', add_leaf_bones=False,
                                     bake_anim=False, use_mesh_modifiers=True)
            rows.append(dict(name=obj.name, triangles=sum(len(p.vertices)-2 for p in obj.data.polygons), size=list(obj.dimensions)))
            obj.location = (variant * 3.4, 3.4 if collapsed else 0, 0)
    bpy.ops.wm.save_as_mainfile(filepath=str(ART / 'MoonEarthenEntrances.blend'))
    print(rows)


if __name__ == '__main__': build()


def textures():
    import numpy as np
    size = 1024
    rng = np.random.default_rng(18391)
    ky = np.fft.fftfreq(size)[:, None] * size
    kx = np.fft.fftfreq(size)[None, :] * size
    radius = np.sqrt(kx*kx + ky*ky)
    def field(frequency):
        a = np.fft.ifft2(np.fft.fft2(rng.normal(size=(size,size))) * np.exp(-(radius/frequency)**2)).real
        return a / max(a.std(), .00001)
    coarse, middle, grain = field(10), field(50), field(200)
    h = .32*coarse + .18*middle + .045*grain
    # Compressed clods, fine dust and tiny dark pores; tileable in every channel.
    albedo = np.clip(.52 + .055*coarse + .035*middle + .022*grain, .30, .72)
    tint = np.array([1.02, 1.00, .965])[None,None,:]
    rgb = np.clip(albedo[:,:,None] * tint, 0, 1)
    dx = (np.roll(h,-1,axis=1)-np.roll(h,1,axis=1))*3
    dy = (np.roll(h,-1,axis=0)-np.roll(h,1,axis=0))*3
    normal = np.stack((-dx,-dy,np.ones_like(h)),axis=2)
    normal /= np.linalg.norm(normal,axis=2)[:,:,None]
    for name, pixels in [('T_MS3_ExcavatedSoil_Color',rgb),('T_MS3_ExcavatedSoil_Normal',normal*.5+.5)]:
        image = bpy.data.images.get(name) or bpy.data.images.new(name,width=size,height=size,alpha=True)
        rgba = np.concatenate((pixels,np.ones((size,size,1))),axis=2).astype(np.float32)
        image.colorspace_settings.name = 'Non-Color'
        image.pixels.foreach_set(rgba.ravel())
        image.filepath_raw = str(ART/(name+'.png')); image.file_format = 'PNG'; image.save()
    print('Soil textures saved')


def preview():
    scene = bpy.data.scenes['MoonEarthenEntrances']; bpy.context.window.scene = scene
    for obj in scene.objects: obj.hide_render = obj.name != 'SM_MS3_SafeSlopingRefuge_0'
    world = bpy.data.worlds.new('EarthenPreviewWorld'); world.use_nodes = True; scene.world = world
    background = next(n for n in world.node_tree.nodes if n.type == 'BACKGROUND')
    background.inputs[0].default_value=(.16,.18,.20,1); background.inputs[1].default_value=.5
    bpy.ops.mesh.primitive_plane_add(size=200, location=(0,0,-.008)); ground=bpy.context.object; ground.name='PreviewGround'
    mat=bpy.data.materials.new('PreviewGroundMat'); mat.diffuse_color=(.30,.30,.29,1); ground.data.materials.append(mat)
    bpy.ops.object.light_add(type='AREA',location=(-3,-4,6)); light=bpy.context.object; light.data.energy=1100; light.data.shape='DISK'; light.data.size=4
    bpy.ops.object.camera_add(location=(-3.4,-2.7,1.6)); camera=bpy.context.object
    camera.rotation_euler=(Vector((-.1,0,.20))-camera.location).to_track_quat('-Z','Y').to_euler(); camera.data.lens=48; scene.camera=camera
    scene.render.resolution_x=1200;scene.render.resolution_y=900;scene.render.resolution_percentage=100
    scene.render.filepath=str(ROOT/'Saved/MoonSurfaceV3/earthen_blender.png')
    scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False
    bpy.ops.render.render(write_still=True)
    print(scene.render.filepath)


def export_buffers():
    import json
    for obj in bpy.data.scenes['MoonEarthenEntrances'].objects:
        if not obj.name.startswith(('SM_MS3_SafeSlopingRefuge_', 'SM_MS3_EntranceRubble_')): continue
        select(obj)
        if len(obj.data.polygons)>20000:
            decimate=obj.modifiers.new('Surface detail budget','DECIMATE');decimate.ratio=.36
            bpy.ops.object.modifier_apply(modifier=decimate.name)
        mesh=obj.data;mesh.calc_loop_triangles()
        positions=[list(v.co*100) for v in mesh.vertices]
        normals=[list(v.normal) for v in mesh.vertices]
        uv=[(v.co.x*.95+v.co.z*.4,v.co.y*.95+v.co.z*.6) for v in mesh.vertices]
        payload=dict(vertices=positions,normals=normals,uv=uv,triangles=[list(t.vertices) for t in mesh.loop_triangles])
        (ART/(obj.name+'.json')).write_text(json.dumps(payload,separators=(',',':')))
        saved=obj.location.copy();obj.location=(0,0,0)
        bpy.ops.export_scene.fbx(filepath=str(ART/(obj.name+'.fbx')),use_selection=True,object_types={'MESH'},
            apply_unit_scale=True,axis_forward='-Y',axis_up='Z',add_leaf_bones=False,bake_anim=False)
        obj.location=saved
    bpy.ops.wm.save_as_mainfile(filepath=str(ART/'MoonEarthenEntrances.blend'))
    print('Exported mesh buffers')
