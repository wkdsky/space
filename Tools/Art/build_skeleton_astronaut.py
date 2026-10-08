"""Combine CC0 KayKit skull and Quaternius suit on the exported player rig.

Run with Blender --background --factory-startup --python <this file>.
Only creates/updates dedicated SkeletonAstronaut source art.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Vector, Matrix

ROOT = Path("D:/projects/space/SourceArt/SkeletonAstronaut")
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(ROOT / "Casual_2_reference.fbx"))
rig = bpy.data.objects["CharacterArmature"]
rig.animation_data_clear()
world = rig.matrix_world.copy()
rig.parent = None
rig.matrix_world = world
rig.data.pose_position = "REST"
for o in list(bpy.data.objects):
    if o != rig:
        bpy.data.objects.remove(o, do_unlink=True)
with bpy.data.libraries.load(str(ROOT / "Astronaut_RaeTheRedPanda.blend"), link=False) as (src, dst):
    dst.objects = ["RaeTheRedPanda", "CharacterArmature"]
for o in dst.objects:
    bpy.context.collection.objects.link(o)
source_rig = next(o for o in dst.objects if o.type == "ARMATURE")
suit = next(o for o in dst.objects if o.type == "MESH")
source_rig.animation_data_clear()
source_rig.data.pose_position = "REST"
bpy.context.view_layer.update()

PALETTE = [
    ("M_SA_SuitIvory", (0.73,0.77,0.73,1)),
    ("M_SA_Graphite", (0.025,0.044,0.064,1)),
    ("M_SA_Orange", (0.94,0.24,0.065,1)),
    ("M_SA_Bone", (0.86,0.79,0.60,1)),
    ("M_SA_Socket", (0.009,0.017,0.026,1)),
    ("M_SA_Cyan", (0.025,0.70,0.80,1)),
    ("M_SA_Metal", (0.19,0.25,0.30,1)),
]
materials=[]
for name,col in PALETTE:
    m=bpy.data.materials.new(name); m.diffuse_color=col; m.use_nodes=True
    p=m.node_tree.nodes.get("Principled BSDF")
    p.inputs["Base Color"].default_value=col
    p.inputs["Roughness"].default_value=0.8
    if "Cyan" in name:
        p.inputs["Emission Color"].default_value=col
        p.inputs["Emission Strength"].default_value=0.7
    materials.append(m)

atlas=bpy.data.images.load(str(ROOT / "Atlas.png"),check_existing=True)
pixels=list(atlas.pixels); aw,ah=atlas.size
uv=suit.data.uv_layers["UVMap"]
face_colors=[]
for p in suit.data.polygons:
    v=uv.data[p.loop_start].uv
    idx=4*(min(ah-1,max(0,int(v.y*ah)))*aw+min(aw-1,max(0,int(v.x*aw))))
    rgb=pixels[idx:idx+3]
    face_colors.append(0 if max(rgb)>0.55 and max(rgb)-min(rgb)<0.22 else 1 if max(rgb)<0.26 else 2 if rgb[0]>rgb[2]*1.5 else 6)
# The connected body and separate neck collar are retained; panda head/eyes removed.
bm=bmesh.new(); bm.from_mesh(suit.data); bm.verts.ensure_lookup_table()
bmesh.ops.delete(bm,geom=[v for v in bm.verts if v.index >= 2249],context="VERTS")
bm.to_mesh(suit.data); bm.free()
for m in list(suit.modifiers): suit.modifiers.remove(m)
src_bones={b.name:source_rig.matrix_world @ b.matrix_local for b in source_rig.data.bones}
dst_bones={b.name:rig.matrix_world @ b.matrix_local for b in rig.data.bones}
mapping={n:n.replace(".L","_L").replace(".R","_R") for n in src_bones}
mapping.update({"PoleTarget.L":"PT_L","PoleTarget.R":"PT_R"})

def frame_map(name):
    sb=source_rig.data.bones[name]; target=mapping[name]
    if target not in rig.data.bones: target="Torso"
    db=rig.data.bones[target]
    sh=source_rig.matrix_world @ sb.head_local
    st=source_rig.matrix_world @ sb.tail_local
    dh=rig.matrix_world @ db.head_local
    dt=rig.matrix_world @ db.tail_local
    # Source torso ends at Neck; target has an additional Chest articulation.
    if name == "Torso": dt=rig.matrix_world @ rig.data.bones["Neck"].head_local
    if name in ("LowerLeg.L", "LowerLeg.R"):
        # The original rig drives feet independently from Root. Its exported calf
        # tails are not the ankle joints; fit the cuffs to the actual foot heads.
        dt=rig.matrix_world @ rig.data.bones[mapping[name.replace("LowerLeg", "Foot")]].head_local
    sd=(st-sh).normalized(); dd=(dt-dh).normalized()
    rotate=sd.rotation_difference(dd)
    length_scale=(dt-dh).length/(st-sh).length
    thickness=0.5
    if name in ("Foot.L", "Foot.R"):
        # Terminal bone display lengths differ on the shared player skeleton.
        # They must not determine boot anatomy: use the same uniform fit on both.
        length_scale=thickness
    def apply(v):
        d=v-sh; parallel=sd*d.dot(sd)
        return dh + rotate @ (parallel*length_scale+(d-parallel)*thickness)
    return target,apply

transforms={n:frame_map(n) for n in src_bones}
old_names={g.index:g.name for g in suit.vertex_groups}
weights=[]
world=suit.matrix_world.copy()
for v in suit.data.vertices:
    if v.index>=2115:
        # Neck ring has a deliberately generous fit around the skull helmet.
        co=world @ v.co
        v.co=Vector((co.x*0.52,(co.y-0.22)*0.52-0.045,1.49+(co.z-1.74)*0.38))
        weights.append({"Neck":1.0})
        continue
    influences=[(old_names[g.group],g.weight) for g in v.groups if old_names[g.group] in transforms and g.weight>1e-6]
    total=sum(w for _,w in influences)
    if total<=0: raise RuntimeError("Source suit vertex has no valid deform weights")
    co=world @ v.co; out=Vector(); new={}
    for name,w in influences:
        target,fn=transforms[name]; w/=total
        out+=fn(co)*w; new[target]=new.get(target,0)+w
    v.co=out; weights.append(new)
suit.parent=None; suit.matrix_world=Matrix.Identity(4)
suit.vertex_groups.clear()
groups={n:suit.vertex_groups.new(name=n) for n in sorted({n for d in weights for n in d})}
for i,d in enumerate(weights):
    for n,w in d.items(): groups[n].add([i],w,"REPLACE")
suit.data.materials.clear()
for m in materials: suit.data.materials.append(m)
for p,slot in zip(suit.data.polygons,face_colors): p.material_index=slot; p.use_smooth=False
suit.name="SA_Spacesuit"
parts=[suit]
bpy.ops.import_scene.gltf(filepath=str(ROOT / "Skeleton_Warrior.glb"))
kayrig=next(o for o in bpy.data.objects if o.type=="ARMATURE" and o not in (rig,source_rig))
kayrig.animation_data_clear(); kayrig.data.pose_position="REST"
bpy.context.view_layer.update()
skull=[bpy.data.objects[n] for n in ("Skeleton_Warrior_Head","Skeleton_Warrior_Jaw","Skeleton_Warrior_Eyes")]
points=[o.matrix_world @ v.co for o in skull for v in o.data.vertices]
lo=Vector([min(c[i] for c in points) for i in range(3)]); hi=Vector([max(c[i] for c in points) for i in range(3)])
center=(lo+hi)*0.5
size=Vector((0.42,0.42,0.43)); target=Vector((0,-0.09,1.755))
for o in skull:
    matrix=o.matrix_world.copy()
    for mod in list(o.modifiers): o.modifiers.remove(mod)
    for v in o.data.vertices:
        co=matrix @ v.co
        v.co=Vector([(co[i]-center[i])*size[i]/(hi[i]-lo[i])+target[i] for i in range(3)])
    o.parent=None; o.matrix_world=Matrix.Identity(4)
    o.vertex_groups.clear(); g=o.vertex_groups.new(name="Head");g.add(list(range(len(o.data.vertices))),1,"REPLACE")
    o.data.materials.clear()
    # Original socket geometry stays dark through the gradient atlas swatch.
    for m in materials:o.data.materials.append(m)
    for p in o.data.polygons: p.material_index=5 if "Eyes" in o.name else 3; p.use_smooth=False
    parts.append(o)

def geometry(name,verts,faces,slot,bone):
    mesh=bpy.data.meshes.new(name); mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(o)
    for m in materials:mesh.materials.append(m)
    for p in mesh.polygons:p.material_index=slot
    o.vertex_groups.new(name=bone).add(list(range(len(verts))),1,"REPLACE")
    parts.append(o);return o

# Faceted helmet: a rear shell and three stepped visor rings leave the skull visible.
hc=Vector((0,-0.01,1.765)); radius=(0.29,0.30,0.31); segments=16
angles=[math.radians(a) for a in (58,82,108,134,157,178)]
verts=[]
for a in angles:
    for i in range(segments):
        t=2*math.pi*i/segments
        verts.append(hc+Vector((radius[0]*math.sin(a)*math.cos(t),-radius[1]*math.cos(a),radius[2]*math.sin(a)*math.sin(t))))
faces=[]
for r in range(len(angles)-1):
    for i in range(segments): j=(i+1)%segments; faces.append((r*segments+i,r*segments+j,(r+1)*segments+j,(r+1)*segments+i))
faces.append(tuple(range((len(angles)-1)*segments,len(angles)*segments)))
geometry("SA_HelmetShell",verts,faces,0,"Head")
for name,scale,y,slot in (("SA_VisorRim",1,-0.186,1),("SA_VisorTrim",0.95,-0.202,5)):
    verts=[]
    for factor in (scale,scale-0.06):
        for i in range(segments):
            t=2*math.pi*i/segments
            verts.append(hc+Vector((0.253*factor*math.cos(t),y,0.271*factor*math.sin(t))))
    faces=[(i,(i+1)%segments,(i+1)%segments+segments,i+segments) for i in range(segments)]
    geometry(name,verts,faces,slot,"Head")

# A compact life-support backpack and front expedition panel reinforce the silhouette.
def box(name,loc,size,slot,bone):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc)
    o=bpy.context.object;o.name=name;o.scale=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    bm=bmesh.new();bm.from_mesh(o.data)
    bmesh.ops.bevel(bm,geom=list(bm.edges),offset=0.018,segments=1,affect="EDGES")
    bm.to_mesh(o.data);bm.free()
    for m in materials:o.data.materials.append(m)
    for p in o.data.polygons:p.material_index=slot
    o.vertex_groups.new(name=bone).add(list(range(len(o.data.vertices))),1,"REPLACE")
    parts.append(o)
box("SA_Backpack",(0,0.22,1.265),(0.36,0.16,0.38),6,"Torso")
box("SA_ChestPanel",(0,-0.25,1.305),(0.20,0.045,0.15),1,"Torso")
box("SA_ChestSignal",(0,-0.278,1.32),(0.12,0.01,0.032),5,"Torso")
box("SA_ChestStripe",(0,-0.278,1.265),(0.13,0.01,0.015),2,"Torso")

bpy.ops.object.select_all(action="DESELECT")
for o in parts:o.select_set(True)
bpy.context.view_layer.objects.active=suit
bpy.ops.object.join();combined=bpy.context.object;combined.name="SK_SkeletonAstronaut"
# Bake world coordinates into the target rig's local coordinates.
world=combined.matrix_world.copy()
for v in combined.data.vertices:v.co=rig.matrix_world.inverted() @ world @ v.co
combined.matrix_world=rig.matrix_world.copy();combined.parent=rig
combined.matrix_parent_inverse=Matrix.Identity(4);combined.matrix_basis=Matrix.Identity(4)
mod=combined.modifiers.new("PlayerSkeleton","ARMATURE");mod.object=rig
for o in list(bpy.data.objects):
    if o not in (rig,combined):bpy.data.objects.remove(o,do_unlink=True)
rig.data.pose_position="POSE"
bpy.context.view_layer.update()
bpy.ops.object.select_all(action="DESELECT");rig.select_set(True);combined.select_set(True)
bpy.context.view_layer.objects.active=combined
bpy.ops.export_scene.fbx(filepath=str(ROOT / "SK_SkeletonAstronaut.fbx"),use_selection=True,
    object_types={"ARMATURE","MESH"},add_leaf_bones=False,bake_anim=False,
    axis_forward="-Y",axis_up="Z",use_armature_deform_only=False,
    mesh_smooth_type="FACE",apply_scale_options="FBX_SCALE_NONE")
combined.data.calc_loop_triangles()
report={"vertices":len(combined.data.vertices),"triangles":len(combined.data.loop_triangles),"bones":len(rig.data.bones)+1,
    "unweighted_vertices":sum(not v.groups for v in combined.data.vertices),"dimensions_m":list(combined.dimensions),
    "material_slots":[m.name for m in combined.data.materials]}
boot_dimensions={}
for side in ("L", "R"):
    group=combined.vertex_groups["Foot_"+side].index
    points=[combined.matrix_world @ v.co for v in combined.data.vertices
            if any(g.group==group and g.weight>0.5 for g in v.groups)]
    boot_dimensions[side]=[max(p[i] for p in points)-min(p[i] for p in points) for i in range(3)]
assert max(abs(a-b) for a,b in zip(boot_dimensions["L"],boot_dimensions["R"]))<0.004, boot_dimensions
report["boot_dimensions_m"]=boot_dimensions
(ROOT/"assembly_report.json").write_text(json.dumps(report,indent=2))
print("ASSEMBLY_REPORT",report)

# Reusable studio preview, saved with the editable source.
def point_at(o,p):o.rotation_euler=(Vector(p)-o.location).to_track_quat("-Z","Y").to_euler()
bpy.ops.object.camera_add(location=(3.1,-5.6,2.8));cam=bpy.context.object;point_at(cam,(0,0,1.02));cam.data.type="ORTHO";cam.data.ortho_scale=2.7
bpy.context.scene.camera=cam
for loc,power,size in (((-3,-4,5),550,4),((3,-1,4),400,3),((0,3,4),600,3)):
    bpy.ops.object.light_add(type="AREA",location=loc);o=bpy.context.object;o.data.energy=power;o.data.shape="DISK";o.data.size=size;point_at(o,(0,0,1))
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-0.025));ground=bpy.context.object
gm=bpy.data.materials.new("StudioGround");gm.diffuse_color=(0.028,0.045,0.062,1);ground.data.materials.append(gm)
scene=bpy.context.scene;scene.render.engine="CYCLES";scene.cycles.samples=32
scene.render.resolution_x=1100;scene.render.resolution_y=1100;scene.render.resolution_percentage=100
scene.world.color=(0.18,0.18,0.18);scene.view_settings.view_transform="AgX"
scene.render.image_settings.file_format="PNG";scene.render.filepath=str(ROOT / "Preview_Standing.png")
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT / "SkeletonAstronaut.blend"))
bpy.ops.render.render(write_still=True)
