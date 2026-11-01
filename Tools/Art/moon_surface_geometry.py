"""Original faceted Moon attachments, authored in the live Blender through MCP.

Recipe positions use the existing UE mesh's local metres (actor scale baked in).
Export reflects Y once, matching the project's established Blender/FBX convention.
"""
import bpy
import bmesh
import json
import math
import random
from pathlib import Path
import numpy as np
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

ROOT = Path('D:/projects/space/SourceArt/MoonSurfaceV3')
SCENE = 'JTS_MoonSurface_V3'
PALETTE = {
    'Moon': '#BFC0C4', 'MoonLight': '#D3D4D5', 'Slope': '#A5A7AD',
    'Feed': '#C9BFAE', 'FeedLight': '#D0C8B9', 'Void': '#4A4B55',
    'Tower': '#ACAAA8', 'TowerLight': '#BBB9B5', 'Lip': '#B3B4BB',
}

def material(name):
    key = 'M_MS3_' + name
    mat = bpy.data.materials.get(key) or bpy.data.materials.new(key)
    mat.use_nodes = True
    rgb = [int(PALETTE[name][i:i+2], 16)/255 for i in (1,3,5)]
    linear = [v/12.92 if v <= .04045 else ((v+.055)/1.055)**2.4 for v in rgb]
    bsdf = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    bsdf.inputs['Base Color'].default_value = (*linear,1)
    bsdf.inputs['Roughness'].default_value = .88
    mat.diffuse_color = (*linear,1)
    return mat

def mesh_object(name, verts, faces, slots=('Moon',), indices=None, collection=None):
    # Input is right-handed numeric UE local coordinates. Reflect winding with Y.
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata([(v[0],-v[1],v[2]) for v in verts], [], [tuple(reversed(f)) for f in faces])
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    (collection or bpy.context.scene.collection).objects.link(obj)
    for key in slots: mesh.materials.append(material(key))
    if indices:
        for poly, index in zip(mesh.polygons, indices): poly.material_index = index
    for poly in mesh.polygons: poly.use_smooth = False
    return obj

def setup():
    ROOT.mkdir(parents=True, exist_ok=True)
    scene = bpy.data.scenes.get(SCENE) or bpy.data.scenes.new(SCENE)
    bpy.context.window.scene = scene
    for obj in list(scene.objects): bpy.data.objects.remove(obj, do_unlink=True)
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1
    return scene

class Surface:
    def __init__(self):
        data = json.loads(Path('D:/projects/space/SourceArt/MoonAntNests/moon_crater_source.json').read_text())
        self.verts = np.asarray([data['vertices_local_cm'][str(i)] for i in range(len(data['vertices_local_cm']))])*.82
        self.faces = np.asarray(data['triangles'], dtype=int)
        self.bvh = BVHTree.FromPolygons([Vector(v) for v in self.verts], self.faces.tolist(), all_triangles=True)
        tri = self.verts[self.faces]
        cross = np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0])
        self.areas = np.linalg.norm(cross,axis=1)*.5
        self.centers = tri.mean(axis=1)
        self.normals = cross / np.linalg.norm(cross,axis=1)[:,None]
        flip = np.sum(self.normals*self.centers,axis=1)<0
        self.normals[flip]*=-1
        self.samples=np.concatenate([tri[:,0]*a+tri[:,1]*b+tri[:,2]*(1-a-b)
                                     for a,b in ((1/3,1/3),(.6,.2),(.2,.6),(.2,.2))])
        self.weights=np.tile(self.areas/4,4)
        self.radials=self.samples/np.linalg.norm(self.samples,axis=1)[:,None]

    def hit(self, direction):
        d=Vector(direction).normalized()
        point,normal,index,distance=self.bvh.ray_cast(d*300,-d,600)
        if point is None: raise ValueError('No real surface at '+str(direction))
        if normal.dot(point)<0: normal=-normal
        return point,normal,index

    def frame(self, direction, angle=0):
        point,up,index=self.hit(direction)
        reference=Vector((0,0,1)) if abs(up.z)<.9 else Vector((1,0,0))
        forward=reference-up*reference.dot(up)
        forward.normalize()
        right=up.cross(forward).normalized()
        f=forward*math.cos(angle)+right*math.sin(angle)
        return point,f,up.cross(f).normalized(),up

    def ground(self, p, forward, right, up, x, y):
        query=p+forward*x+right*y
        hit,_,_,_=self.bvh.ray_cast(query+up*12,-up,30)
        return (hit-query).dot(up) if hit is not None else 0

