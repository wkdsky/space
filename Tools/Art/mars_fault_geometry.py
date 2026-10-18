"""Closed crust slivers extracted from the intact mesh along future fault edges."""
import bmesh
from mathutils import Vector


def append_mesh(target, source):
    import bpy
    mesh=bpy.data.meshes.new('FaultAppendTemporary')
    source.to_mesh(mesh)
    target.from_mesh(mesh)
    bpy.data.meshes.remove(mesh)
    source.free()


def split_fault_slivers(bm, radial, planes, clip_closed):
    """Return a parent plate and small, separately animated falling crust solids.

    Every sliver is subtracted from its parent, preserving the original surface
    at frame zero. A deep plate body and its residual skin share the same bone.
    """
    candidates=[]
    for normal,offset in planes:
        if normal.dot(radial)<-.85:
            continue
        edge=[v.co.copy() for v in bm.verts if abs(normal.dot(v.co)-offset)<2.e-5 and radial.dot(v.co)>.76]
        if len(edge)<2:
            continue
        extent=max((a-b).length for a in edge for b in edge)
        if extent>.20:
            candidates.append((extent,normal,offset))
    if not candidates:
        return [(bm,False)]
    skin=bm.copy()
    clip_closed(skin,-radial,-.76)
    clip_closed(bm,radial,.76)
    pieces=[]
    for _,normal,offset in sorted(candidates,key=lambda x:-x[0])[:1]:
        width=.014
        strip=skin.copy()
        clip_closed(strip,-normal,-offset+width)
        if not strip.faces or abs(strip.calc_volume())<2.e-6:
            strip.free()
            continue
        clip_closed(skin,normal,offset-width)
        tangent=radial.cross(normal).normalized()
        positions=[tangent.dot(v.co) for v in strip.verts]
        lo,hi=min(positions),max(positions)
        # Unequal lengths avoid the regular row of identical falling bricks.
        for fraction in (.29,.68):
            boundary=lo+(hi-lo)*fraction
            part=strip.copy()
            clip_closed(part,tangent,boundary)
            clip_closed(strip,-tangent,-boundary)
            if part.faces and abs(part.calc_volume())>1.e-7:
                pieces.append((part,True))
            else:
                append_mesh(bm,part)
        if strip.faces and abs(strip.calc_volume())>1.e-7:
            pieces.append((strip,True))
        else:
            append_mesh(bm,strip)
    append_mesh(bm,skin)
    return [(bm,False)]+pieces
