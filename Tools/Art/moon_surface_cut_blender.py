"""Fractured rock openings and paired closed cutters, authored by live Blender MCP."""
import bpy
import bmesh
import math
import random
import json
from pathlib import Path
import moon_surface_geometry as g


def outline(seed):
    rng=random.Random(seed)
    # Fracture joints change direction abruptly. The two faces have different
    # missing chips; they are not parallel ribbons around a symmetric lens.
    centers=[(-2.5,-.28),(-2.12,-.12),(-1.75,-.2),(-1.44,.06),(-1.02,.16),
             (-.66,-.08),(-.27,.02),(.10,0),(.49,.19),(.91,.34),
             (1.21,.24),(1.58,.43),(1.99,.27),(2.38,.40)]
    left=[];right=[]
    sx=rng.uniform(.90,1.10);sy=rng.uniform(.86,1.12)
    for i,(x,y) in enumerate(centers):
        y+=rng.uniform(-.06,.06)
        taper=.035 if i in (0,len(centers)-1) else rng.uniform(.09,.28)
        left.append((x*sx,(y-taper*rng.uniform(.65,1.2))*sy))
        right.append((x*sx,(y+taper*rng.uniform(.7,1.3))*sy))
        if i==4:
            # A narrow secondary fracture forks from one side of the main cut.
            right.extend([((x+.12)*sx,(y+.44)*sy),((x+.27)*sx,(y+1.02)*sy),
                          ((x+.31)*sx,(y+.43)*sy)])
    # Origin lies in the open main fracture, for refuge and physical-depth queries.
    return left+list(reversed(right))


def finish(obj):
    bm=bmesh.new();bm.from_mesh(obj.data)
    bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.00001)
    bmesh.ops.triangulate(bm,faces=list(bm.faces))
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    bm.to_mesh(obj.data);bm.free();obj.data.update()
    from moon_art_finish_blender import planar_uv
    planar_uv(obj)
    for slot in obj.material_slots:
        if slot.material and slot.material.name=='M_MS3_Moon':slot.material=bpy.data.materials['M_MS3_PropStone']
    return obj


def shard(b,x,y,sx,sy,h,angle,rng,z=-.035):
    polygon=[]
    for i in range(5):
        a=angle+i*math.tau/5;r=rng.uniform(.7,1.15)
        polygon.append((x+math.cos(a)*sx*r,y+math.sin(a)*sy*r))
    n=len(polygon)
    vertices=[(px,py,z) for px,py in polygon]+[(x+(px-x)*.65,y+(py-y)*.65,z+h*rng.uniform(.7,1.15)) for px,py in polygon]
    b.add(vertices,[tuple(range(n-1,-1,-1)),tuple(range(n,n*2))]+
          [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)],0)


def rim(name,seed):
    rng=random.Random(seed+910);points=outline(seed);b=g.Builder()
    # Sparse broken fragments, with bare terrain between them. The visible
    # fracture edge itself comes from the subtraction, not an added raised rim.
    for i in (2,5,9,12,18,23,27):
        x,y=points[i%len(points)];side=-1 if i<14 else 1
        shard(b,x,y+side*.075,rng.uniform(.06,.17),rng.uniform(.04,.10),
              rng.uniform(.025,.075),rng.uniform(0,6.28),rng)
    return finish(b.object(name,('Moon',)))


def loft(name,points,rings):
    n=len(points);v=[];faces=[];materials=[]
    for z,scale,dx,dy in rings:v.extend((x*scale+dx,y*scale+dy,z) for x,y in points)
    for row in range(len(rings)-1):
        for i in range(n):
            a=row*n+i;b=row*n+(i+1)%n;c=(row+1)*n+(i+1)%n;d=(row+1)*n+i
            faces.append((a,b,c,d));materials.append(0 if row<2 else 1)
    faces.extend([tuple(range(n-1,-1,-1)),tuple((len(rings)-1)*n+i for i in range(n))]);materials.extend([0,1])
    obj=finish(g.mesh_object(name,v,faces,('Moon','Void'),materials))
    bm=bmesh.new();bm.from_mesh(obj.data)
    assert all(e.is_manifold for e in bm.edges),name
    assert abs(bm.calc_volume())>.01,name
    bm.free()
    return obj


def volume(name,seed):
    depth=1.45+random.Random(seed).random()*.35
    return loft(name,outline(seed),[(1.3,1,0,0),(.03,1,0,0),(-.12,1.025,.01,0),
                (-.30,.91,-.015,.012),(-.62,.94,.025,-.025),(-1.05,.73,.035,.01),(-depth,.46,.08,.035)])


def mouth_outline(seed):
    rng=random.Random(seed)
    points=[]
    for i in range(13):
        a=math.tau*i/13;r=rng.uniform(.82,1.17)
        points.append((.82*math.cos(a)*r,.65*math.sin(a)*r))
    return points


