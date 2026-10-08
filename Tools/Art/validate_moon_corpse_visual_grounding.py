"""Compare the corpse's actual support vertices with the Moon's LOD0 triangles."""
import json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()/"SourceArt/SkeletonAstronaut"
actors={a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
corpse=actors["Moon_Corpse_SkeletonAstronaut"]
component=corpse.get_editor_property("corpse_mesh")
moon=actors["MoonPlanet"].static_mesh_component
def vec(v):return (v.x,v.y,v.z)
up=vec(corpse.get_actor_up_vector())
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def add(a,b):return tuple(x+y for x,y in zip(a,b))
def mul(a,s):return tuple(x*s for x in a)
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
description=moon.static_mesh.get_static_mesh_description(0)
transform=moon.get_world_transform()
vertices={i:vec(transform.transform_location(description.get_vertex_position(unreal.VertexID(i))))
          for i in range(description.get_vertex_count()) if description.is_vertex_valid(unreal.VertexID(i))}
triangles=[]
for i in range(description.get_triangle_count()):
    triangle=unreal.TriangleID(i)
    if not description.is_triangle_valid(triangle):continue
    ids=[description.get_vertex_instance_vertex(description.get_triangle_vertex_instance(triangle,j)).id_value for j in range(3)]
    a,b,c=(vertices[j] for j in ids)
    triangles.append((a,sub(b,a),sub(c,a)))
direction=mul(up,-1)
def visual_altitude(point):
    start=add(point,mul(up,500))
    nearest=None;normal=None
    for a,e1,e2 in triangles:
        p=cross(direction,e2);det=dot(e1,p)
        if abs(det)<1e-10:continue
        tvec=sub(start,a);u=dot(tvec,p)/det
        if u<0 or u>1:continue
        q=cross(tvec,e1);v=dot(direction,q)/det
        if v<0 or u+v>1:continue
        distance=dot(e2,q)/det
        if 0<=distance<=1000 and (nearest is None or distance<nearest):
            nearest=distance
            n=cross(e1,e2);normal=mul(n,1/(dot(n,n)**0.5))
            if dot(normal,up)<0:normal=mul(normal,-1)
    assert nearest is not None,point
    return nearest-500,normal
description=component.static_mesh.get_static_mesh_description(0)
transform=component.get_world_transform()
support=[]
for i in range(description.get_vertex_count()):
    vertex=unreal.VertexID(i)
    if not description.is_vertex_valid(vertex):continue
    local=description.get_vertex_position(vertex)
    if local.z>2.5:continue
    world=vec(transform.transform_location(local))
    support.append({"vertex":i,"local_cm":vec(local),"local_z_cm":local.z,"visual_altitude_cm":visual_altitude(world)[0]})
root_altitude,root_normal=visual_altitude(vec(corpse.get_actor_location()))
regions={"head":lambda p:p[1]<-55,"back":lambda p:-55<=p[1]<-15,
         "pelvis":lambda p:-15<=p[1]<30,"left_boot":lambda p:p[1]>75 and p[0]<0,
         "right_boot":lambda p:p[1]>75 and p[0]>=0}
contacts={name:min(p["visual_altitude_cm"] for p in support if predicate(p["local_cm"]))
          for name,predicate in regions.items()}
report={"root_visual_altitude_cm":root_altitude,"surface_normal_alignment":dot(root_normal,up),
        "support_vertex_count":len(support),"minimum_visual_support_cm":min(p["visual_altitude_cm"] for p in support),
        "maximum_visual_support_cm":max(p["visual_altitude_cm"] for p in support),
        "region_contacts_cm":contacts,"supports":support}
assert abs(root_altitude-0.3)<0.01
assert report["surface_normal_alignment"]>0.99999
assert report["minimum_visual_support_cm"]>=0.2
assert max(contacts.values())<2.1, contacts
(root/"moon_corpse_visual_grounding.json").write_text(json.dumps(report,indent=2))
print("VISUAL_GROUNDING",{k:v for k,v in report.items() if k!="supports"})
