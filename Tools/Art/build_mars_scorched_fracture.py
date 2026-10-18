"""Fracture the intact Mars; the supplied debris appears only behind a blackout.

Blender --background --factory-startup --python <this file> -- [--render]
All source surfaces are partitioned by closed, target-guided power cells. There is
no fading shell, hidden second planet, or plate scale animation. One local morph
rounds and erodes only the fracture rims after release.
"""
import json
import math
import sys
from pathlib import Path

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_mars_destruction as common
from mars_thermal_fields import BURN_KEYS, RIFT_KEYS, OPENING_KEYS, bake_flow, rock_shader
from mars_fault_geometry import split_fault_slivers

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'SourceArt/MarsDestruction/ScorchedFractureV3'
FRAMES = ROOT / 'Saved/MarsDestruction/frames_scorched_v3'
FPS, ANIM_END, END, TEAR, RELEASE, STEP = 30, 1005, 1200, 150, 240, 1
OUT.mkdir(parents=True, exist_ok=True)
FRAMES.mkdir(parents=True, exist_ok=True)
RNG = np.random.default_rng(912031)
PRESSURE = [(0,0.),(24,0.),(36,.35),(60,.65),(105,.9),(150,1.15),(210,1.7),
            (240,2.3),(270,1.6),(330,.7),(420,.15),(510,.02),(570,0.),(END,0.)]
# Legacy material parameters are explicitly zeroed when re-authoring the level.
# Actual leakage now uses a stationary central light and volumetric shadows.
RAY_STRENGTH = [(0,0.), (END,0.)]
RAY_REACH = [(0,0.), (END,0.)]
EDGE_GLOW = [(0,0.), (END,0.)]
LEAKAGE = [(0,0.),(105,0.),(120,.22),(150,.48),(180,.78),(210,1.),
           (240,1.25),(255,.70),(270,.40),(300,.18),(330,.10),(420,.03),(510,.005),(570,0.),(END,0.)]
EROSION = [(0,0.), (240,0.), (330,.15), (450,.65), (600,1.), (ANIM_END,1.)]
# Only radiance fades. This fixed bounds mesh has no visible surface shader.
CORE_SCALE = [(0,1.), (END,1.)]
CORE_HIDE = 570


def clip_closed(bm, normal, offset, keep_outer=False):
    cut = bmesh.ops.bisect_plane(bm, geom=list(bm.verts)+list(bm.edges)+list(bm.faces),
        plane_co=normal*offset, plane_no=normal, dist=1e-6,
        clear_inner=keep_outer, clear_outer=not keep_outer)
    edges = [e for e in cut['geom_cut'] if isinstance(e,bmesh.types.BMEdge) and e.is_boundary]
    if edges:
        for face in bmesh.ops.holes_fill(bm,edges=edges,sides=0)['faces']:
            face.material_index=1
            face.smooth=False


def outside_light_pocket(bm):
    """Subtract only a 0.05R-wide light pocket, retaining all surrounding mass."""
    pieces=[]
    for direction in ((1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,1),(0,0,-1)):
        normal=Vector(direction)
        distances=[normal.dot(v.co) for v in bm.verts]
        if not distances:
            break
        if max(distances) <= .025+1e-6:
            continue
        if min(distances) >= .025-1e-6:
            pieces.append(bm)
            return pieces
        outside=bm.copy()
        clip_closed(outside,normal,.025,keep_outer=True)
        pieces.append(outside)
        clip_closed(bm,normal,.025)
    bm.free()
    return pieces


def inner_rock(bm, index, references):
    """Retain the material previously discarded to make the lighting cavity."""
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    if not bm.faces or abs(bm.calc_volume(signed=True)) < 1e-6:
        bm.free()
        return None
    assert all(e.is_manifold for e in bm.edges), index
    bmesh.ops.triangulate(bm,faces=list(bm.faces))
    volume=abs(bm.calc_volume(signed=True))
    center=sum((v.co for v in bm.verts),Vector())/len(bm.verts)
    mesh=bpy.data.meshes.new(f'MarsInnerRock_{index}')
    mesh.materials.append(bpy.data.materials['M_MarsScorchedSurface'])
    mesh.materials.append(bpy.data.materials['M_MarsFractureInterior'])
    for face in bm.faces:
        face.material_index=1
        face.smooth=False
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()
    if not mesh.uv_layers:
        mesh.uv_layers.new(name='UVMap')
    field=mesh.uv_layers.new(name='HeatField')
    impact=Vector((-.15,-.92,.36)).normalized()
    for loop in mesh.loops:
        p=mesh.vertices[loop.vertex_index].co
        arrival=.13+.60*(1-p.normalized().dot(impact))*.5+.06
        field.data[loop.index].uv=(arrival,1.)
    mesh.uv_layers.active_index=0
    mesh.attributes.new('ErosionDelta','FLOAT_VECTOR','POINT')
    mesh.attributes.new('RiftDelta','FLOAT_VECTOR','POINT')
    mesh.attributes.new('OpeningDelta','FLOAT_VECTOR','POINT')
    obj=bpy.data.objects.new(mesh.name,mesh)
    bpy.context.collection.objects.link(obj)
    reference=max(references,key=lambda r:Vector(r['center']).normalized().dot(center.normalized()))
    obj['target_source']=reference['source']
    obj['start_center']=list(center)
    obj['rest_volume']=volume
    obj['outer_area']=0.
    obj['inner_rock']=True
    print('MARS_INNER_ROCK',obj.name,reference['source'],round(volume,6),flush=True)
    return obj,center,reference,volume


def normalize(objects):
    center, diameter = common.box(objects)
    for obj in objects:
        transform = obj.matrix_world.copy()
        obj.parent = None
        for vertex in obj.data.vertices:
            vertex.co = (transform @ vertex.co - center) * 2 / diameter
        obj.matrix_world = Matrix.Identity(4)