def mouth_rim(name,seed):
    rng=random.Random(seed);b=g.Builder()
    if 'FixedNest' in name:
        for x,y,sx,sy,h in [(.22,-.62,.10,.07,.06),(.34,.57,.08,.05,.045),(.54,-.25,.06,.04,.035)]:
            shard(b,x,y,sx,sy,h,rng.uniform(0,6.28),rng)
        return finish(b.object(name,('Moon',)))
    # One broken bedrock overhang at the back. Unequal side fragments leave
    # the front open, instead of forming a regular arch or a complete collar.
    rear=[(-.34,-.53),(-.65,-.75),(-1.05,-.49),(-1.14,.10),(-.93,.62),(-.52,.67),(-.40,.24)]
    top=[(x,y,rng.uniform(.18,.34)) for x,y in rear]
    bottom=[(x+.06,y,-.11) for x,y in rear];n=len(rear)
    # The nine fixed mouths already abut a crater wall. Its surviving bedrock
    # supplies the roof; adding another cap there would read as a fitted lid.
    if 'FixedNest' not in name:
        b.add(bottom+top,[tuple(range(n-1,-1,-1)),tuple(range(n,n*2))]+
              [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)],0)
    for x,y,sx,sy,h in [(-.08,-.68,.22,.16,.15),(.32,-.62,.13,.10,.08),
                         (-.30,.65,.25,.17,.19),(.43,.48,.12,.10,.055),(.74,-.20,.07,.04,.035)]:
        shard(b,x+rng.uniform(-.07,.07),y,sx,sy,h,rng.uniform(0,6.28),rng)
    return finish(b.object(name,('Moon',)))


def mouth_volume(name,seed):
    if 'FixedCutVolume' in name:
        # These entrances sit at the toe of the original crater wall. Bore
        # horizontally under that wall, leaving its existing roof in place.
        rng=random.Random(seed);n=11;vertices=[];faces=[];mi=[]
        sections=[(.42,-.18,.44,.22),(.02,-.02,.66,.67),(-.42,-.08,.64,.66),
                  (-.92,-.25,.55,.60),(-1.52,-.60,.43,.46),(-1.96,-1.0,.24,.24)]
        shape=[rng.uniform(.74,1.18) for _ in range(n)]
        for x,z,w,h in sections:
            vertices.extend((x,math.cos(math.tau*i/n)*w*shape[i],z+math.sin(math.tau*i/n)*h*shape[i]) for i in range(n))
        for row in range(len(sections)-1):
            for i in range(n):
                faces.append((row*n+i,row*n+(i+1)%n,(row+1)*n+(i+1)%n,(row+1)*n+i));mi.append(0 if row<2 else 1)
        faces.extend([tuple(range(n-1,-1,-1)),tuple((len(sections)-1)*n+i for i in range(n))]);mi.extend([0,1])
        obj=finish(g.mesh_object(name,vertices,faces,('Moon','Void'),mi))
        bm=bmesh.new();bm.from_mesh(obj.data);assert all(e.is_manifold for e in bm.edges);bm.free()
        return obj
    return loft(name,mouth_outline(seed),[(1.2,1,0,0),(.04,1,0,0),(-.20,.93,-.06,.01),
                (-.42,1,-.16,.025),(-.82,.84,-.38,.04),(-1.85,.48,-.72,.10)])


def build():
    bpy.context.window.scene=bpy.data.scenes[g.SCENE];exported=[]
    specs=[]
    for i in range(4):
        specs += [(f'RiftRim_{i}',rim,500+i),(f'RiftCutVolume_{i}',volume,500+i),
                  (f'MirrorRefuge_{i}',mouth_rim,400+i),(f'MirrorCutVolume_{i}',mouth_volume,400+i)]
    for i in range(9):
        specs += [(f'FixedNest_{i+1:02d}',mouth_rim,1600+i),(f'FixedCutVolume_{i+1:02d}',mouth_volume,1600+i)]
    for suffix,fn,seed in specs:
        name='SM_MS3_'+suffix;old=bpy.data.objects.get(name)
        if old:bpy.data.objects.remove(old,do_unlink=True)
        obj=fn(name,seed)
        exported.append(g.export(obj));obj.hide_set(True);obj.hide_render=True
    (g.ROOT/'cut_modules.json').write_text(json.dumps(exported,indent=2),encoding='utf-8')
    bpy.ops.wm.save_as_mainfile(filepath=str(g.ROOT/'MoonSurfaceV3.blend'),compress=True)
    return dict(assets=len(exported),fracture_depth_cm=[145,180],mouth_depth_cm=185,
                max_fracture_debris_height_cm=7.5,max_burrow_overhang_height_cm=34)
