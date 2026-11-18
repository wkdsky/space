"""Bake the Editor's real-surface samples into independent flexible film sections."""
import bpy
import json
from pathlib import Path
from mathutils import Vector

ROOT=Path('D:/projects/space')
OUT=ROOT/'SourceArt/MarsStation/Refined/ProjectedFilm'
OUT.mkdir(parents=True,exist_ok=True)
name='MarsStationProjectedFilm'
old=bpy.data.collections.get(name)
if old:
    for o in list(old.objects):bpy.data.objects.remove(o,do_unlink=True)
    bpy.data.collections.remove(old)
collection=bpy.data.collections.new(name);bpy.context.scene.collection.children.link(collection)
materials=[]
for slot,color in (('SolarDust',(.19,.13,.1,1)),('Orange',(.9,.25,.045,1)),('Frame',(.76,.78,.72,1))):
    material=bpy.data.materials.get('MP_'+slot) or bpy.data.materials.new('MP_'+slot)
    material.use_nodes=True;shader=next(n for n in material.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    shader.inputs['Base Color'].default_value=color;shader.inputs['Roughness'].default_value=.92;material.diffuse_color=color
    materials.append(material)
manifest={}
for section in json.loads((ROOT/'Saved/MarsStationFilmProjection.json').read_text(encoding='utf8')):
    vertices=[];faces=[];slots=[];samples=section['samples']
    def vertex(x,y,z):
        sample=samples[str(float(x))+','+str(float(y))]
        return (Vector(sample['point'])+Vector(sample['up'])*(3+z))/100
    def volume(xs,ys,bottom,top,slot):
        start=len(vertices);nx=len(xs);ny=len(ys)
        vertices.extend(vertex(x,y,z) for z in (bottom,top) for y in ys for x in xs)
        index=lambda x,y,k:start+k*nx*ny+y*nx+x
        added=[]
        for y in range(ny-1):
            for x in range(nx-1):
                added += [(index(x,y,0),index(x,y+1,0),index(x+1,y+1,0),index(x+1,y,0)),
                          (index(x,y,1),index(x+1,y,1),index(x+1,y+1,1),index(x,y+1,1))]
        for y in range(ny-1):
            for x in (0,nx-1):
                face=(index(x,y,0),index(x,y,1),index(x,y+1,1),index(x,y+1,0))
                added.append(face if x==0 else tuple(reversed(face)))
        for x in range(nx-1):
            for y in (0,ny-1):
                face=(index(x,y,0),index(x+1,y,0),index(x+1,y,1),index(x,y,1))
                added.append(face if y==0 else tuple(reversed(face)))
        faces.extend(added);slots.extend([slot]*len(added))
        return vertices[start:],added,start
    ys=list(range(-300,301,100))
    collision_vertices,collision_faces,collision_start=volume([-120,0,120],ys,0,5,0)
    for x in (-120,120):volume([x-10,x+10],ys,0,20,1)
    # End compression strips need only their visible upper face.
    for y in (-300,300):
        start=len(vertices);vertices.extend(vertex(x,yy,6) for x,yy in ((-120,y-2.5),(120,y-2.5),(120,y+2.5),(-120,y+2.5)))
        faces.append(tuple(range(start,start+4)));slots.append(2)
    # FBX's conversion into Unreal reverses the authored Y axis. These samples are already UE-local.
    vertices=[(v.x,-v.y,v.z) for v in vertices]
    faces=[tuple(reversed(f)) for f in faces]
    mesh=bpy.data.meshes.new(section['name']);mesh.from_pydata(vertices,[],faces);mesh.update()
    for m in materials:mesh.materials.append(m)
    for polygon,slot in zip(mesh.polygons,slots):polygon.material_index=slot
    actor=bpy.data.objects.new(section['name'],mesh);collection.objects.link(actor)
    # One thin convex collision layer; no per-triangle runtime collision.
    hull=bpy.data.meshes.new('UCX');hull.from_pydata([(v.x,-v.y,v.z) for v in collision_vertices],[],[tuple(reversed(tuple(i-collision_start for i in f))) for f in collision_faces]);hull.update()
    collider=bpy.data.objects.new('UCX_'+section['name']+'_00',hull);collection.objects.link(collider)
    bpy.ops.object.select_all(action='DESELECT');actor.select_set(True);collider.select_set(True);bpy.context.view_layer.objects.active=actor
    bpy.ops.export_scene.fbx(filepath=str(OUT/(section['name']+'.fbx')),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False)
    collider.hide_set(True)
    triangles=len(faces)*2;assert triangles<=200
    manifest[section['name']]=dict(triangles=triangles,panel_area_m2=14.4,collision_hulls=1,collision='thin_convex',ground_laid=True)
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'MarsStationProjectedFilm.blend'))
print('Exported',len(manifest),'projected film sections')