def solid_volume(objects):
    total = 0.
    for obj in objects:
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        assert all(e.is_manifold for e in bm.edges), obj.name
        total += abs(bm.calc_volume(signed=True))
        bm.free()
    return total


def animated_value(nodes, name, values):
    node = nodes.new('ShaderNodeValue')
    node.name = name
    for frame, value in values:
        node.outputs[0].default_value = value
        node.outputs[0].keyframe_insert('default_value', frame=frame)
    return node.outputs[0]


def site_data(debris):
    records = []
    for obj in debris:
        verts = [v.co for v in obj.data.vertices]
        volume = abs(sum(verts[p.vertices[0]].dot(verts[p.vertices[1]].cross(verts[p.vertices[2]])) / 6
                         for p in obj.data.polygons))
        records.append(dict(source=obj.name, volume=volume,
                            center=list(sum(verts, Vector()) / len(verts))))
    # Tiny rocks are represented by a smaller number of animation cells. The
    # exploration model still retains every one of its original 107 pieces.
    records = sorted(records, key=lambda r: -r['volume'])[:48]
    sites = np.array([r['center'] for r in records], dtype=np.float64) * .78
    fractions = np.array([r['volume'] for r in records]) ** .85
    fractions /= fractions.sum()
    samples = RNG.normal(size=(26000, 3))
    samples /= np.linalg.norm(samples, axis=1)[:, None]
    samples *= RNG.random((len(samples), 1)) ** (1 / 3) * .98
    base = 2 * samples @ sites.T - np.sum(sites * sites, axis=1)
    weights = np.zeros(len(sites))
    for iteration in range(150):
        labels = np.argmax(base + weights, axis=1)
        actual = np.bincount(labels, minlength=len(sites)) / len(samples)
        gain = .32 if iteration < 80 else .15
        weights += np.clip((fractions - actual) / np.sqrt(fractions) * gain, -.018, .018)
        weights -= weights.mean()
    for i, record in enumerate(records):
        record.update(site=list(sites[i]), weight=float(weights[i]), target_fraction=float(fractions[i]))
    return records, sites, weights


