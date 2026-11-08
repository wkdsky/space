"""Reusable lunar impact lithologies and carbonate accretions; run in Blender MCP.

Rock reference: original Kenney rock_crystals GLBs and LPI samples 60025 / 65015.
Carbonate morphology reference:
https://www.nature.com/articles/s41598-020-65667-w (terrestrial biomineralization).
Alien secretion, exposed scale and persistence are fictional art direction.
"""
import bpy
import bmesh
import json
import math
import random
from pathlib import Path
from mathutils import Vector

ROOT = Path('D:/projects/space/SourceArt/MoonResources')
PALETTE = {
    'Regolith': ('#BFC0C4', .96, 0), 'Fresh': ('#C3C5C9', .87, 0),
    'Fracture': ('#93969E', .95, 0), 'Metal': ('#7E878E', .54, .48),
    'Crystal': ('#9ABFB8', .30, .06), 'CarbonCrust': ('#AAB49F', .92, 0),
    'CarbonNodule': ('#B9C3AC', .68, 0), 'CarbonCore': ('#94A28C', .79, 0),
    'CarbonMargin': ('#AAB49F', .98, 0),
}
MATERIAL_CACHE = {}

def mat(role):
    if role in MATERIAL_CACHE:return MATERIAL_CACHE[role]
    name = 'M_MR_' + role
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    m.node_tree.nodes.clear()
    p=m.node_tree.nodes.new('ShaderNodeBsdfPrincipled')
    output=m.node_tree.nodes.new('ShaderNodeOutputMaterial')
    m.node_tree.links.new(p.outputs['BSDF'],output.inputs['Surface'])
    MATERIAL_CACHE[role]=m
    h, rough, metal = PALETTE[role]
    c = [int(h[i:i+2], 16)/255 for i in (1,3,5)]
    c = [v/12.92 if v <= .04045 else ((v+.055)/1.055)**2.4 for v in c]
    p.inputs['Base Color'].default_value = (*c, 1)
    p.inputs['Roughness'].default_value = rough
    p.inputs['Metallic'].default_value = metal
    if role=='CarbonMargin':
        uv=m.node_tree.nodes.new('ShaderNodeTexCoord')
        separate=m.node_tree.nodes.new('ShaderNodeSeparateXYZ')
        blend=m.node_tree.nodes.new('ShaderNodeMixRGB');blend.blend_type='MIX'
        blend.inputs[1].default_value=mat('Regolith').diffuse_color
        blend.inputs[2].default_value=(*c,1)
        m.node_tree.links.new(uv.outputs['UV'],separate.inputs[0])
        m.node_tree.links.new(separate.outputs['Y'],blend.inputs[0])
        m.node_tree.links.new(blend.outputs[0],p.inputs['Base Color'])
    m.diffuse_color = (*c, 1)
    return m

def hull(name, points, role):
    bm = bmesh.new()
    for p in points: bm.verts.new(p)
    bmesh.ops.convex_hull(bm, input=list(bm.verts))
    loose = [v for v in bm.verts if not v.link_faces]
    if loose: bmesh.ops.delete(bm, geom=loose, context='VERTS')
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    mesh = bpy.data.meshes.new(name); bm.to_mesh(mesh); bm.free()
    o = bpy.data.objects.new(name, mesh); bpy.context.scene.collection.objects.link(o)
    o.data.materials.append(mat(role))
    return o

def body(name, seed, family, scale=1, offset=(0,0,0), role='Regolith'):
    r = random.Random(seed)
    # Continuum of tabular ejecta, jointed blocks, inclined wedges and softened breccias.
    shapes = [(1,.76,.25),(.79,.73,.64),(1,.61,.46),(.86,.76,.48),(.98,.45,.32),(.72,.85,.75)]
    sx,sy,sz = shapes[family%len(shapes)]
    lean = r.uniform(-.23,.23); phase = r.uniform(0,6.28)
    points=[]
    for ring,(z,rad) in enumerate(((-.05,.82),(.12,1),(.59,.84),(1,.40))):
        for i in range(11):
            a = phase + i*2*math.pi/11 + r.uniform(-.075,.075)
            rad2 = rad*r.uniform(.86,1.14)
            x = math.cos(a)*sx*rad2 + lean*z
            y = math.sin(a)*sy*rad2 + math.sin(phase)*z*.10
            height = z*sz*(1+.18*math.sin(a+phase))
            if family%6 == 2: height *= .73+.32*(x/sx+1)
            points.append(Vector((x*scale,y*scale,height*scale))+Vector(offset))
    o=hull(name, points, role)
    if role=='Regolith':
        o.data.materials.append(mat('Fresh'));o.data.materials.append(mat('Fracture'))
        for p in o.data.polygons:
            p.material_index = 1 if p.normal.x > .53 and p.normal.z < .82 else (2 if p.normal.z < -.1 else 0)
    return o

