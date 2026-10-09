"""Build nine small faceted mouths fitted to the detected crater foot edges."""
import bpy
import json
import math
from pathlib import Path
import numpy as np
from mathutils import Vector

root=Path("D:/projects/space/SourceArt/MoonAntNests")
source=json.loads((root/"moon_crater_source.json").read_text())
foot=json.loads((root/"moon_crater_foot.json").read_text())
lookup={int(i):np.array(p) for i,p in source["vertices_local_cm"].items()}
ring=[lookup[i] for i in foot["foot_vertex_ids"]]
lengths=[float(np.linalg.norm(ring[(i+1)%len(ring)]-p)) for i,p in enumerate(ring)]
total=sum(lengths)
anchor=np.array(source["anchor_local_cm"])
first=min(range(len(ring)),key=lambda i:np.linalg.norm((ring[i]+ring[(i+1)%len(ring)])/2-anchor))
start=sum(lengths[:first])+lengths[first]/2
center=np.array(foot["foot_center_local_cm"])
edge_lookup={tuple(sorted(e["vertices"])):e for e in foot["edges"]}
bpy.ops.wm.read_factory_settings(use_empty=True)
black=bpy.data.materials.new("M_AntNestVoid");black.diffuse_color=(0.001,0.001,0.001,1)
rock=bpy.data.materials.new("M_MoonRock");rock.diffuse_color=(0.43,0.45,0.46,1)
placements=[]
inner=[(-.34,0),(.34,0),(.375,.10),(.31,.29),(.16,.425),(-.065,.45),(-.265,.365),(-.375,.20)]
outer=[(-.46,-.025),(.465,-.025),(.495,.11),(.405,.355),(.215,.535),(-.085,.57),(-.35,.445),(-.49,.235)]

for nest in range(9):
    distance=(start+nest*total/9)%total
    i=0
    while distance>lengths[i]:distance-=lengths[i];i+=1
    # A mouth occupies less than a metre; keep it away from three-face vertices.
    fraction=max(.07,min(.93,distance/lengths[i]))
    point=ring[i]*(1-fraction)+ring[(i+1)%len(ring)]*fraction
    edge=tuple(sorted((foot["foot_vertex_ids"][i],foot["foot_vertex_ids"][(i+1)%len(ring)])))
    info=edge_lookup[edge]
    up=np.array(info["floor_normal"])
    right=ring[(i+1)%len(ring)]-ring[i];right/=np.linalg.norm(right)
    forward=np.cross(right,up);forward/=np.linalg.norm(forward)
    if forward@(center-point)<0:forward=-forward;right=-right
    wall=np.array(info["wall_normal"])
    wall_local=np.array([wall@forward,wall@right,wall@up])
    assert wall_local[0]>.5 and abs(wall_local[1])<1e-5,wall_local
    slope=wall_local[2]/wall_local[0]
    vertices=[];faces=[];slots=[]
    def add(y,z,offset):
        p=np.array([-slope*z,y,z])+wall_local*offset
        # UE's FBX import reflects the Blender Y axis.
        vertices.append((float(p[0]),float(-p[1]),float(p[2])))
        return len(vertices)-1
    dark=[add(y,-.002 if z==0 else z,.0025) for y,z in inner]
    faces.append(tuple(reversed(dark)));slots.append(0)
    rim_inner=[add(y,z,.025) for y,z in inner]
    rim_outer=[add(y,z,-.004 if z<0 else .001) for y,z in outer]
    for j in range(len(inner)):
        if j==0:continue # Keep the floor-to-mouth threshold open, without a stone crossbar.
        k=(j+1)%len(inner)
        faces.append((rim_inner[j],rim_inner[k],rim_outer[k],rim_outer[j]));slots.append(1)
        faces.append((dark[j],dark[k],rim_inner[k],rim_inner[j]));slots.append(1)
    # A short dark threshold follows the actual basin floor, connecting the mouth to it.
    threshold=[]
    for x,y in ((-.02,-.34),(.10,-.30),(.10,.30),(-.02,.34)):
        vertices.append((x,-y,.0025));threshold.append(len(vertices)-1)
    faces.append(tuple(threshold));slots.append(0)
    mesh=bpy.data.meshes.new(f"AntNestEntrance_{nest+1:02}")
    mesh.from_pydata(vertices,[],faces);mesh.update()
    obj=bpy.data.objects.new(f"SM_MoonAntNestEntrance_{nest+1:02}",mesh)
    bpy.context.collection.objects.link(obj)
    mesh.materials.append(black);mesh.materials.append(rock)
    # Choose the outward winding explicitly for every small surface patch.
    outward=Vector((wall_local[0],-wall_local[1],wall_local[2]))
    for polygon,slot in zip(mesh.polygons,slots):
        polygon.material_index=slot;polygon.use_smooth=False
        if polygon.normal.dot(outward)<0:polygon.flip()
    bpy.ops.object.select_all(action="DESELECT");obj.select_set(True);bpy.context.view_layer.objects.active=obj
    bpy.ops.export_scene.fbx(filepath=str(root/(obj.name+".fbx")),use_selection=True,object_types={"MESH"},
        bake_anim=False,axis_forward="-Y",axis_up="Z",mesh_smooth_type="FACE")
    placements.append({"name":f"Moon_AntNest_{nest+1:02}","mesh":obj.name,"point_local_cm":point.tolist(),
        "forward_local":forward.tolist(),"right_local":right.tolist(),"up_local":up.tolist(),
        "edge_vertices":list(edge),"edge_fraction":fraction,"wall_triangle":info["wall_triangle"],
        "floor_triangle":info["floor_triangle"],"slope_break_degrees":info["slope_break_degrees"],
        "opening_width_cm":75,"opening_height_cm":45})
    obj.hide_render=True

(root/"moon_ant_nest_placements.json").write_text(json.dumps({"count":9,"foot_perimeter_world_m":total*82/100,
    "placements":placements},indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(root/"MoonAntNestEntrances.blend"))
print("ANT_ENTRANCES_BUILT",[(p["name"],p["edge_vertices"],round(p["slope_break_degrees"],2)) for p in placements])