def _region_metric(radials, region):
    up=np.array(region['direction']);f=np.array(region['forward']);r=np.cross(up,f)
    dot=np.clip(radials@up,-1,1)
    angles=np.arctan2(radials@r,radials@f)
    warp=1+.11*np.sin(3*angles+region['phase'])+.065*np.sin(5*angles-region['phase'])
    ellipse=np.sqrt(np.cos(angles)**2+(np.sin(angles)/region['minor'])**2)
    return np.arccos(dot)*150*ellipse/warp

def make_regions(surface):
    rng=random.Random(38031)
    directions=[Vector((-.28,.93,.23)).normalized()]
    count=15
    golden=math.pi*(3-math.sqrt(5))
    for i in range(count):
        z=1-2*(i+.5)/count
        radius=math.sqrt(1-z*z);a=i*golden+.58
        directions.append(Vector((radius*math.cos(a),radius*math.sin(a),z)))
    regions=[]
    for i,up in enumerate(directions):
        ref=Vector((0,0,1)) if abs(up.z)<.9 else Vector((1,0,0))
        f=(ref-up*ref.dot(up)).normalized();r=up.cross(f)
        a=rng.uniform(-math.pi,math.pi);f=f*math.cos(a)+r*math.sin(a)
        p,_,_=surface.hit(up)
        regions.append(dict(id=f'Eco_{i:02d}',point=list(p),direction=list(up),forward=list(f),
                            minor=rng.uniform(.74,.92),phase=rng.uniform(0,6.28),size=rng.uniform(.86,1.12)))
    metrics=np.stack([_region_metric(surface.radials,r)/r['size'] for r in regions])
    closest=metrics.min(axis=0)
    arcs=np.stack([np.arccos(np.clip(surface.radials@np.array(r['direction']),-1,1))*150/r['size'] for r in regions])
    activity_closest=np.maximum(metrics,arcs).min(axis=0)
    def solve(target,distances):
        lo,hi=10,180
        for _ in range(28):
            mid=(lo+hi)*.5
            fraction=surface.weights[distances<=mid].sum()/surface.weights.sum()
            if fraction<target:lo=mid
            else:hi=mid
        return (lo+hi)*.5
    core=solve(.5,closest);activity=solve(.75,activity_closest)
    for r in regions:
        r['core_radius_m']=core*r.pop('size')
        r['activity_radius_m']=activity*r['core_radius_m']/core
    coverage={'core':float(surface.weights[closest<=core].sum()/surface.weights.sum()),'activity':float(surface.weights[activity_closest<=activity].sum()/surface.weights.sum())}
    coverage['surface_area_m2']=float(surface.areas.sum())
    coverage['samples']=len(surface.samples)
    coverage['method']='Area-weighted samples on the unchanged 2400 actual mesh triangles; union, overlaps counted once.'
    return regions,coverage

def clip_polygon(poly, axis, limit, positive):
    if not poly:return []
    result=[]
    for a,b in zip(poly,poly[1:]+poly[:1]):
        da=(a[axis]-limit)*(1 if positive else -1);db=(b[axis]-limit)*(1 if positive else -1)
        if da>=0:result.append(a)
        if (da>=0)!=(db>=0): result.append(a+(b-a)*(da/(da-db)))
    return result