def join(parts, name):
    bpy.ops.object.select_all(action='DESELECT')
    for p in parts: p.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]
    if len(parts)>1:bpy.ops.object.join()
    o=parts[0];o.name=name
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    # Stable UVs for shared fine regolith texture, no randomized flat-face rainbow.
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=1.15,island_margin=.02)
    bpy.ops.object.mode_set(mode='OBJECT')
    o.data.uv_layers.active.name='UVMap'
    return o

def rock(name, seed, family, size, ore):
    r=random.Random(seed);root=body(name,seed,family);parts=[root]
    # Paired detachable chips share the host lithology; the footprint blends into smaller sizes.
    if family in (3,5):
        for i in range(2 if family==3 else 3):
            a=r.uniform(0,6.28);d=r.uniform(.65,.98)
            parts.append(body(name+'_chip',seed+i+190, (family+i+1)%6,r.uniform(.12,.27),(math.cos(a)*d,math.sin(a)*d,.01)))
    if ore:
        # Reference: the original Kenney rock_crystals / LargeA / LargeB GLBs.
        # Separate solid inclusions, buried roots and exposed angular terminations.
        facets=[p for p in root.data.polygons if p.normal.z>-.05 and p.center.z>.03]
        centers=[]
        for i in range(10 if ore=='Crystal' else 8):
            for attempt in range(60):
                p=r.choices(facets,weights=[p.area for p in facets])[0]
                pts=[root.data.vertices[j].co for j in p.vertices]
                a,b=r.random(),r.random()
                if a+b>1:a,b=1-a,1-b
                center=pts[0]+a*(pts[1]-pts[0])+b*(pts[2]-pts[0])
                radius=r.uniform(.065,.105)
                if all((center-c).length>radius*2.3 for c in centers):break
            else:continue
            centers.append(center)
            axis=(p.normal*.85+Vector((r.uniform(-.18,.18),r.uniform(-.18,.18),.28))).normalized()
            orient=Vector((0,0,1)).rotation_difference(axis)
            sides=4 if ore=='Crystal' else 5;phase=r.uniform(0,6.28)
            height=radius*r.uniform(1.1,1.9);points=[]
            for z,rad in ((-height*.55,radius),(.50*height,radius),(.85*height,radius*.52)):
                for j in range(sides):
                    angle=phase+j*2*math.pi/sides
                    points.append(center+orient@Vector((math.cos(angle)*rad,math.sin(angle)*rad,z)))
            parts.append(hull(name+'_embedded_'+ore,points,ore))
    o=join(parts,name)
    widths={'Small':.35,'Medium':1.15,'Large':2.65}
    factor=widths[size]/max(o.dimensions.x,o.dimensions.y)
    for v in o.data.vertices:v.co*=factor
    return o

def nodule(name,r,center,radius,role):
    points=[]
    for j in range(1,4):
        lat=math.pi*j/4
        for i in range(6):
            a=i*2*math.pi/6
            rr=radius*r.uniform(.88,1.12)
            points.append(Vector(center)+Vector((math.cos(a)*math.sin(lat)*rr,math.sin(a)*math.sin(lat)*rr,math.cos(lat)*rr*.62)))
    points += [Vector(center)+Vector((0,0,radius*.64)),Vector(center)-Vector((0,0,radius*.60))]
    o=hull(name,points,role)
    for p in o.data.polygons:p.use_smooth=True
    return o

