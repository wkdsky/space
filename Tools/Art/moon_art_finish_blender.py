"""Live Blender MCP authoring: mineral crusts, restrained shell textures and UVs."""
import bpy
import bmesh
import json
import math
import random
from pathlib import Path
from mathutils import Matrix

ROOT=Path('D:/projects/space')
ART=ROOT/'SourceArt/MoonSurfaceV3'
TEX=ART/'Textures'
KINDS=('MirrorSkimmer','Riftlip','SpireWorker','SpireSoldier')
COLORS={
    'MirrorSkimmer':dict(Shell='#8FAEB9',Crown='#C7C4A5',Wear='#637A83',Joint='#34414A'),
    'Riftlip':dict(Shell='#8D967A',Wear='#69745F',Joint='#3C4645',Seal='#423844',LipRim='#B29A65'),
    'SpireWorker':dict(Shell='#A18B66',Wear='#695E4B',Joint='#424642'),
    'SpireSoldier':dict(Shell='#788A98',Crown='#A5AFB4',Wear='#62655D',Joint='#343F49')}
ENV=dict(Moon='#929AA5',Lip='#B2A991',Void='#383B46',Tower='#97866B',TowerLight='#B3A285',Feed='#9B8E6A',FeedLight='#C5BA96')

def linear(h):
    values=[int(h[i:i+2],16)/255 for i in (1,3,5)]
    return tuple(v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in values)

def selected(obj):
    if bpy.context.object and bpy.context.object.mode!='OBJECT':bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.select_all(action='DESELECT')
    obj.hide_set(False);obj.select_set(True);bpy.context.view_layer.objects.active=obj

def planar_uv(obj):
    uv=obj.data.uv_layers.get('UVMap') or obj.data.uv_layers.new(name='UVMap')
    for poly in obj.data.polygons:
        n=poly.normal
        for i in poly.loop_indices:
            p=obj.data.vertices[obj.data.loops[i].vertex_index].co
            if abs(n.z)>.6:uv.data[i].uv=(p.x*.38+.5,p.y*.38+.5)
            else:
                a=math.atan2(n.y,n.x)
                uv.data[i].uv=((-p.x*math.sin(a)+p.y*math.cos(a))*.38+.5,p.z*.38+.5)