def fitted_polygon(surface, frame, polygon, offset=.009):
    p,f,r,u=frame
    verts=[];faces=[]
    # Clip each real triangle against a convex irregular footprint in tangent UV.
    for tid,triangle in enumerate(surface.verts[surface.faces]):
        rel=triangle-np.array(p)
        uvz=np.stack((rel@np.array(f),rel@np.array(r),rel@np.array(u)),axis=1)
        if np.min(uvz[:,2])>4 or np.max(uvz[:,2])<-4:continue
        clipped=[Vector(v) for v in uvz]
        for a,b in zip(polygon,polygon[1:]+polygon[:1]):
            result=[]
            edge=Vector((b[0]-a[0],b[1]-a[1]))
            for c,d in zip(clipped,clipped[1:]+clipped[:1]):
                dc=edge.x*(c.y-a[1])-edge.y*(c.x-a[0]);dd=edge.x*(d.y-a[1])-edge.y*(d.x-a[0])
                if dc>=-1e-8:result.append(c)
                if (dc>=0)!=(dd>=0):result.append(c+(d-c)*(dc/(dc-dd)))
            clipped=result
            if not clipped:break
        if len(clipped)<3:continue
        base=len(verts)
        normal=Vector(surface.normals[tid])
        for c in clipped:verts.append(p+f*c.x+r*c.y+u*c.z+normal*offset)
        for j in range(1,len(clipped)-1): faces.append((base,base+j,base+j+1))
    return verts,faces

def patch(surface, name, direction, seed):
    rng=random.Random(seed);frame=surface.frame(direction,rng.uniform(0,6.28))
    size=rng.uniform(1.2,2.4)
    polygon=[]
    # A convex irregular fan reads as a mineral/feeding stain, never grass.
    for i in range(7):
        a=i*2*math.pi/7;rad=size*(.88+rng.random()*.12)
        polygon.append((math.cos(a)*rad,math.sin(a)*rad*rng.uniform(.65,.8)))
    verts,faces=fitted_polygon(surface,frame,polygon)
    obj=mesh_object(name,verts,faces,('Feed','FeedLight'),[0 if i%5 else 1 for i in range(len(faces))])
    return obj,dict(point=list(frame[0]),forward=list(frame[1]),up=list(frame[3]))

def fissure(surface, name, direction, seed):
    rng=random.Random(seed);frame=surface.frame(direction,rng.uniform(0,6.28))
    n=rng.randint(2,4);length=rng.uniform(2.2,5.6);verts=[];faces=[]
    x=-length*.5;y=0
    for i in range(n):
        nx=x+length/n;ny=y+rng.uniform(-.4,.4)
        width=rng.uniform(.055,.17)
        poly=[(x,y-width),(nx,ny-width*.65),(nx+.08,ny+width*.65),(x-.08,y+width)]
        v,f=fitted_polygon(surface,frame,poly,.011)
        base=len(verts);verts+=v;faces += [tuple(base+j for j in t) for t in f]
        x,y=nx,ny
    obj=mesh_object(name,verts,faces,('Void',))
    return obj,dict(point=list(frame[0]),forward=list(frame[1]),up=list(frame[3]))

class Builder:
    def __init__(self):self.v=[];self.f=[];self.mat=[]
    def add(self,vertices,faces,material=0):
        start=len(self.v);self.v+=list(vertices)
        for face in faces:self.f.append(tuple(start+i for i in face));self.mat.append(material)
    def prism(self,polygon,top,height=0,material=0):
        v=[(x,y,height) for x,y in polygon]+[(x*.8,y*.8,top) for x,y in polygon]
        n=len(polygon);f=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]
        f += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
        self.add(v,f,material)
    def object(self,name,slots):return mesh_object(name,self.v,self.f,slots,self.mat)

def rift_refuge(name, seed):
    """Only the thin removable lip; depth is supplied by its paired runtime Boolean volume."""
    from moon_surface_cut_blender import rim
    return rim(name,seed)


