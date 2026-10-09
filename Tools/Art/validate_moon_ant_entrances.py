"""Validate loaded entrance geometry against the actual wall/floor junctions."""
import json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()/"SourceArt/MoonAntNests"
source=json.loads((root/"moon_crater_source.json").read_text())
plan=json.loads((root/"moon_ant_nest_placements.json").read_text())
actors={a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
assert "Moon_CorpseAnchor_A" not in actors
nests=[a for a in actors.values() if isinstance(a,unreal.JTSMoonAntNestActor)]
assert len(nests)==9
controller=actors["BP_MoonSurfaceController"]
assert not controller.get_editor_property("corpse_surface_anchor")
assert set(controller.get_editor_property("placed_moon_ant_nests"))==set(nests)
moon=actors["MoonPlanet"].static_mesh_component
transform=moon.get_world_transform()
assert unreal.load_asset("/Game/Space/Data/Planets/Moon/PDA_MoonSurfaceGameplay").get_editor_property("moon_ant_nest_count")==9
rows=[]
for item in plan["placements"]:
    nest=actors[item["name"]]
    point=transform.transform_location(unreal.Vector(*item["point_local_cm"]))
    assert (nest.get_actor_location()-point).length()<.01
    assert {c.get_name() for c in nest.get_components_by_class(unreal.ActorComponent)}=={"SceneRoot","NestMesh","EntranceShape"}
    visual=nest.get_editor_property("nest_mesh")
    assert visual.get_editor_property("relative_scale3d")==unreal.Vector(1,1,1)
    assert visual.static_mesh.get_name()==item["mesh"]
    floor_normal=transform.rotation.rotate_vector(unreal.Vector(*item["up_local"]))
    assert unreal.MathLibrary.dot_vector_vector(nest.get_actor_up_vector(),floor_normal)>.999999
    wall_indices=source["triangles"][item["wall_triangle"]]
    wall_points=[unreal.Vector(*source["vertices_local_cm"][str(i)]) for i in wall_indices]
    wall_normal=unreal.MathLibrary.normal(unreal.MathLibrary.cross_vector_vector(wall_points[1]-wall_points[0],wall_points[2]-wall_points[0]))
    if unreal.MathLibrary.dot_vector_vector(wall_normal,wall_points[0])<0:wall_normal=-wall_normal
    wall_normal=transform.rotation.rotate_vector(wall_normal)
    description=visual.static_mesh.get_static_mesh_description(0)
    void_vertices=set()
    slots=list(visual.static_mesh.static_materials)
    for i in range(description.get_triangle_count()):
        triangle=unreal.TriangleID(i)
        group=description.get_polygon_polygon_group(description.get_triangle_polygon(triangle)).id_value
        if str(slots[group].material_slot_name)!="M_AntNestVoid":continue
        void_vertices.update(description.get_vertex_instance_vertex(description.get_triangle_vertex_instance(triangle,j)).id_value for j in range(3))
    assert void_vertices
    gaps=[]
    wall_gaps=[]
    for i in void_vertices:
        local=description.get_vertex_position(unreal.VertexID(i))
        world=visual.get_world_transform().transform_location(local)
        delta=world-point
        wall_gap=unreal.MathLibrary.dot_vector_vector(delta,wall_normal)
        floor_gap=unreal.MathLibrary.dot_vector_vector(delta,floor_normal)
        gaps.append(min(abs(wall_gap),abs(floor_gap)))
        if local.z>1:wall_gaps.append(wall_gap)
    assert wall_gaps and min(wall_gaps)>.20 and max(wall_gaps)<.30,(item["name"],wall_gaps)
    assert max(gaps)<.30,(item["name"],gaps)
    rows.append({"name":item["name"],"root_on_foot_edge":True,"single_nest_actor":True,
                 "void_vertex_count":len(void_vertices),"maximum_surface_gap_cm":max(gaps),
                 "wall_mouth_gap_cm":[min(wall_gaps),max(wall_gaps)]})
report={"nest_count":9,"old_corpse_anchor_absent":True,"controller_placed_refs":9,
        "void_surface_fit_maximum_cm":max(row["maximum_surface_gap_cm"] for row in rows),"nests":rows}
(root/"moon_ant_nests_geometry_validation.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
print("ANT_ENTRANCE_GEOMETRY",json.dumps(report))