def crusts():
    import moon_surface_geometry as g
    bpy.context.window.scene=bpy.data.scenes[g.SCENE]
    rows=[]
    for variant in range(4):
        name=f'SM_MS3_PortableFeed_{variant}'
        old=bpy.data.objects.get(name)
        if old:bpy.data.objects.remove(old,do_unlink=True)
        rng=random.Random(9030+variant);v=[];f=[];mi=[]
        centers=[(-.48,.08,.40),(.18,.31,.33),(.46,-.25,.31)]
        if variant%2:centers.append((-.12,-.43,.21))
        for cx,cy,size in centers:
            n=rng.randint(6,8);start=len(v);v.append((cx,cy,0))
            ring=[]
            for i in range(n):
                a=math.tau*i/n;r=size*rng.uniform(.67,1.2)
                ring.append((cx+math.cos(a)*r,cy+math.sin(a)*r*.75,0))
            v+=ring
            for i in range(n):f.append((start,start+i+1,start+(i+1)%n+1));mi.append(0)
            # Broken salt rims only on selected edges, with real bare gaps between lobes.
            for i in range(n):
                if rng.random()>.55:continue
                p=ring[i];q=ring[(i+1)%n];k=len(v)
                v += [p,q,(cx+(q[0]-cx)*1.10,cy+(q[1]-cy)*1.10,0),(cx+(p[0]-cx)*1.10,cy+(p[1]-cy)*1.10,0)]
                f.append((k+3,k+2,k+1,k));mi.append(1)
        obj=g.mesh_object(name,v,f,('Feed','FeedLight'))
        for face,slot in zip(obj.data.polygons,mi):face.material_index=slot
        bm=bmesh.new();bm.from_mesh(obj.data)
        bmesh.ops.subdivide_edges(bm,edges=list(bm.edges),cuts=3,use_grid_fill=True)
        bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free();obj.data.update()
        planar_uv(obj);rows.append(g.export(obj))
    (ART/'portable_modules.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
    return rows

def environment():
    import importlib,moon_surface_geometry as g
    importlib.reload(g)
    bpy.context.window.scene=bpy.data.scenes[g.SCENE]
    # Rebuild only independent cave meshes, never the Moon reference.
    names=[]
    for i in range(4):
        for mirror,prefix,seed in ((True,'MirrorRefuge',400),(False,'RiftRefuge',500)):
            name=f'SM_MS3_{prefix}_{i}';old=bpy.data.objects.get(name)
            if old:bpy.data.objects.remove(old,do_unlink=True)
            g.refuge(name,mirror,seed+i);names.append(name)
    for i in range(9):
        name=f'SM_MS3_FixedNest_{i+1:02d}';old=bpy.data.objects.get(name)
        if old:bpy.data.objects.remove(old,do_unlink=True)
        g.refuge(name,True,1600+i);names.append(name)
    names += [f'SM_MS3_Tower_{i}_{stage}' for i in range(4) for stage in range(3)]
    names += [f'SM_MS3_Rock_{i}' for i in range(3)]
    for name in names:
        obj=bpy.data.objects[name];planar_uv(obj);g.export(obj)
    rows=crusts()
    (ART/'finish_assets.json').write_text(json.dumps(names+[r['name'] for r in rows],indent=2),encoding='utf-8')
    # Hide obsolete line markers in the source preview as well as removing them from gameplay recipes.
    for obj in bpy.context.scene.objects:
        if 'Crack_' in obj.name or obj.name.startswith('PREVIEW_Crack'):
            obj.hide_set(True);obj.hide_render=True
    return dict(meshes=len(names)+len(rows),free_standing_cracks=0)


def rebuild_refuges():
    """Rebuild only the four fissures and nine fixed MirrorSkimmer mouths."""
    import importlib,moon_surface_geometry as g
    importlib.reload(g)
    bpy.context.window.scene=bpy.data.scenes[g.SCENE]
    names=[(f'SM_MS3_RiftRefuge_{i}',False,500+i) for i in range(4)]
    names += [(f'SM_MS3_FixedNest_{i+1:02d}',True,1600+i) for i in range(9)]
    exported=[]
    for name,mirror,seed in names:
        old=bpy.data.objects.get(name)
        obj=g.refuge(name+'_Rebuild',mirror,seed)
        planar_uv(obj)
        for slot in obj.material_slots:
            if slot.material and slot.material.name=='M_MS3_Moon':
                slot.material=bpy.data.materials.get('M_MS3_PropStone') or slot.material
        if old:
            previous=old.data;old.data=obj.data
            # Keep linked source previews consistent with the asset being exported.
            for preview in bpy.data.objects:
                if preview.type=='MESH' and preview.data==previous:preview.data=old.data
            bpy.data.objects.remove(obj,do_unlink=True);obj=old
        else:obj.name=name
        obj.location=(0,0,0);obj.rotation_euler=(0,0,0);obj.scale=(1,1,1)
        exported.append(g.export(obj))
        obj.hide_set(True);obj.hide_render=True
    recipe=json.loads((ART/'surface_recipe.json').read_text(encoding='utf-8'))
    for row in recipe['fixed_nests']:row['species']='MirrorSkimmer'
    replacements={r['name']:r for r in exported}
    recipe['assets']=[replacements.get(r['name'],r) for r in recipe['assets']]
    (ART/'surface_recipe.json').write_text(json.dumps(recipe,indent=2),encoding='utf-8')
    bpy.ops.wm.save_as_mainfile(filepath=str(ART/'MoonSurfaceV3.blend'),compress=True)
    return dict(meshes=exported,fixed_mirror_mouths=9,rift_cave_mouths=0)

def mineral_textures():
    import numpy as np
    TEX.mkdir(parents=True,exist_ok=True)
    n=1024;y,x=np.mgrid[0:n,0:n]/n;rng=np.random.default_rng(22917)
    field=np.zeros((n,n))
    for freq,amp in ((4,.32),(12,.14),(40,.06),(160,.015)):
        grid=rng.uniform(-1,1,(freq,freq));gx=x*freq;gy=y*freq
        ix=gx.astype(int);iy=gy.astype(int);fx=gx-ix;fy=gy-iy
        fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy)
        a=grid[iy%freq,ix%freq]*(1-fx)+grid[iy%freq,(ix+1)%freq]*fx
        b=grid[(iy+1)%freq,ix%freq]*(1-fx)+grid[(iy+1)%freq,(ix+1)%freq]*fx
        field+=amp*(a*(1-fy)+b*fy)
    grain=np.clip(.52+field+rng.normal(0,.028,(n,n)),0,1)
    strata=np.clip(.5+.18*np.sin(y*math.tau*11+field*8)+field*.4,0,1)
    rough=np.clip(.76+field*.36+rng.normal(0,.018,(n,n)),.5,.94)
    channels=np.stack([grain,strata,rough,np.ones((n,n))],axis=-1)
    def write(name,pixels):
        im=bpy.data.images.get(name) or bpy.data.images.new(name,width=n,height=n,alpha=True)
        im.colorspace_settings.name='Non-Color';im.pixels.foreach_set(pixels.astype('float32').ravel())
        im.filepath_raw=str(TEX/(name+'.png'));im.file_format='PNG';im.save();return im
    detail=write('T_MoonMineral_Detail',channels)
    dy,dx=np.gradient(field)
    normal=np.stack([-dx*2,-dy*2,np.ones((n,n))],axis=-1)
    normal/=np.linalg.norm(normal,axis=-1,keepdims=True)
    normal=write('T_MoonMineral_Normal',np.concatenate([normal*.5+.5,np.ones((n,n,1))],axis=-1))
    for role,h in ENV.items():
        # This new material is only for independent props. M_MS3_Moon remains untouched.
        name='M_MS3_PropStone' if role=='Moon' else 'M_MS3_'+role
        m=bpy.data.materials.get(name) or bpy.data.materials.new(name);m.use_nodes=True
        nodes=m.node_tree.nodes;links=m.node_tree.links;bsdf=next(n for n in nodes if n.type=='BSDF_PRINCIPLED')
        color=linear(h);bsdf.inputs['Base Color'].default_value=(*color,1);bsdf.inputs['Roughness'].default_value=.8
        m.diffuse_color=(*color,1)
        tex=nodes.new('ShaderNodeTexImage');tex.image=detail
        ramp=nodes.new('ShaderNodeValToRGB')
        ramp.color_ramp.elements[0].color=(*(v*.70 for v in color),1)
        ramp.color_ramp.elements[1].color=(*(min(1,v*1.16) for v in color),1)
        links.new(tex.outputs['Color'],ramp.inputs[0]);links.new(ramp.outputs[0],bsdf.inputs['Base Color'])
        if role=='Moon':
            for name in json.loads((ART/'finish_assets.json').read_text()):
                obj=bpy.data.objects[name]
                for slot in obj.material_slots:
                    if slot.material and slot.material.name=='M_MS3_Moon':slot.material=m
    return dict(textures=[detail.name,normal.name])