def refuge(name, mirror, seed):
    if not mirror:return rift_refuge(name,seed)
    from moon_surface_cut_blender import mouth_rim
    return mouth_rim(name,seed)

def tower(name,seed,stage):
    rng=random.Random(seed);b=Builder()
    centers=[(0,0,2.05,.85),(-1.15,.4,.88,.5),(.8,.85,1.15,.54),(.65,-.9,.68,.46)]
    for towerindex,(cx,cy,height,radius) in enumerate(centers):
        n=7;angle=rng.uniform(0,6.28);leanx=rng.uniform(-.20,.20);leany=rng.uniform(-.16,.16)
        height*=rng.uniform(.85,1.15);radius*=rng.uniform(.9,1.1)
        rings=[]
        for z,s in ((-.10,1.15),(.12,1),(height*.68,.76),(height,.47)):
            rings += [(cx+radius*s*math.cos(angle+i*2*math.pi/n)+leanx*z,
                       cy+radius*s*math.sin(angle+i*2*math.pi/n)+leany*z,z) for i in range(n)]
        fs=[]
        for row in range(3):
            for i in range(n):
                # Stage changes open a dark low side seam on the principal tower only.
                if towerindex==0 and stage>0 and row==0 and i in ((0,) if stage==1 else (0,1)):continue
                fs.append((row*n+i,row*n+(i+1)%n,(row+1)*n+(i+1)%n,(row+1)*n+i))
        fs.append(tuple(range(3*n,4*n))) # sealed blunt top, never an open chimney.
        b.add(rings,fs,0 if towerindex%2==0 else 1)
        if towerindex==0:
            # Thin, closed offset seam: clear at close range, no permanent hole.
            a=angle+math.pi/n
            direction=Vector((math.cos(a),math.sin(a),0));side=Vector((-direction.y,direction.x,0))
            center=Vector((cx,cy,.24))+direction*(radius*.96)
            w=.025 if stage==0 else (.24 if stage==1 else .45)
            h=.30 if stage==0 else (.46 if stage==1 else .65)
            corners=[center-side*w,center+side*w,center+side*w+Vector((0,0,h)),center-side*w+Vector((0,0,h))]
            b.add(corners,[(0,1,2,3)],2)
    if stage>0:
        for i in range(2 if stage==1 else 3):
            a=rng.uniform(0,6.28);x=1.1*math.cos(a);y=1.1*math.sin(a)
            b.add([(x-.18,y-.12,-.03),(x+.25,y-.15,-.03),(x+.08,y+.22,-.03),(x,y,.23)],[(0,1,3),(1,2,3),(2,0,3),(0,2,1)],1)
    return b.object(name,('Tower','TowerLight','Void'))

def rock(name,kind):
    b=Builder()
    outlines=[([(-.8,-.4),(.65,-.5),(.9,.1),(.25,.6),(-.75,.3)],.22),
              ([(-.6,-.4),(.6,-.35),(.4,.45),(-.7,.3)],.5),
              ([(-.6,-.4),(.8,-.2),(-.15,.65)],.65)]
    outline,h=outlines[kind];b.prism(outline,h,-.12)
    return b.object(name,('Moon',))

def export(obj):
    bpy.ops.object.select_all(action='DESELECT')
    obj.hide_set(False);obj.select_set(True);bpy.context.view_layer.objects.active=obj
    path=ROOT/'FBX'/(obj.name+'.fbx');path.parent.mkdir(parents=True,exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},
        apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',
        use_mesh_modifiers=True,mesh_smooth_type='FACE',bake_anim=False)
    return dict(name=obj.name,file=str(path),triangles=sum(len(p.vertices)-2 for p in obj.data.polygons),
                materials=[m.name for m in obj.data.materials])

def portable_modules():
    """Reusable local-space attachments, with no vertices copied from the Moon."""
    from moon_art_finish_blender import crusts
    return crusts()

