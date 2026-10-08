"""Fit the connected crater floor containing the authored radial anchor; render placement reference."""
import bpy
import json
import numpy as np
from pathlib import Path
from mathutils import Vector

root = Path("D:/projects/space/SourceArt/SkeletonAstronaut")
info = json.loads((root / "moon_placement_before.json").read_text())
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(root / "MoonPlanet_placement_reference.fbx"))
moon = next(o for o in bpy.data.objects if o.type == "MESH")
moon.select_set(True); bpy.context.view_layer.objects.active=moon
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
anchor = Vector(info["anchor_mesh_local"]) / 100
direction = anchor.normalized()
hit, location, normal, face = moon.ray_cast(direction * 4, -direction, distance=6)
assert hit
# The low-poly crater floor is authored as planar, adjacent faces.
floor = []
for p in moon.data.polygons:
    if p.normal.dot(normal) > 0.99999 and abs((p.center-location).dot(normal)) < 0.00005:
        floor.append(p.index)
verts = sorted({i for f in floor for i in moon.data.polygons[f].vertices})
center = sum((moon.data.vertices[i].co for i in verts), Vector()) / len(verts)
report = {"anchor_mesh_local_m": list(anchor), "hit_local_m": list(location),
          "normal_local": list(normal), "floor_faces": floor, "floor_vertices": verts,
          "floor_vertex_center_local_m": list(center),
          "floor_vertices_local_m": [list(moon.data.vertices[i].co) for i in verts]}
# The authored rim uses one concentric ring at radius 1.712 m in the source mesh.
# Select it around this anchor, then fit its plane and circle rather than using a triangle centroid.
rim = [v for v in moon.data.vertices if abs(v.co.length-1.712) < 0.003 and (v.co-anchor).length < 1.25]
points=np.array([list(v.co) for v in rim]); origin=points.mean(axis=0)
_,_,basis=np.linalg.svd(points-origin)
coords=(points-origin) @ basis[:2].T
solution=np.linalg.lstsq(np.column_stack([2*coords,np.ones(len(coords))]),(coords**2).sum(axis=1),rcond=None)[0]
rim_center=origin+basis[:2].T @ solution[:2]
center_direction=Vector(rim_center.tolist()).normalized()
ok,center_hit,center_normal,_=moon.ray_cast(center_direction*4,-center_direction,distance=6)
assert ok
report.update({"rim_vertices":[v.index for v in rim],"rim_center_local_m":rim_center.tolist(),
               "rim_radius_m":float(np.sqrt(solution[2]+sum(solution[:2]**2))),
               "rim_plane_max_error_m":float(max(abs((points-origin) @ basis[2]))),
               "center_surface_local_m":list(center_hit),"center_normal_local":list(center_normal)})
(root / "moon_crater_fit.json").write_text(json.dumps(report, indent=2))
print("CRATER_FLOOR", report)
mat = bpy.data.materials.new("MoonGrey"); mat.use_nodes=True
mat.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value=(0.18,0.20,0.23,1)
moon.data.materials.clear(); moon.data.materials.append(mat)
for p in moon.data.polygons: p.material_index=0; p.use_smooth=False
mark = bpy.data.materials.new("AnchorRed"); mark.use_nodes=True
mark.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value=(0.9,0.02,0.01,1)
bpy.ops.mesh.primitive_uv_sphere_add(radius=0.012, location=location+normal*0.015)
bpy.context.object.data.materials.append(mark)
target=location
bpy.ops.object.camera_add(location=target+normal*2.1+Vector((0,0,0.3)))
cam=bpy.context.object; cam.rotation_euler=(target-cam.location).to_track_quat("-Z","Y").to_euler()
cam.data.type="ORTHO";cam.data.ortho_scale=1.7;bpy.context.scene.camera=cam
for loc in (target+normal*2+Vector((2,0,2)),target+normal*3-Vector((2,0,1))):
    bpy.ops.object.light_add(type="AREA",location=loc); light=bpy.context.object
    light.data.energy=180;light.data.size=2
    light.rotation_euler=(target-light.location).to_track_quat("-Z","Y").to_euler()
scene=bpy.context.scene;scene.render.engine="CYCLES";scene.cycles.samples=16
scene.render.resolution_x=900;scene.render.resolution_y=900
scene.render.image_settings.file_format="PNG";scene.render.filepath=str(root/"Moon_Crater_Before.png")
bpy.ops.render.render(write_still=True)
