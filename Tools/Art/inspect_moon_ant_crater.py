"""Export the current user-selected crater and actual Moon LOD0 geometry."""
import json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()/"SourceArt/MoonAntNests"
root.mkdir(parents=True,exist_ok=True)
actors={a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
anchor=actors["Moon_CorpseAnchor_A"]
moon=actors["MoonPlanet"].static_mesh_component
mesh=moon.static_mesh
d=mesh.get_static_mesh_description(0)
def v(p):return [p.x,p.y,p.z]
t=moon.get_world_transform()
q=t.rotation
report={"anchor":anchor.get_path_name(),"anchor_world_cm":v(anchor.get_actor_location()),
        "anchor_local_cm":v(t.inverse_transform_location(anchor.get_actor_location())),
        "moon_mesh":mesh.get_path_name(),"moon_transform":{"location":v(t.translation),
         "rotation_xyzw":[q.x,q.y,q.z,q.w],"scale":v(t.scale3d)},
        "vertices_local_cm":{str(i):v(d.get_vertex_position(unreal.VertexID(i)))
            for i in range(d.get_vertex_count()) if d.is_vertex_valid(unreal.VertexID(i))},
        "triangles":[[d.get_vertex_instance_vertex(d.get_triangle_vertex_instance(unreal.TriangleID(i),j)).id_value
                      for j in range(3)] for i in range(d.get_triangle_count()) if d.is_triangle_valid(unreal.TriangleID(i))]}
report["existing_nests"]=[a.get_path_name() for a in actors.values() if isinstance(a,unreal.JTSMoonAntNestActor)]
(root/"moon_crater_source.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
print("CRATER_SOURCE",{k:val for k,val in report.items() if k not in ("vertices_local_cm","triangles")})
print("MATERIALS",[moon.get_material(i).get_path_name() for i in range(moon.get_num_materials())])
print("DIRTY",[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()+unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()])