def carbon(name,seed,variant):
    r=random.Random(seed)
    # One identity: a low, coalesced granular carbonate accretion. Only its footprint varies.
    # Sub-mm margin becomes the surrounding regolith, without a raised disk or separate stages.
    length=(.65,.75,.59)[variant];width=(.38,.32,.43)[variant];phase=r.uniform(0,6.28)
    def perimeter(a):return 1+.12*math.sin(3*a+phase)+.09*math.sin(7*a-phase)+.035*math.sin(13*a)
    count=72;rings=12;verts=[(0,0,.018)];coverage=[1]
    for ring in range(1,rings+1):
        t=ring/rings
        for i in range(count):
            a=i*2*math.pi/count;rad=perimeter(a)*t
            z=.0003+.018*(1-t*t)**2*(1+.18*math.sin(8*a+t*9))
            verts.append((math.cos(a)*length*rad,math.sin(a)*width*rad,z))
            coverage.append(max(0,min(1,(1-t)/.43)))
    faces=[]
    for i in range(count):faces.append((0,1+i,1+(i+1)%count))
    for ring in range(rings-1):
        for i in range(count):
            a=1+ring*count+i;b=1+ring*count+(i+1)%count
            faces.append((a,a+count,b+count,b))
    data=bpy.data.meshes.new(name+'_feathered_margin');data.from_pydata(verts,[],faces);data.materials.append(mat('CarbonMargin'))
    rind=bpy.data.objects.new(name+'_margin',data);bpy.context.scene.collection.objects.link(rind)
    for p in data.polygons:p.use_smooth=True
    uv=data.uv_layers.new(name='UVMap')
    for loop in data.loops:uv.data[loop.index].uv=(data.vertices[loop.vertex_index].co.x/length*.5+.5,coverage[loop.vertex_index])
    # Keep this custom UV0: its Y channel is the substrate-to-carbonate coverage mask.
    parts=[]
    for i in range(155):
        a=r.uniform(0,6.28);t=math.sqrt(r.random())*.92;rad=perimeter(a)*t
        x=math.cos(a)*length*rad;y=math.sin(a)*width*rad
        edge=max(0,min(1,(1-t)/.4));radius=r.uniform(.018,.047)*(.28+.72*edge)
        z=.002+.016*(1-t*t)**2+radius*.12
        role='CarbonNodule' if i%7 else 'CarbonCore'
        if t>.78:role='Regolith' if i%2 else 'CarbonCrust'
        parts.append(nodule(name+'_grain',r,(x,y,z),radius,role))
    grains=join(parts,name+'_coalesced_grains')
    # Joining keeps the explicit mask UVs and normal UVs on their respective material sections.
    bpy.ops.object.select_all(action='DESELECT');rind.select_set(True);grains.select_set(True)
    bpy.context.view_layer.objects.active=rind;bpy.ops.object.join();rind.name=name
    return rind

def export(o):
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    path=ROOT/'FBX'/(o.name+'.fbx');path.parent.mkdir(parents=True,exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},
        apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',
        use_mesh_modifiers=True,mesh_smooth_type='FACE',bake_anim=False)
    return dict(name=o.name,triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials],dimensions=list(o.dimensions))

def loose_mineral(name, seed, role, variant):
    r=random.Random(seed)
    points=[]
    sides=5 if role=='Metal' else 6
    phase=r.uniform(0,math.tau)
    width=.12+r.uniform(0,.035)
    height=width*(1.1 if role=='Metal' else 1.55)
    for z,radius in ((-height*.5,.48),(-height*.23,1),(height*.29,.88),(height*.61,.34)):
        for i in range(sides):
            a=phase+i*math.tau/sides
            rr=width*radius*r.uniform(.84,1.12)
            points.append(Vector((math.cos(a)*rr,math.sin(a)*rr*(.75+.035*variant),z*r.uniform(.9,1.1))))
    # The entire connected fragment is mineral: there is no rock matrix or soil base.
    return join([hull(name,points,role)],name)


def build():
    ROOT.mkdir(parents=True,exist_ok=True)
    scene=bpy.data.scenes.get('JTS_MoonResources') or bpy.data.scenes.new('JTS_MoonResources')
    bpy.context.window.scene=scene
    for o in list(scene.objects):bpy.data.objects.remove(o,do_unlink=True)
    scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
    rows=[]
    for k,ore in enumerate(('', 'Metal','Crystal')):
        for j,size in enumerate(('Small','Medium','Large')):
            if ore and size=='Small':continue
            for family in range(6):
                name=f'SM_MR_{ore or "Rock"}_{size}_{family+1:02d}'
                o=rock(name,7140+family*39+j*7,family,size,ore);rows.append(export(o))
                o.location=(family*3.5,k*9+j*3,0)
        if ore:
            for variant in range(6):
                name=f'SM_MR_{ore}_Loose_{variant+1:02d}'
                o=loose_mineral(name,8430+k*100+variant,ore,variant);rows.append(export(o))
                o.location=(variant*1.5,k*9,0)
    for variant in range(3):
        o=carbon(f'SM_MR_Carbon_Accretion_{variant+1:02d}',9800+variant*7,variant)
        rows.append(export(o));o.location=(variant*3.5,29,0)
    (ROOT/'manifest.json').write_text(json.dumps(dict(palette=PALETTE,assets=rows),indent=2),encoding='utf-8')
    # Deliver only this scene and its dependencies, without other open projects or QA copies.
    bpy.data.libraries.write(str(ROOT/'MoonResources.blend'),{scene},path_remap='RELATIVE_ALL',compress=True)
    print(json.dumps(dict(meshes=len(rows),max_triangles=max(r['triangles'] for r in rows))))

build()