def creature(kind):
    import moon_creatures_delivery as d
    scene,rig=d.show(kind);obj=bpy.data.objects['SK_'+kind]
    selected(obj);bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.025)
    bpy.ops.object.mode_set(mode='OBJECT')
    scene.render.engine='CYCLES';scene.cycles.samples=1;scene.cycles.device='CPU'
    scene.render.bake.margin=10;scene.render.bake.use_clear=True
    TEX.mkdir(parents=True,exist_ok=True)
    images={}
    for suffix in ('BaseColor','Roughness'):
        name=f'T_{kind}_{suffix}'
        im=bpy.data.images.get(name) or bpy.data.images.new(name,width=1024,height=1024,alpha=False)
        im.colorspace_settings.name='sRGB' if suffix=='BaseColor' else 'Non-Color'
        images[suffix]=im
    materials=[]
    for mat in obj.data.materials:
        role=mat.name.split('_')[-1];color=linear(COLORS[kind][role])
        nodes=mat.node_tree.nodes;links=mat.node_tree.links;nodes.clear()
        out=nodes.new('ShaderNodeOutputMaterial');bsdf=nodes.new('ShaderNodeBsdfPrincipled')
        bsdf.inputs['Base Color'].default_value=(*color,1)
        rough=.37 if kind=='MirrorSkimmer' and role in ('Shell','Crown') else .73
        bsdf.inputs['Roughness'].default_value=rough
        bsdf.inputs['Metallic'].default_value=0
        coord=nodes.new('ShaderNodeTexCoord');noise=nodes.new('ShaderNodeTexNoise')
        noise.inputs['Scale'].default_value=15 if kind=='MirrorSkimmer' else 22
        noise.inputs['Detail'].default_value=2
        links.new(coord.outputs['Generated'],noise.inputs['Vector'])
        ramp=nodes.new('ShaderNodeValToRGB')
        ramp.color_ramp.elements[0].position=.18;ramp.color_ramp.elements[1].position=.82
        ramp.color_ramp.elements[0].color=(*(v*.70 for v in color),1)
        ramp.color_ramp.elements[1].color=(*(min(1,v*1.16) for v in color),1)
        links.new(noise.outputs['Fac'],ramp.inputs[0])
        roughnode=nodes.new('ShaderNodeMapRange');roughnode.inputs['To Min'].default_value=max(.2,rough-.08)
        roughnode.inputs['To Max'].default_value=min(.95,rough+.10)
        links.new(noise.outputs['Fac'],roughnode.inputs['Value'])
        emit=nodes.new('ShaderNodeEmission');links.new(emit.outputs[0],out.inputs['Surface'])
        target=nodes.new('ShaderNodeTexImage');nodes.active=target
        materials.append((mat,bsdf,out,emit,target,ramp,roughnode))
        mat.diffuse_color=(*color,1)
    for suffix in ('BaseColor','Roughness'):
        for mat,bsdf,out,emit,target,ramp,roughnode in materials:
            target.image=images[suffix];mat.node_tree.nodes.active=target
            mat.node_tree.links.new(ramp.outputs[0] if suffix=='BaseColor' else roughnode.outputs[0],emit.inputs['Color'])
        bpy.ops.object.bake(type='EMIT')
        im=images[suffix];im.filepath_raw=str(TEX/(im.name+'.png'));im.file_format='PNG';im.save()
    for mat,bsdf,out,emit,target,ramp,roughnode in materials:
        links=mat.node_tree.links;nodes=mat.node_tree.nodes
        target.image=images['BaseColor'];rt=nodes.new('ShaderNodeTexImage');rt.image=images['Roughness']
        links.new(target.outputs['Color'],bsdf.inputs['Base Color']);links.new(rt.outputs['Color'],bsdf.inputs['Roughness'])
        links.new(bsdf.outputs[0],out.inputs['Surface'])
    return dict(species=kind,uv_layers=len(obj.data.uv_layers),textures=[i.name for i in images.values()],bones=len(rig.data.bones))