def cells(original, records, sites, weights):
    """Clip the original solid. Every new face is an internal rock cross-section."""
    source_points = [v.co.copy() for v in original.data.vertices]
    source_normals = [v.normal.copy() for v in original.data.vertices]
    source_faces = [list(p.vertices) for p in original.data.polygons]
    source_tree = BVHTree.FromPolygons(source_points, source_faces)
    output = []
    interiors = []
    inner_references = [r for r in records if Vector(r['center']).length < .45]
    assert len(inner_references) >= 5
    for index, record in enumerate(records):
        bm = bmesh.new()
        bm.from_mesh(original.data)
        planes = []
        si = sites[index]
        others = sorted((j for j in range(len(sites)) if j != index),
                        key=lambda j: np.linalg.norm(si - sites[j]))
        for j in others:
            difference = sites[j] - si
            distance = np.linalg.norm(difference)
            normal = Vector(difference / distance)
            offset = (np.dot(sites[j], sites[j]) - weights[j] - np.dot(si, si) + weights[index]) / (2 * distance)
            planes.append((normal, offset))
            signed = [normal.dot(v.co) - offset for v in bm.verts]
            if not signed or min(signed) > 1e-6:
                bm.clear()
                break
            if max(signed) < 1e-6:
                continue
            result = bmesh.ops.bisect_plane(bm, geom=list(bm.verts) + list(bm.edges) + list(bm.faces),
                                           plane_co=normal * offset, plane_no=normal,
                                           dist=1e-6, clear_outer=True, clear_inner=False)
            edges = [e for e in result['geom_cut'] if isinstance(e, bmesh.types.BMEdge) and e.is_boundary]
            if edges:
                cap = bmesh.ops.holes_fill(bm, edges=edges, sides=0)
                for face in cap['faces']:
                    face.material_index = 1
                    face.smooth = False
        if not bm.faces:
            bm.free()
            continue
        # A center sample prevents a clipped triangle with three seam vertices
        # from interpolating as an entirely incandescent polygon.
        bmesh.ops.poke(bm, faces=[f for f in bm.faces if f.material_index == 0],
                       offset=0, use_relative_offset=False, center_mode='MEAN_WEIGHTED')
        motion_center = sum((v.co for v in bm.verts), Vector()) / len(bm.verts)
        # Preserve the approved outer shape and motion center. The inner side
        # is retained below as independently moving solid rock, not discarded.
        radial = motion_center.normalized()
        exterior = [v.co for v in bm.verts if any(f.material_index == 0 for f in v.link_faces)]
        inner_depth = min(.22, min(radial.dot(p) for p in exterior)-.01) if exterior else min(.22,max(radial.dot(v.co) for v in bm.verts)*.65)
        assert inner_depth > 0., (index,inner_depth)
        # Split off the inner side instead of deleting its mass. Each recovered
        # piece is a real closed solid with its own rigid motion. A much smaller
        # central pocket gives the fixed point light room; it is hidden inside
        # the fragment cluster once the visible energy has faded away.
        if min(radial.dot(v.co) for v in bm.verts) < inner_depth-1e-5:
            inside=bm.copy()
            clip_closed(inside,radial,inner_depth)
            for part_index,part in enumerate(outside_light_pocket(inside)):
                rock=inner_rock(part,f'{index:02d}_{part_index:02d}',inner_references)
                if rock:
                    interiors.append(rock)
        cut = bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),
            plane_co=radial*inner_depth,plane_no=radial,dist=1e-6,clear_inner=True,clear_outer=False)
        edges = [e for e in cut['geom_cut'] if isinstance(e,bmesh.types.BMEdge) and e.is_boundary]
        if edges:
            for face in bmesh.ops.holes_fill(bm,edges=edges,sides=0)['faces']:
                face.material_index=1;face.smooth=False
        planes.append((-radial,-inner_depth))
        units=split_fault_slivers(bm,radial,planes,clip_closed) if exterior else [(bm,False)]
        for part_index,(bm,fault_shard) in enumerate(units):
            # Resolve the torn lip locally, retaining the exact original
            # triangles' surface and UV interpolation in the intact pose.
            rim_edges=[e for e in bm.edges if e.calc_length()>.035 and
                any(source_tree.find_nearest(v.co)[3]<.085 and
                    min(d-n.dot(v.co) for n,d in planes)<.09 for v in e.verts)]
            bmesh.ops.subdivide_edges(bm,edges=rim_edges,cuts=1,use_grid_fill=True)
            # Add samples on the cross-sections so rim loss bends a narrow band,
            # rather than pulling one enormous planar cap into a pointed fan.
            bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if f.material_index == 1])
            long_edges = [e for e in bm.edges if e.calc_length() > .09
                          and any(v.co.length > .72 for v in e.verts)
                          and any(f.material_index == 1 for f in e.link_faces)]
            bmesh.ops.subdivide_edges(bm, edges=long_edges, cuts=2, use_grid_fill=True)
            bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
            mesh = bpy.data.meshes.new(f'MarsFault_{index:02d}_{part_index:02d}' if fault_shard else f'MarsPlate_{index:02d}')
            mesh.materials.append(bpy.data.materials['M_MarsScorchedSurface'])
            mesh.materials.append(bpy.data.materials['M_MarsFractureInterior'])
            bm.to_mesh(mesh)
            bm.free()
            mesh.update()
            obj = bpy.data.objects.new(mesh.name, mesh)
            bpy.context.collection.objects.link(obj)
            # Smooth the hot lip into the cross-section. The exterior keeps the
            # original normals, so the intact sphere still has no visible cuts.
            normals = []
            for face in mesh.polygons:
                face.use_smooth = True
                for li in face.loop_indices:
                    p = mesh.vertices[mesh.loops[li].vertex_index].co
                    _, _, fi, surface_distance = source_tree.find_nearest(p)
                    a, b, c = source_faces[fi][:3]
                    n = barycentric_transform(p, source_points[a], source_points[b], source_points[c],
                                              source_normals[a], source_normals[b], source_normals[c]).normalized()
                    if face.material_index == 1:
                        blend = .65 * max(0., 1 - surface_distance/.055)**2
                        n = face.normal.lerp(n, blend).normalized()
                    normals.append(n)
            mesh.normals_split_custom_set(normals)
            mesh.uv_layers.active_index = 0
            field = mesh.uv_layers.new(name='HeatField')
            for face in mesh.polygons:
                for li in face.loop_indices:
                    p = mesh.vertices[mesh.loops[li].vertex_index].co
                    direction = p.normalized()
                    impact = Vector((-.15, -.92, .36)).normalized()
                    front = (1 - direction.dot(impact)) * .5
                    noise = .5 + .5 * math.sin(p.x * 7.1 + math.sin(p.y * 6.7) + p.z * 4.3)
                    arrival = .13 + .60 * front + .12 * noise
                    seam = max(0, min(offset - normal.dot(p) for normal, offset in planes))
                    # FBX flips V. UE therefore reads the seam distance directly in G.
                    field.data[li].uv = (arrival, 1 - seam)
            mesh.uv_layers.active_index = 0
            outer_area = sum(p.area for p in mesh.polygons if p.material_index == 0)
            volume_bm = bmesh.new()
            volume_bm.from_mesh(mesh)
            assert all(e.is_manifold for e in volume_bm.edges), obj.name
            volume = abs(volume_bm.calc_volume(signed=True))
            volume_bm.free()
            center = sum((v.co for v in mesh.vertices),Vector())/len(mesh.vertices) if fault_shard else motion_center
            # Store deltas before joining. A single exported morph carries this
            # closed mesh to a slightly smaller, rounded, irregular lip. No opacity
            # or replacement surface is involved. The bulk of every plate is fixed.
            eroded = mesh.attributes.new('ErosionDelta', 'FLOAT_VECTOR', 'POINT')
            rift = mesh.attributes.new('RiftDelta', 'FLOAT_VECTOR', 'POINT')
            opening = mesh.attributes.new('OpeningDelta','FLOAT_VECTOR','POINT')
            near_rim = 0
            for vertex in mesh.vertices:
                p = vertex.co
                seam = max(0., min(offset - normal.dot(p) for normal, offset in planes))
                surface_distance = source_tree.find_nearest(p)[3]
                influence = max(0., 1 - seam/.065)**2 * max(0., 1 - surface_distance/.075)**2
                radial = p.normalized()
                inward = center - p
                inward -= radial * inward.dot(radial)
                if inward.length > 1e-7:
                    inward.normalize()
                mottling = .5 + .23*math.sin(p.x*39+p.y*23+math.sin(p.z*31)) + .17*math.sin(p.z*57-p.y*41)
                delta = (inward * (.018 + .022*mottling) - radial * (.010 + .010*mottling)) * influence
                if fault_shard:
                    delta*=.08
                eroded.data[vertex.index].vector = delta
                # A broken, deepening trench and opposite lip retreat expose
                # the fixed internal light before the large plates translate.
                rough=.5+.25*math.sin(p.x*83+p.y*47+p.z*59)+.18*math.sin(-p.x*167+p.y*113+p.z*97)
                width=.045+.035*rough
                valley=max(0.,1-seam/width)**.7*max(0.,1-surface_distance/.10)**1.3
                depth=.055+.060*rough
                rift.data[vertex.index].vector=-radial*depth*valley if not fault_shard else Vector()
                normal,offset=min(planes,key=lambda item:item[1]-item[0].dot(p))
                tangent_normal=normal-radial*normal.dot(radial)
                if tangent_normal.length>1.e-6:
                    tangent_normal.normalize()
                band=max(0.,1-seam/.065)**1.2*max(0.,1-surface_distance/.18)
                retreat=.010+.021*rough
                wobble=Vector((math.sin(p.y*17+p.z*13),math.sin(p.z*19-p.x*11),math.sin(p.x*15+p.y*14)))*.024
                wobble+=Vector((math.sin(p.z*61+p.x*43),math.sin(p.x*53+p.y*47),math.sin(p.y*59-p.z*41)))*.006
                wobble-=radial*wobble.dot(radial)
                opening.data[vertex.index].vector=(wobble-tangent_normal*retreat)*band if not fault_shard else Vector()
                near_rim += delta.length > .001
            obj['fault_shard'] = fault_shard
            obj['eroded_rim_vertices'] = near_rim
            obj['target_source'] = record['source']
            obj['start_center'] = list(center)
            obj['rest_volume'] = volume
            obj['outer_area'] = outer_area
            output.append((obj, center, record, volume))
            print('MARS_CELL', obj.name, record['source'], len(mesh.polygons), round(volume, 5), flush=True)
    return output + interiors


