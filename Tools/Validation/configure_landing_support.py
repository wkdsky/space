"""Editor Python: calibrate ship landing feet from the authored deployed rig and migrate SpaceWorld."""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
ASSET = '/Game/Space/Ships/TwinforkR1/BP_Twinfork_R1'
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not unreal.EditorLevelLibrary.get_pie_worlds(True), 'Stop PIE before authoring landing defaults'
assert unreal.EditorLevelLibrary.get_editor_world().get_path_name() == '/Game/Space/Maps/L_SpaceWorld.L_SpaceWorld', 'Open L_SpaceWorld before migrating landing volumes'
blueprint = unreal.load_asset(ASSET)
cls = unreal.load_class(None, ASSET + '.BP_Twinfork_R1_C')
actor = sub.spawn_actor_from_class(cls, unreal.Vector(), transient=True)
def v(x): return [x.x, x.y, x.z]
out = ROOT / 'Docs/Validation/spacecraft_landing_configuration.json'
previous = json.loads(out.read_text(encoding='utf-8')) if out.exists() else {}
report = dict(feet=[], removed_sites=previous.get('removed_sites', []), arrival_anchors=[], former_landing_volumes=previous.get('former_landing_volumes', []))
try:
    definitions = []
    for foot in actor.get_components_by_class(unreal.StaticMeshComponent):
        if not foot.get_name().endswith('_Foot'):
            continue
        shin = foot.get_attach_parent()
        box = foot.static_mesh.get_bounding_box()
        desc = foot.static_mesh.get_static_mesh_description(0)
        bottom_area = 0.0
        bottom_triangles = 0
        for index in range(desc.get_triangle_count()):
            triangle = unreal.TriangleID(index)
            vertices = [desc.get_vertex_position(desc.get_vertex_instance_vertex(desc.get_triangle_vertex_instance(triangle, corner))) for corner in range(3)]
            if all(abs(p.z - box.min.z) < 0.01 for p in vertices):
                cross = unreal.MathLibrary.cross_vector_vector(vertices[1]-vertices[0], vertices[2]-vertices[0])
                bottom_area += cross.length()/2
                bottom_triangles += 1
        assert bottom_area >= (box.max.x-box.min.x)*(box.max.y-box.min.y), 'Foot mesh has no complete flat sole'
        # The measured Twinfork sole is 56 x 40 cm, on local Z=-18. Inset 2 cm at each edge.
        sole = unreal.Vector((box.min.x + box.max.x)/2, (box.min.y + box.max.y)/2, box.min.z)
        frame = foot.get_world_transform()
        frame.translation = frame.transform_location(sole)
        lower_bounds = shin.static_mesh.get_bounding_box()
        d = unreal.JTSLandingFootDefinition()
        d.foot_component_name = foot.get_name()
        d.strut_component_name = shin.get_name()
        d.sole_centre_local = sole
        d.strut_rest_length = abs(lower_bounds.min.z)
        d.deployed_contact_frame = frame
        d.pad_half_extent = unreal.Vector2D((box.max.x-box.min.x)/2-2, (box.max.y-box.min.y)/2-2)
        d.max_compression = 18
        d.max_extension = 32
        definitions.append(d)
        report['feet'].append(dict(foot=foot.get_name(), strut=shin.get_name(), root_contact=v(frame.translation), sole_local=v(sole), half_extent=[d.pad_half_extent.x,d.pad_half_extent.y], rest_length=d.strut_rest_length, flat_sole_triangles=bottom_triangles, flat_sole_area=bottom_area))
    assert len(definitions) == 4
    support = unreal.get_default_object(cls).get_editor_property('landing_support_component')
    support.set_editor_property('feet', definitions)
    support.set_editor_property('settings', unreal.JTSLandingSupportSettings())
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
finally:
    sub.destroy_actor(actor)

# Remove every obsolete volume from the shared universe; leave both first-arrival objects untouched.
for a in sub.get_all_level_actors():
    if isinstance(a, unreal.JTSPlanetArrivalAnchor):
        report['arrival_anchors'].append(dict(name=a.get_name(),label=a.get_actor_label(),location=v(a.get_actor_location()),rotation=str(a.get_actor_rotation()),ship_location=v(a.get_spacecraft_arrival_transform().translation)))
    elif isinstance(a, unreal.JTSPlanetLandingSite):
        if a.get_actor_label() not in report['removed_sites']: report['removed_sites'].append(a.get_actor_label())
        for box in a.get_components_by_class(unreal.BoxComponent):
            pose = box.get_world_transform()
            report['former_landing_volumes'].append(dict(label=a.get_actor_label(), location=v(pose.translation), rotation_quat=[pose.rotation.x,pose.rotation.y,pose.rotation.z,pose.rotation.w],scale=v(pose.scale3d),default_half_extent=v(box.get_unscaled_box_extent())))
        sub.destroy_actor(a)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report))
