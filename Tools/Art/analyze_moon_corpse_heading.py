"""Find a tangent heading that seats the corpse across the crater's actual facets."""
import json
import math
from pathlib import Path
import unreal

actors={a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
corpse=actors["Moon_Corpse_SkeletonAstronaut"]
transform=corpse.get_actor_transform()
moon=actors["MoonPlanet"].static_mesh_component
d=moon.static_mesh.get_static_mesh_description(0)
vertices={i:transform.inverse_transform_location(moon.get_world_transform().transform_location(d.get_vertex_position(unreal.VertexID(i))))
          for i in range(d.get_vertex_count()) if d.is_vertex_valid(unreal.VertexID(i))}
triangles=[]
for i in range(d.get_triangle_count()):
    t=unreal.TriangleID(i)
    if not d.is_triangle_valid(t):continue
    points=[vertices[d.get_vertex_instance_vertex(d.get_triangle_vertex_instance(t,j)).id_value] for j in range(3)]
    if any(min(getattr(p,axis) for p in points)>150 or max(getattr(p,axis) for p in points)<-150 for axis in ("x","y")):continue
    if max(p.z for p in points)<-1000:continue
    a,b,c=points
    determinant=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y)
    if abs(determinant)<1e-8:continue
    triangles.append((a,b,c,determinant))

def height(x,y):
    hits=[]
    for a,b,c,det in triangles:
        u=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/det
        v=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/det
        if u>=-1e-8 and v>=-1e-8 and u+v<=1.00000001:
            hits.append(u*a.z+v*b.z+(1-u-v)*c.z)
    assert hits,(x,y)
    return max(hits)

d=corpse.get_editor_property("corpse_mesh").static_mesh.get_static_mesh_description(0)
points=[d.get_vertex_position(unreal.VertexID(i)) for i in range(d.get_vertex_count()) if d.is_vertex_valid(unreal.VertexID(i))]
regions={"head":lambda p:p.y<-55,"back":lambda p:-55<=p.y<-15,
         "pelvis":lambda p:-15<=p.y<30,"left_boot":lambda p:p.y>75 and p.x<0,
         "right_boot":lambda p:p.y>75 and p.x>=0}
candidates=[]
for degrees in range(0,360,5):
    a=math.radians(degrees);c=math.cos(a);s=math.sin(a)
    gaps=[p.z-height(c*p.x-s*p.y,s*p.x+c*p.y) for p in points]
    contacts={name:min(gap for p,gap in zip(points,gaps) if predicate(p)) for name,predicate in regions.items()}
    candidates.append({"local_heading_delta_degrees":degrees,"minimum_gap_cm":min(gaps),
                       "region_contacts_cm":contacts,"worst_contact_cm":max(contacts.values())})
safe=[row for row in candidates if row["minimum_gap_cm"]>=0.25]
assert safe
best=min(safe,key=lambda row:row["worst_contact_cm"])
root=Path(unreal.Paths.project_dir()).resolve()/"SourceArt/SkeletonAstronaut"
report={"nearby_triangle_count":len(triangles),"current":candidates[0],"best":best}
(root/"moon_corpse_heading_analysis.json").write_text(json.dumps(report,indent=2))
print("MOON_CORPSE_HEADING",json.dumps(report))