def math_node(nodes, links, operation, a, b=None):
    node = nodes.new('ShaderNodeMath')
    node.operation = operation
    for i, value in enumerate((a, b)):
        if value is None:
            continue
        if isinstance(value, (float, int)):
            node.inputs[i].default_value = value
        else:
            links.new(value, node.inputs[i])
    return node.outputs[0]


def heat_material(name, texture=None, interior=False, cooled=False):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    if not cooled:
        return rock_shader(mat,texture,interior,PRESSURE)
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get('Principled BSDF')
    bsdf.inputs['Roughness'].default_value = .83
    base = nodes.new('ShaderNodeTexImage') if texture else nodes.new('ShaderNodeRGB')
    if texture:
        base.image = texture
    else:
        base.outputs[0].default_value = (.075, .026, .014, 1)
    uv = nodes.new('ShaderNodeUVMap')
    uv.uv_map = 'HeatField'
    split = nodes.new('ShaderNodeSeparateXYZ')
    links.new(uv.outputs[0], split.inputs[0])
    arrival = split.outputs['X']
    time = nodes.new('ShaderNodeValue')
    time.name = 'BurnProgress'
    time.outputs[0].default_value = 1.2
    age = math_node(nodes, links, 'SUBTRACT', time.outputs[0], arrival)
    burn = math_node(nodes, links, 'MULTIPLY', age, 1/.18)
    burn = math_node(nodes, links, 'MAXIMUM', burn, 0.)
    burn = math_node(nodes, links, 'MINIMUM', burn, 1.)
    color = nodes.new('ShaderNodeMixRGB')
    links.new(burn, color.inputs[0])
    links.new(base.outputs['Color'], color.inputs[1])
    color.inputs[2].default_value = (.032, .009, .004, 1)
    links.new(color.outputs[0], bsdf.inputs['Base Color'])
    # A hot advancing band followed by red fissures, never an opacity operation.
    band = math_node(nodes, links, 'SUBTRACT', age, .025)
    band = math_node(nodes, links, 'ABSOLUTE', band)
    band = math_node(nodes, links, 'MULTIPLY', band, -28.)
    band = math_node(nodes, links, 'ADD', band, 1.)
    band = math_node(nodes, links, 'MAXIMUM', band, 0.)
    band = math_node(nodes, links, 'POWER', band, 2.)
    coord = nodes.new('ShaderNodeTexCoord')
    noise = nodes.new('ShaderNodeTexNoise')
    noise.inputs['Scale'].default_value = 24
    noise.inputs['Detail'].default_value = 3
    links.new(coord.outputs['UV'], noise.inputs['Vector'])
    veins = math_node(nodes, links, 'SUBTRACT', noise.outputs['Fac'], .5)
    veins = math_node(nodes, links, 'ABSOLUTE', veins)
    veins = math_node(nodes, links, 'MULTIPLY', veins, -20.)
    veins = math_node(nodes, links, 'ADD', veins, 1.)
    veins = math_node(nodes, links, 'MAXIMUM', veins, 0.)
    veins = math_node(nodes, links, 'MINIMUM', veins, 1.)
    residual = math_node(nodes, links, 'MULTIPLY', veins, burn)
    residual = math_node(nodes, links, 'MULTIPLY', residual, .07)
    strength = math_node(nodes, links, 'MULTIPLY', band, 3.8 if not interior else 2.5)
    strength = math_node(nodes, links, 'ADD', strength, residual)
    bsdf.inputs['Emission Color'].default_value = (1, .16, .012, 1)
    links.new(strength, bsdf.inputs['Emission Strength'])
    return mat


