import unreal, json
from pathlib import Path
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
earth=next(a for a in actors if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='EarthFragments')
actor=earth.get_gameplay_surface_actor()
mesh=actor.static_mesh_component.static_mesh
dm=unreal.DynamicMesh()
dm,outcome=unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(mesh,dm,unreal.GeometryScriptCopyMeshFromAssetOptions(),unreal.GeometryScriptMeshReadLOD())
parts={}
box=mesh.get_bounding_box()
centre=(box.min+box.max)*0.5
scale=actor.get_actor_scale3d().x
for i in range(unreal.GeometryScript_MeshQueries.get_num_triangle_i_ds(dm)):
    valid,a,b,c=unreal.GeometryScript_MeshQueries.get_triangle_positions(dm,i)
    if not valid: continue
    mid,valid=unreal.GeometryScript_Materials.get_triangle_material_id(dm,i)
    for p in (a,b,c):
        p=(p-centre)*scale
        parts.setdefault(str(mid),[]).append([p.x,p.y,p.z])
report=dict(parts=parts,scale=scale,rotation=str(actor.get_actor_rotation()))
Path(unreal.Paths.project_saved_dir(),'EarthFragments/envelope_source.json').write_text(json.dumps(report),encoding='utf-8')
print('ENVELOPE_EXPORTED',len(parts),scale)