def preview_layout():
    """Linked instances let the source blend show the authored layout as well as modules."""
    scene=bpy.data.scenes[SCENE];bpy.context.window.scene=scene
    old=bpy.data.collections.get('PREVIEW_AuthoredMoon')
    if old:
        for obj in list(old.objects):bpy.data.objects.remove(obj,do_unlink=True)
        bpy.data.collections.remove(old)
    collection=bpy.data.collections.new('PREVIEW_AuthoredMoon');scene.collection.children.link(collection)
    plan=json.loads((ROOT/'surface_recipe.json').read_text(encoding='utf-8'))
    reflection=Matrix.Diagonal((1,-1,1));surface=Surface()
    def instance(row,name,baked=False):
        source=bpy.data.objects[row['mesh']];obj=source.copy();obj.name='PREVIEW_'+name
        collection.objects.link(obj);obj.hide_set(False);obj.hide_render=False
        if baked:obj.rotation_euler=(0,0,0)
        else:
            up=Vector(row.get('up',row.get('direction')));forward=Vector(row['forward'])
            forward=(forward-up*forward.dot(up)).normalized();right=up.cross(forward)
            obj.rotation_euler=(reflection@Matrix((forward,right,up)).transposed()@reflection).to_euler()
        obj.location=reflection@Vector(row['point']);obj.scale=(row.get('scale',1),)*3
        return obj
    for row in plan['fixed_nests']:instance(row,'Nest_'+str(row['index']))
    for row in plan['food_patches']:instance(row,row['id'],True)
    for row in plan['decorations']:instance(row,row['id'])
    for row in plan['towers']:instance(dict(row,mesh=f'SM_MS3_Tower_{row["variant"]}_0'),row['id'])
    for i,region in enumerate(plan['regions']):
        row=next(c for c in plan['cracks'] if c['id']==region['crack_ids'][0])
        instance(dict(row,mesh='SM_MS3_RiftRefuge_'+str(i%4)),'Refuge_'+region['id'])
        for j,food_id in enumerate(region['food_ids']):
            row=next(f for f in plan['food_patches'] if f['id']==food_id)
            direction=(Vector(row['point'])+Vector(row['forward'])*3.2).normalized()
            point,forward,right,up=surface.frame(direction,0)
            instance(dict(row,point=list(point),mesh='SM_MS3_MirrorRefuge_'+str((i+j)%4)),food_id+'_Shell')
    camera_data=bpy.data.cameras.new('MoonSurfaceOverview');camera=bpy.data.objects.new('MoonSurfaceOverview',camera_data)
    collection.objects.link(camera);camera.location=reflection@Vector((-120,510,180))
    camera.rotation_euler=(-camera.location).to_track_quat('-Z','Y').to_euler()
    camera_data.type='ORTHO';camera_data.ortho_scale=390;camera_data.clip_end=2000;scene.camera=camera
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'MoonSurfaceV3.blend'))
    return dict(preview_instances=len(collection.objects)-1,file=str(ROOT/'MoonSurfaceV3.blend'))