def rig(plates, debris_scale):
    arm_data = bpy.data.armatures.new('MarsScorchedFracture_Skeleton')
    arm = bpy.data.objects.new('SK_MarsScorchedFracture', arm_data)
    bpy.context.collection.objects.link(arm)
    bpy.ops.object.select_all(action='DESELECT')
    bpy.context.view_layer.objects.active = arm
    arm.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    root = arm_data.edit_bones.new('root')
    root.head, root.tail = (0, 0, 0), (0, 0, .1)
    for obj, center, _, _ in plates:
        bone = arm_data.edit_bones.new(obj.name)
        bone.head, bone.tail = center, center + Vector((0, 0, .08))
        bone.parent = root
    bpy.ops.object.mode_set(mode='OBJECT')
    # Preserve the approved outer-plate timing when adding small central rocks.
    volumes = np.array([volume for obj, _, _, volume in plates if not obj.get('inner_rock',False) and not obj.get('fault_shard',False)])
    low, high = np.percentile(volumes ** (1 / 3), [5, 95])
    motion = []
    for obj, center, record, volume in plates:
        obj.vertex_groups.new(name=obj.name).add(list(range(len(obj.data.vertices))), 1, 'REPLACE')
        obj.parent = arm
        obj.modifiers.new('RigidPlate', 'ARMATURE').object = arm
        size = float(np.clip((volume ** (1 / 3) - low) / (high - low), 0, 1))
        direction = center.normalized()
        goal = Vector(record['center']) * debris_scale
        tangent = goal - center
        target_radial = tangent.dot(direction)
        tangent -= direction * tangent.dot(direction)
        if tangent.length > .12:
            tangent *= .12 / tangent.length
        distance = max(.35 if obj['outer_area'] > .1 else .18, target_radial)
        inner = obj.get('inner_rock',False)
        fault = obj.get('fault_shard',False)
        if inner:
            distance = min(.50,max(.18,target_radial))
            if tangent.length > .055:
                tangent *= .055/tangent.length
        # Fault slivers collapse first; every solid releases outward together.
        # Translation has an exponential impulse plus a nonzero drift tail, so
        # even the largest pieces keep moving through the full-black frame.
        wave = (1-direction.dot(Vector((0,-.96,.28)).normalized()))*.5
        tear_frame = TEAR + round(25*wave + 5*size)
        if fault:
            tear_frame=30+round(22*wave+8*float(RNG.random()))
        release_frame = RELEASE
        duration = ANIM_END/FPS - release_frame/FPS
        decay = .10 + .075*(1-size)
        offset = direction * distance + tangent
        bend = Vector(RNG.normal(size=3)).normalized()
        bend -= direction*bend.dot(direction)
        bend.normalize()
        outer_vertices = {vi for p in obj.data.polygons if p.material_index == 0 for vi in p.vertices}
        pivot = max((obj.data.vertices[vi].co.copy() for vi in outer_vertices),
                    key=lambda p: p.dot(bend)) if outer_vertices else center+bend*.25
        lever = center-pivot
        axis = lever.cross(direction).normalized()
        angle = math.radians((3.+2.*(1-size))*float(RNG.uniform(.85,1.)))
        if fault:
            angle=math.radians(float(RNG.uniform(12,22)))
        shear = bend*(.008+.006*(1-size))
        final_hinge = Quaternion(axis,angle)@lever-lever+shear
        drift_offset = offset-final_hinge
        pb = arm.pose.bones[obj.name]
        basis = pb.bone.matrix_local.to_quaternion()
        pb.rotation_mode = 'QUATERNION'
        for frame in range(0, ANIM_END + 1, STEP):
            elapsed = min(duration, max(0., (frame-release_frame)/FPS))
            progress = .7*(1-math.exp(-decay*elapsed))/(1-math.exp(-decay*duration)) + .3*elapsed/duration
            stress = min(1.,max(0.,(frame-tear_frame)/((75+15*wave) if fault else (release_frame-tear_frame))))
            stress = stress*stress*(3-2*stress)
            impulse_peel = (1-math.exp(-1.4*elapsed))/(1-math.exp(-1.4*duration))
            peel = .70*stress+.30*impulse_peel if fault else impulse_peel
            rotation = Quaternion(axis,angle*peel)
            hinge = rotation@lever-lever+shear*peel
            collapse=-direction*(.10+.055*wave)*stress*(1-progress) if fault else Vector()
            pb.location = basis.inverted() @ (drift_offset*progress+hinge+collapse)
            pb.rotation_quaternion = basis.inverted() @ rotation @ basis
            pb.scale = (1, 1, 1)
            for prop in ('location', 'rotation_quaternion', 'scale'):
                pb.keyframe_insert(prop, frame=frame, group=obj.name)
        motion.append(dict(name=obj.name, target_source=record['source'], volume=volume,
                           layer='fault' if fault else 'inner' if inner or obj['outer_area'] < .001 else 'outer',
                           fault_shard=bool(fault), collapse_depth=(.10+.055*wave) if fault else 0.,
                           recovered_cavity_rock=bool(inner),
                           outer_area=obj['outer_area'], center=list(center), offset=list(offset),
                           radial_travel=distance, target_radial_travel=target_radial,
                           target_center=list(goal), duration=duration, rotation_degrees=math.degrees(angle),
                           tear_frame=tear_frame, release_frame=release_frame, decay=decay, drift_fraction=.3,
                           hinge_pivot=list(pivot), hinge_axis=list(axis), shear=list(shear)))
    common_linear(arm.animation_data.action)
    arm.animation_data.action.name = 'A_MarsScorchedFracture'
    (OUT / 'fragment_motion.json').write_text(json.dumps(motion, indent=2))
    return arm