def export_creature_mesh(kind):
    import moon_creatures_delivery as d
    scene,source=d.show(kind);original=bpy.data.objects['SK_'+kind]
    rig=source.copy();rig.data=source.data.copy();rig.animation_data_clear()
    mesh=original.copy();mesh.data=original.data.copy();scene.collection.objects.link(rig);scene.collection.objects.link(mesh)
    units=scene.unit_settings.scale_length
    try:
        rig.name='Armature';rig.data.transform(Matrix.Scale(100,4));mesh.data.transform(Matrix.Scale(100,4))
        if mesh.parent==source:mesh.parent=rig
        for mod in mesh.modifiers:
            if mod.type=='ARMATURE':mod.object=rig
        for bone in rig.pose.bones:bone.location=(0,0,0);bone.rotation_quaternion=(1,0,0,0);bone.scale=(1,1,1)
        scene.unit_settings.scale_length=.01;selected(rig);mesh.select_set(True)
        path=ROOT/'SourceArt/MoonCreaturesV3'/kind/('SK_'+kind+'.fbx')
        bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH','ARMATURE'},
            add_leaf_bones=False,use_armature_deform_only=True,axis_forward='-Y',axis_up='Z',global_scale=1,
            apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',use_mesh_modifiers=True,mesh_smooth_type='FACE',bake_anim=False)
    finally:
        scene.unit_settings.scale_length=units;md=mesh.data;rd=rig.data
        bpy.data.objects.remove(mesh,do_unlink=True);bpy.data.objects.remove(rig,do_unlink=True)
        bpy.data.meshes.remove(md);bpy.data.armatures.remove(rd)
    return str(path)

def save():
    import moon_creatures_delivery as d
    metadata=dict(species=COLORS,environment=ENV,texture_folder=str(TEX))
    (ART/'finish_palette.json').write_text(json.dumps(metadata,indent=2),encoding='utf-8')
    recipe=json.loads((ART/'surface_recipe.json').read_text(encoding='utf-8'))
    for role,color in ENV.items():
        if role!='Moon':recipe['palette'][role]=color
    (ART/'surface_recipe.json').write_text(json.dumps(recipe,indent=2),encoding='utf-8')
    source=ROOT/'SourceArt/MoonCreaturesV3/materials.json'
    mats=json.loads(source.read_text(encoding='utf-8'))
    for kind,rows in mats.items():
        for row in rows:row['color']=[*linear(COLORS[kind][row['name'].split('_')[-1]]),1]
    source.write_text(json.dumps(mats,indent=2),encoding='utf-8')
    d.save_source()
    bpy.ops.wm.save_as_mainfile(filepath=str(ART/'MoonSurfaceV3.blend'),compress=True)
    return metadata
