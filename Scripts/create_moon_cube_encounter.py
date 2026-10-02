"""Author the Moon cube encounter in both SpaceWorld maps. Run in Unreal's Python commandlet."""
import unreal

MATERIAL_PATH = "/Game/Space/Materials/Moon"
ANCHOR = unreal.Vector(50700.0, 96500.0, 32000.0)


def make_material(name, base, emissive, roughness, metallic):
    path = MATERIAL_PATH + "/" + name
    material = unreal.load_asset(path)
    if material:
        return material
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, MATERIAL_PATH, unreal.Material, unreal.MaterialFactoryNew())
    assert material, "Could not create " + path
    for prop, value, node_class, y in (
        (unreal.MaterialProperty.MP_BASE_COLOR, base, unreal.MaterialExpressionVectorParameter, 0),
        (unreal.MaterialProperty.MP_EMISSIVE_COLOR, emissive, unreal.MaterialExpressionVectorParameter, 160),
        (unreal.MaterialProperty.MP_ROUGHNESS, roughness, unreal.MaterialExpressionScalarParameter, 320),
        (unreal.MaterialProperty.MP_METALLIC, metallic, unreal.MaterialExpressionScalarParameter, 480),
    ):
        node = unreal.MaterialEditingLibrary.create_material_expression(material, node_class, -340, y)
        assert node
        node.set_editor_property("parameter_name", str(prop).replace("MaterialProperty.MP_", ""))
        node.set_editor_property("default_value", value)
        unreal.MaterialEditingLibrary.connect_material_property(node, "", prop)
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


body = make_material(
    "M_MoonCubeBody", unreal.LinearColor(0.035, 0.105, 0.17, 1.0),
    unreal.LinearColor(0.015, 0.09, 0.17, 1.0), 0.38, 0.45)
crown = make_material(
    "M_MoonCubeWeakPoint", unreal.LinearColor(0.9, 0.35, 0.025, 1.0),
    unreal.LinearColor(5.0, 1.25, 0.08, 1.0), 0.22, 0.25)

cluster_class = unreal.load_class(None, "/Script/space.JTSMoonCubeCluster")
cube_mesh = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
assert cluster_class
assert cube_mesh
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for level_name in ("L_SpaceWorld", "L_SpaceWorld_Authoring"):
    map_path = "/Game/Space/Maps/" + level_name
    unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    actors = actor_subsystem.get_all_level_actors()
    suns = [a for a in actors if a.get_actor_label() == "Sun_Main"]
    controllers = [a for a in actors if a.get_actor_label() == "BP_MoonSurfaceController"]
    clusters = [a for a in actors if a.get_actor_label() == "Moon_SunlitCubeCluster"]
    assert len(suns) == 1 and len(controllers) == 1, map_path
    cluster = clusters[0] if clusters else actor_subsystem.spawn_actor_from_class(cluster_class, ANCHOR)
    assert cluster
    cluster.set_actor_label("Moon_SunlitCubeCluster")
    cluster.set_actor_location(ANCHOR, False, False)
    cluster.set_editor_property("sun_light", suns[0])
    cluster.set_editor_property("enemy_count", 20)
    cluster.set_editor_property("spawn_radius", 670.0)
    cluster.set_editor_property("minimum_sun_dot", 0.20)
    cluster.set_editor_property("body_mesh_asset", cube_mesh)
    cluster.set_editor_property("weak_point_mesh_asset", cube_mesh)
    cluster.set_editor_property("body_material", body)
    cluster.set_editor_property("weak_point_material", crown)
    controllers[0].set_editor_property("moon_cube_cluster", cluster)
    assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True), map_path
    unreal.log("MOON_CUBE_AUTHORED map={} cluster={} sun={} controller={}".format(
        map_path, cluster.get_actor_location(), suns[0].get_actor_label(), controllers[0].get_actor_label()))
