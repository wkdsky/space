"""Read-only audit of the Blueprint hull, movement proxy and camera templates."""
import unreal

ship_class = unreal.load_class(None, "/Game/Space/Blueprints/Ships/BP_Spacecraft.BP_Spacecraft_C")
ship = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
    ship_class, unreal.Vector(0, 0, 100000)
)
try:
    for component in ship.get_components_by_class(unreal.SceneComponent):
        unreal.log("SHIP_AUDIT {} location={} rotation={} scale={}".format(
            component.get_name(), component.get_editor_property("relative_location"),
            component.get_editor_property("relative_rotation"), component.get_editor_property("relative_scale3d")))
        if isinstance(component, unreal.StaticMeshComponent):
            mesh = component.get_editor_property("static_mesh")
            unreal.log("SHIP_AUDIT mesh={} bounds={} collision={}".format(
                mesh.get_path_name() if mesh else None, mesh.get_bounds() if mesh else None,
                component.get_collision_enabled()))
        if isinstance(component, unreal.BoxComponent):
            unreal.log("SHIP_AUDIT box_extent={}".format(component.get_unscaled_box_extent()))
    hull = ship.get_component_by_class(unreal.BoxComponent)
    extent = hull.get_scaled_box_extent()
    assert extent.x >= 524.9 and extent.y >= 499.9 and extent.z >= 199.9, "Hull does not enclose the configured model"
    unreal.log("SHIP_AUDIT PASS scaled_hull_extent={}".format(extent))
    # Python commandlet map loads do not reliably create collision bodies. Real terrain queries
    # are covered by JTS.Spacecraft automation worlds and the L_SpaceWorld -game smoke run.
finally:
    unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(ship)