def interior_effects(plates, arm, source):
    """A stationary core illuminates local medium through real moving rock."""
    core_mat = bpy.data.materials.new('M_MarsInteriorCore')
    core_mat.use_nodes = True
    nodes, links = core_mat.node_tree.nodes, core_mat.node_tree.links
    nodes.clear()
    out = nodes.new('ShaderNodeOutputMaterial')
    # A soft density field emits light throughout space. No surface input is
    # connected, so there is no luminous sphere, shrinking contour or black mask.
    position=nodes.new('ShaderNodeTexCoord')
    radius=nodes.new('ShaderNodeVectorMath');radius.operation='LENGTH'
    links.new(position.outputs['Object'],radius.inputs[0])
    falloff=math_node(nodes,links,'SUBTRACT',.94,radius.outputs['Value'])
    falloff=math_node(nodes,links,'MINIMUM',math_node(nodes,links,'MAXIMUM',math_node(nodes,links,'MULTIPLY',falloff,1/.22),0.),1.)
    falloff=math_node(nodes,links,'POWER',falloff,2.)
    noise=nodes.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=6.
    noise.inputs['Detail'].default_value=2.
    links.new(position.outputs['Object'],noise.inputs['Vector'])
    turbulence=math_node(nodes,links,'ADD',.55,math_node(nodes,links,'MULTIPLY',noise.outputs['Fac'],.45))
    radiance=math_node(nodes,links,'MULTIPLY',falloff,turbulence)
    pressure=animated_value(nodes,'InternalPressure',PRESSURE)
    burst=math_node(nodes,links,'MULTIPLY',pressure,math_node(nodes,links,'ADD',1.,pressure))
    radiance=math_node(nodes,links,'MULTIPLY',radiance,burst)
    volume_emission=nodes.new('ShaderNodeVolumePrincipled')
    volume_emission.inputs['Density'].default_value=0.
    volume_emission.inputs['Emission Color'].default_value=(1,.40,.10,1)
    links.new(math_node(nodes,links,'MULTIPLY',radiance,24.),volume_emission.inputs['Emission Strength'])
    links.new(volume_emission.outputs[0],out.inputs['Volume'])
    bpy.ops.mesh.primitive_cube_add(size=2.24,location=(0,0,0))
    core=bpy.context.object
    core.name = 'Mars_InteriorEnergy'
    core.data.materials.clear()
    core.data.materials.append(core_mat)
    core.visible_shadow = False
    for frame,scale in CORE_SCALE:
        core.scale=(scale,scale,scale)
        core.keyframe_insert('scale',frame=frame)
    common_linear(core.animation_data.action)
    # Export only a fixed volume bounds cube. It never supplies visible faces.
    bpy.context.scene.frame_set(0)
    export_fbx([core],'SM_MarsInteriorCore.fbx')
    bpy.ops.object.light_add(type='POINT',location=(0,0,0))
    lamp = bpy.context.object
    lamp.name = 'Mars_CentralPressureLight'
    lamp.data.color = (1,.40,.10)
    lamp.data.shadow_soft_size = .012
    lamp.data.use_shadow = True
    for frame,pressure in PRESSURE:
        lamp.data.energy = 2700*pressure
        lamp.data.keyframe_insert('energy',frame=frame)
    common_linear(lamp.data.animation_data.action)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32,ring_count=16,radius=1.8)
    fog = bpy.context.object
    fog.name = 'Mars_LocalScatteringMedium'
    volume = bpy.data.materials.new('M_MarsLocalScattering')
    volume.use_nodes = True
    nodes,links = volume.node_tree.nodes,volume.node_tree.links
    nodes.clear()
    out = nodes.new('ShaderNodeOutputMaterial')
    scatter = nodes.new('ShaderNodeVolumeScatter')
    scatter.inputs['Color'].default_value = (1,.82,.64,1)
    scatter.inputs['Anisotropy'].default_value = .15
    density = math_node(nodes,links,'MULTIPLY',animated_value(nodes,'InternalPressure',PRESSURE),.10)
    links.new(density,scatter.inputs['Density'])
    links.new(scatter.outputs[0],out.inputs['Volume'])
    fog.data.materials.append(volume)
    fog.visible_shadow = False
    for frame,hidden in ((0,False),(CORE_HIDE-1,False),(CORE_HIDE,True)):
        fog.hide_render = hidden
        fog.keyframe_insert('hide_render',frame=frame)
    boundary_edges = 0
    for obj,_,_,_ in plates:
        bm=bmesh.new();bm.from_mesh(obj.data)
        boundary_edges += sum({f.material_index for f in e.link_faces} == {0,1} for e in bm.edges)
        bm.free()
    (OUT/'crack_coverage.json').write_text(json.dumps(dict(
        boundary_edges=boundary_edges, emitted_edges=0, attached_light_cards=0,
        method='Fixed central point light, local scattering medium and actual moving opaque occluders.',
        light_origin=[0,0,0], light_source_follows_plates=False),indent=2))
    return [], core


def common_linear(action):
    for slot in action.slots:
        for layer in action.layers:
            for strip in layer.strips:
                bag = strip.channelbag(slot)
                if bag:
                    for curve in bag.fcurves:
                        for key in curve.keyframe_points:
                            key.interpolation = 'LINEAR'


def export_fbx(objects, filename, animated=False):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.export_scene.fbx(filepath=str(OUT / filename), use_selection=True,
                             object_types={'ARMATURE', 'MESH'} if animated else {'MESH'},
                             add_leaf_bones=False, bake_anim=animated,
                             bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                             bake_anim_use_all_actions=False, bake_anim_simplify_factor=0,
                             axis_forward='-Y', axis_up='Z', path_mode='RELATIVE', mesh_smooth_type='FACE')


