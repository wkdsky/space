"""Original white-hot beam head and procedural red heated-atmosphere artwork."""
import bpy
import math
import json
import numpy as np
from pathlib import Path

OUT=Path('D:/projects/space/SourceArt/FrontEnd')
collection=bpy.data.collections.get('FE_AlienThreat')
if not collection:
    collection=bpy.data.collections.new('FE_AlienThreat');bpy.context.scene.collection.children.link(collection)
for obj in list(collection.objects):bpy.data.objects.remove(obj,do_unlink=True)
assets=[]
def move(obj):
    for c in list(obj.users_collection):c.objects.unlink(obj)
    collection.objects.link(obj);return obj
def cylinder(radius,depth,z):
    bpy.ops.mesh.primitive_cylinder_add(vertices=64,radius=radius,depth=depth,location=(0,0,z))
    return move(bpy.context.object)
def cap(radius,height):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=64,ring_count=24,location=(0,0,height))
    o=move(bpy.context.object);o.scale=(radius,radius,height);return o
def export(name,objects):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.hide_set(False);o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0];bpy.ops.object.join()
    o=bpy.context.object;o.name=name;bpy.context.scene.cursor.location=(0,0,0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    for face in o.data.polygons:face.use_smooth=True
    bpy.ops.export_scene.fbx(filepath=str(OUT/'Models'/(name+'.fbx')),use_selection=True,object_types={'MESH'},bake_anim=False,axis_forward='-Y',axis_up='Z')
    o.data.calc_loop_triangles()
    assets.append({'id':name,'file':'Models/'+name+'.fbx','source':'original_blender',
                   'triangles':len(o.data.loop_triangles),'purpose':'Rounded white-hot alien beam front suspended above Earth; original geometry'})
    o.hide_set(True);o.hide_render=True
export('SM_FE_ThreatCore',[cap(.46,.66),cylinder(.46,60,30.66)])
export('SM_FE_ThreatHalo',[cap(.95,1.1),cylinder(.95,60,31.1)])
arcs=[]
for j,(radius,z) in enumerate([(1.2,.7),(1.7,1.65)]):
    curve=bpy.data.curves.new('PlasmaArc','CURVE');curve.dimensions='3D';curve.bevel_depth=.025;curve.bevel_resolution=3
    spline=curve.splines.new('POLY');spline.points.add(127)
    for i,p in enumerate(spline.points):
        a=i/127*math.tau;r=radius*(1+.045*math.sin(a*5+j))
        p.co=(r*math.cos(a),r*.83*math.sin(a),z+.13*math.sin(a*3+j),1)
    spline.use_cyclic_u=True
    obj=bpy.data.objects.new('PlasmaArc',curve);collection.objects.link(obj)
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    bpy.ops.object.convert(target='MESH');arcs.append(bpy.context.object)
export('SM_FE_ThreatFrontRings',arcs)

size=1024
y,x=np.mgrid[-1:1:complex(size),-1:1:complex(size)]
rng=np.random.default_rng(271027)
def noise(frequency):
    grid=rng.random((frequency+2,frequency+2))
    u=(x+1)*.5*frequency;v=(y+1)*.5*frequency
    ix=np.floor(u).astype(int);iy=np.floor(v).astype(int)
    sx=u-ix;sy=v-iy;sx=sx*sx*(3-2*sx);sy=sy*sy*(3-2*sy)
    return ((grid[iy,ix]*(1-sx)+grid[iy,ix+1]*sx)*(1-sy)+
            (grid[iy+1,ix]*(1-sx)+grid[iy+1,ix+1]*sx)*sy)
n=.5*noise(5)+.27*noise(12)+.15*noise(29)+.08*noise(67)
radius=np.sqrt((x/(.88+.09*n))**2+(y/(.57+.12*n))**2)
edge=np.clip((1-radius)/.22,0,1);edge=edge*edge*(3-2*edge)
temperature=np.exp(-4.5*radius**2)
alpha=edge*np.exp(-2.5*radius**2)*(.28+.72*n)
alpha*=1-np.exp(-35*radius**2)*.28
rgba=np.zeros((size,size,4),dtype=np.float32)
rgba[:,:,0]=1;rgba[:,:,1]=.018+.30*temperature**2;rgba[:,:,2]=.003+.05*temperature**3
rgba[:,:,3]=alpha
image=bpy.data.images.get('T_FE_ThreatHeatCloud') or bpy.data.images.new('T_FE_ThreatHeatCloud',width=size,height=size,alpha=True)
image.pixels.foreach_set(rgba.ravel());image.file_format='PNG'
image.filepath_raw=str(OUT/'Textures/T_FE_ThreatHeatCloud.png');image.save()
assets.append({'id':'T_FE_ThreatHeatCloud','file':'Textures/T_FE_ThreatHeatCloud.png','source':'original_blender_procedural',
               'size':[size,size],'recipe':'Seeded multiscale value noise, radial atmospheric scattering, white-orange center and red outer corona; no sampled artwork'})
bpy.data.libraries.write(str(OUT/'FrontEnd_Threat.blend'),{collection,image},fake_user=True)
(OUT/'threat_manifest.json').write_text(json.dumps(assets,indent=2),encoding='utf-8')
result={'original_meshes':3,'original_heat_cloud':1,'source':'FrontEnd_Threat.blend'}