def build():
    scene=setup();surface=Surface();rng=random.Random(38031)
    regions,coverage=make_regions(surface)
    verts=surface.verts;faces=surface.faces
    planet=mesh_object('REFERENCE_ExistingMoon',verts,faces,('Moon',))
    planet.hide_select=True
    assets=[];placements=[];foods=[];cracks=[];towers=[];nests=[]
    def asset(obj, point=None):
        if point is not None:
            shift=Vector((point[0],-point[1],point[2]))
            for v in obj.data.vertices:v.co-=shift
        assets.append(export(obj))
        if point is not None:obj.location=shift
        return obj.name
    for i in range(3):asset(rock('SM_MS3_Rock_'+str(i),i))
    for i in range(4):
        for stage in range(3):asset(tower(f'SM_MS3_Tower_{i}_{stage}',900+i,stage))
    for i in range(4):
        asset(refuge(f'SM_MS3_MirrorRefuge_{i}',True,400+i))
        asset(refuge(f'SM_MS3_RiftRefuge_{i}',False,500+i))
    old=json.loads(Path('D:/projects/space/SourceArt/MoonAntNests/moon_ant_nest_placements.json').read_text())['placements']
    for i,item in enumerate(old):
        mirror=True;kind='MirrorSkimmer'
        # Keep the original floor-aligned coordinate system and exact entrance origin.
        name=asset(refuge(f'SM_MS3_FixedNest_{i+1:02d}',mirror,1600+i))
        nests.append(dict(index=i+1,species=kind,mesh=name,point=[v*.82 for v in item['point_local_cm']],
                          forward=item['forward_local'],right=item['right_local'],up=item['up_local']))
    for region in regions:
        up=Vector(region['direction']);forward=Vector(region['forward']);right=up.cross(forward)
        sunny=up.y>.18
        regionfoods=[];regioncracks=[]
        for j in range(rng.randint(2,4)):
            a=rng.uniform(0,6.28);distance=rng.uniform(5,region['core_radius_m']*.72)
            d=(up*150+(forward*math.cos(a)+right*math.sin(a))*distance).normalized()
            obj,frame=fissure(surface,f'SM_MS3_Crack_{region["id"]}_{j}',d,42000+len(cracks))
            name=asset(obj,frame['point'])
            row=dict(id='Crack_'+str(len(cracks)),mesh=name,region=region['id'],**frame)
            cracks.append(row);regioncracks.append(row['id'])
        if sunny:
            for j in range(rng.randint(1,3)):
                a=rng.uniform(0,6.28);distance=rng.uniform(4,region['core_radius_m']*.62)
                d=(up*150+(forward*math.cos(a)+right*math.sin(a))*distance).normalized()
                if d.y<.1:continue
                obj,frame=patch(surface,f'SM_MS3_Feed_{region["id"]}_{j}',d,62000+len(foods))
                name=asset(obj,frame['point'])
                row=dict(id='Feed_'+str(len(foods)),mesh=name,region=region['id'],**frame)
                foods.append(row);regionfoods.append(row['id'])
        region['food_ids']=regionfoods;region['crack_ids']=regioncracks
        # Three quiet rock shapes, scattered singly with wide gaps.
        for j in range(rng.randint(1,3)):
            a=rng.uniform(0,6.28);distance=rng.uniform(4,region['core_radius_m']*.7)
            d=(up*150+(forward*math.cos(a)+right*math.sin(a))*distance).normalized()
            p,f,r,u=surface.frame(d,rng.uniform(0,6.28))
            placements.append(dict(id=f'Rock_{len(placements)}',mesh='SM_MS3_Rock_'+str(rng.randrange(3)),point=list(p),forward=list(f),up=list(u),scale=rng.uniform(.5,1.25)))
        if up.y<-.32 and len(towers)<5:
            p,f,r,u=surface.frame(up,rng.uniform(0,6.28))
            towers.append(dict(id=f'Tower_{len(towers):02d}',variant=len(towers)%4,region=region['id'],point=list(p),forward=list(f),up=list(u)))
    recipe=dict(version=3,seed=38031,coordinate_system='UE mesh-local metres; original Moon scale 82 baked; actor transform retained',
        palette=PALETTE,coverage=coverage,regions=regions,fixed_nests=nests,food_patches=foods,cracks=cracks,towers=towers,decorations=placements,assets=assets)
    (ROOT/'surface_recipe.json').write_text(json.dumps(recipe,indent=2),encoding='utf-8')
    # Export modules stay at the origin; the native editor applies the recipe frames.
    for obj in list(scene.objects):
        if obj.name!='REFERENCE_ExistingMoon':obj.hide_set(True);obj.hide_render=True
    planet.hide_set(False)
    preview_layout()
    return dict(assets=len(assets),regions=len(regions),fixed_nests=len(nests),patches=len(foods),cracks=len(cracks),towers=len(towers),rocks=len(placements),coverage=coverage)