def main():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    scene = bpy.context.scene
    scene.name = 'Mars_Scorched_Fracture_V3'
    intact = common.source('mars planet 3d model/mars+planet+3d+model.fbx')
    debris = common.source('asteroid 3d model/asteroid+3d+model.fbx')
    scene.render.fps, scene.render.fps_base = FPS, 1.
    scene.frame_start, scene.frame_end = 0, ANIM_END
    normalize(intact)
    normalize(debris)
    original = intact[0]
    original.data.update()
    texture = next(n.image for n in original.data.materials[0].node_tree.nodes if n.type == 'TEX_IMAGE' and n.image)
    texture.scale(4096, 4096)
    texture.filepath_raw, texture.file_format = str(OUT / 'T_MarsSurface_4K.png'), 'PNG'
    texture.save()
    bake_flow(OUT/'T_MarsThermalFlow.png')
    heat_material('M_MarsScorchedSurface', texture)
    heat_material('M_MarsFractureInterior', interior=True)
    records, sites, weights = site_data(debris)
    (OUT / 'fracture_sites.json').write_text(json.dumps(records, indent=2))
    plates = cells(original, records, sites, weights)
    assert len(plates) >= 36
    source_area = sum(p.area for p in original.data.polygons)
    source_bm = bmesh.new()
    source_bm.from_mesh(original.data)
    source_volume = abs(source_bm.calc_volume(signed=True))
    source_bm.free()
    debris_volume = solid_volume(debris)
    debris_scale = (source_volume / debris_volume) ** (1/3)
    compacted_radius = (debris_volume * 3 / (4*math.pi)) ** (1/3)
    print('MARS_COMPACTION', debris_volume, compacted_radius, debris_scale, flush=True)
    exterior_area = sum(obj['outer_area'] for obj, _, _, _ in plates)
    volume = sum(v for _, _, _, v in plates)
    assert abs(exterior_area - source_area) / source_area < 1e-4, (exterior_area, source_area)
    # The supplied source has two closed islands; cap tessellation around its
    # concave canyon has a small numerical volume discrepancy, recorded below.
    assert .99 < volume/source_volume < 1.001, (volume, source_volume)
    arm = rig(plates, debris_scale)
    rays, core = interior_effects(plates, arm, original)
    bpy.data.objects.remove(original, do_unlink=True)
    scene.frame_set(0)
    bpy.ops.object.select_all(action='DESELECT')
    for obj, _, _, _ in plates:
        rest=obj.data.attributes.new('RestPosition','FLOAT_VECTOR','POINT')
        for vertex in obj.data.vertices:
            rest.data[vertex.index].vector=vertex.co
        obj.select_set(True)
    for obj in rays:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = plates[0][0]
    bpy.ops.object.join()
    geometry = bpy.context.object
    geometry.name = 'MarsScorchedFracture_Geometry'
    geometry.shape_key_add(name='Basis')
    rift = geometry.shape_key_add(name='TectonicRift',from_mix=False)
    for vertex,delta in zip(rift.data,geometry.data.attributes['RiftDelta'].data):
        vertex.co += delta.vector
    for frame,value in RIFT_KEYS:
        rift.value=value
        rift.keyframe_insert('value',frame=frame)
    opening=geometry.shape_key_add(name='RiftOpening',from_mix=False)
    for vertex,delta in zip(opening.data,geometry.data.attributes['OpeningDelta'].data):
        vertex.co+=delta.vector
    for frame,value in OPENING_KEYS:
        opening.value=value
        opening.keyframe_insert('value',frame=frame)
    erosion = geometry.shape_key_add(name='ScorchedRim',from_mix=False)
    erosion_deltas = geometry.data.attributes['ErosionDelta']
    for vertex, delta in zip(erosion.data, erosion_deltas.data):
        vertex.co += delta.vector
    for frame, value in EROSION:
        erosion.value = value
        erosion.keyframe_insert('value', frame=frame)
    common_linear(geometry.data.shape_keys.animation_data.action)
    scene.frame_set(0)
    export_fbx([arm, geometry], 'SK_MarsScorchedFracture.fbx', animated=True)
    # The second asset exists only in the post-blackout scene, never under a shell.
    common.OUT = OUT
    atlas_mat = common.debris_atlas(debris)
    atlas_image = next(n.image for n in atlas_mat.node_tree.nodes if n.type == 'TEX_IMAGE')
    cooled = heat_material('M_MarsDebris_Cooled', atlas_image, cooled=True)
    for obj in debris:
        for vertex in obj.data.vertices:
            vertex.co *= debris_scale
        obj.name = 'Exploration_' + obj.name
        obj.data.materials.clear()
        obj.data.materials.append(cooled)
        field = obj.data.uv_layers.new(name='HeatField')
        for loop in field.data:
            loop.uv = (.4, .9)
        obj.data.uv_layers.active_index = 0
    export_fbx(debris, 'SM_MarsDebris_Exploration.fbx')
    for obj in debris:
        for frame, hidden in ((0, True), (1004, True), (1005, False)):
            obj.hide_render = hidden
            obj.keyframe_insert('hide_render', frame=frame)
            obj.hide_viewport = hidden
            obj.keyframe_insert('hide_viewport', frame=frame)
    for obj in (geometry,):
        for frame, hidden in ((0, False), (1004, False), (1005, True)):
            obj.hide_render = hidden
            obj.keyframe_insert('hide_render', frame=frame)
            obj.hide_viewport = hidden
            obj.keyframe_insert('hide_viewport', frame=frame)
    for frame,hidden in ((0,False),(CORE_HIDE-1,False),(CORE_HIDE,True)):
        core.hide_render=hidden
        core.keyframe_insert('hide_render',frame=frame)
        core.hide_viewport=hidden
        core.keyframe_insert('hide_viewport',frame=frame)
    scene.frame_end = END
    bpy.ops.object.camera_add(location=(0, -7.8, 2.05))
    cine = bpy.context.object
    cine.name = 'Destruction_Camera'
    common.aim(cine)
    cine.data.type, cine.data.ortho_scale = 'ORTHO', 7.4
    scene.camera = cine
    bpy.ops.object.camera_add(location=(.35, -12.4, 2.1))
    ship = bpy.context.object
    ship.name = 'ShipView_Preview'
    common.aim(ship)
    ship.data.type, ship.data.lens = 'PERSP', 48
    for frame, camera in ((0, cine), (1005, ship)):
        marker = scene.timeline_markers.new(camera.name, frame=frame)
        marker.camera = camera
    for name, loc, power, color in [('Key', (-3, -4, 5), 1000, (1, .78, .59)),
                                    ('Fill', (4, -3, 1), 650, (.57, .68, 1)),
                                    ('Rim', (1, 3, 3), 950, (1, .28, .055))]:
        bpy.ops.object.light_add(type='AREA', location=loc)
        light = bpy.context.object
        light.name, light.data.energy, light.data.color = name, power, color
        light.data.shape, light.data.size = 'DISK', 4
        light.data.volume_factor = 0
        common.aim(light)
    scene.world.use_nodes = True
    scene.world.node_tree.nodes.get('Background').inputs['Color'].default_value = (.002, .003, .008, 1)
    scene.world.node_tree.nodes.get('Background').inputs['Strength'].default_value = .2
    scene.render.engine = 'BLENDER_EEVEE'
    scene.eevee.use_volumetric_shadows = True
    scene.eevee.volumetric_shadow_samples = 64
    scene.eevee.volumetric_tile_size = '2'
    scene.eevee.volumetric_samples = 128
    scene.render.resolution_x, scene.render.resolution_y = 1280, 720
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.view_settings.view_transform = 'AgX'
    for frame, label in [(60, 'GROWING_TECTONIC_GORGES'), (150, 'GORGES_TEAR_MAIN_SOLIDS_HOLD'),
                          (240, 'PRESSURE_RELEASE_AND_DRIFT'), (420, 'FIRE_ROLLS_UP_CROSS_SECTIONS'), (900, 'SCORCHED_FRACTURE'),
                          (960, 'FADE_OUT'), (990, 'FULL_BLACK'), (1005, 'SWAP_BEHIND_BLACK'),
                          (1020, 'FADE_BACK_TO_SHIP_VIEW'), (1065, 'EXPLORATION')]:
        scene.timeline_markers.new(label, frame=frame)
    scene.frame_set(0)
    for material in bpy.data.materials:
        if material.use_nodes and material.node_tree.animation_data and material.node_tree.animation_data.action:
            common_linear(material.node_tree.animation_data.action)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'Mars_Scorched_Fracture_V3.blend'))
    motion = json.loads((OUT / 'fragment_motion.json').read_text())
    coverage = json.loads((OUT / 'crack_coverage.json').read_text())
    manifest = dict(version=3, revision=10, geometry_revision=10, fps=FPS, video_fps=60, frames=[0, END], duration_seconds=END / FPS,
                    animation_seconds=ANIM_END / FPS, release_seconds=TEAR / FPS, main_drift_seconds=RELEASE/FPS,
                    rift_keys=RIFT_KEYS, rift_morph='TectonicRift',
                    opening_keys=OPENING_KEYS, opening_morph='RiftOpening',
                    falling_fault_shards=sum(r['fault_shard'] for r in motion),
                    plates=len(plates), bones=len(arm.data.bones), materials=2,
                    triangles=sum(len(p.vertices) - 2 for p in geometry.data.polygons),
                    original_outer_triangles=5004, source_surface_area=source_area,
                    partition_surface_area=exterior_area, source_volume=source_volume,
                    partition_volume=volume, final_original_fragments=107,
                    recovered_inner_fragments=sum(r['recovered_cavity_rock'] for r in motion),
                    interior_fragments=sum(r['layer']=='inner' for r in motion),
                    recovered_inner_volume=sum(r['volume'] for r in motion if r['recovered_cavity_rock']),
                    debris_solid_volume_before_rescale=debris_volume,
                    debris_compacted_volume_equivalent_radius=compacted_radius,
                    debris_to_intact_scale=debris_scale,
                    radial_travel=[min(r['radial_travel'] for r in motion),max(r['radial_travel'] for r in motion)],
                    drift_seconds=[min(r['duration'] for r in motion),max(r['duration'] for r in motion)],
                    drift_continues_until_seconds=ANIM_END/FPS,
                    internal_pressure_keys=PRESSURE, central_light_intensity_scale=220., leakage_keys=LEAKAGE, ray_strength_keys=RAY_STRENGTH,
                    ray_reach_keys=RAY_REACH, edge_glow_keys=EDGE_GLOW, erosion_keys=EROSION,
                    crack_coverage=coverage, local_rim_morph='ScorchedRim',
                    energy_volume_bounds_half_extent=1.12, energy_core_scale_keys=CORE_SCALE,
                    energy_has_visible_surface=False, energy_extinction_mode='fixed soft volume, radiance decay only',
                    thermal_flow_texture='T_MarsThermalFlow.png', burn_progress_keys=BURN_KEYS,
                    thermal_flow='Transient branching hot fingers with layered yellow/orange/red emission, patch-gated secondary branches, sparse cooling reservoirs. No permanent fine emissive web.',
                    energy_core_hide_seconds=CORE_HIDE/FPS, central_light_pocket_depth=.025,
                    central_cavity=False, fracture_ray_pairs=0,
                    fade_out=[32, 33], full_black=[33, 34], swap_seconds=33.5, fade_in=[34, 35.5],
                    render_step=STEP, render_frames_directory=str(FRAMES.relative_to(ROOT)).replace('\\', '/'),
                    cinematic_frames_directory='Saved/MarsDestruction/offline_r10',
                    geometry='Closed partition of the FIRST mesh. Original crust slivers fall inward while TectonicRift deepens gorges and RiftOpening bends and separates irregular lips; all solids release outward together at 8 seconds. Localized rim erosion, source exterior UV and central solid fragments retained.',
                    second_mesh='Original second mesh uniformly rescaled by solid-volume compaction, not by matching exploded bounds. Only visible AFTER the blackout.',
                    heat='Opaque solid rock; transient root-to-tip flame fingers, bright hot cores, orange rolling fronts and sparse dark-red cooling reservoirs. Fixed soft volume emission fades without shrinking.',
                    physical_scope='Authored restrained motion, not a self-gravity simulation.')
    (OUT / 'manifest.json').write_text(json.dumps(manifest, indent=2))
    for obj in (geometry,):
        assert all(abs(s - 1) < 1e-6 for s in obj.scale)
    frames = range(0, END + 1, STEP) if '--render' in sys.argv else (0, 60, 105, 144, 180, 240, 330, 420, 600, 960, 1110)
    for frame in frames:
        scene.frame_set(frame)
        scene.render.filepath = str(FRAMES / f'{frame:04d}.png')
        bpy.ops.render.render(write_still=True)
    print('MARS_SCORCHED_V3', json.dumps(manifest), flush=True)


if __name__ == '__main__':
    main()
