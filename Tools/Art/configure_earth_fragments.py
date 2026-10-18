"""Author the supplied shattered Earth as a playable body in L_SpaceWorld.

Run in the editor with: py D:/projects/space/Tools/Art/configure_earth_fragments.py
The Moon reference radius sets the full Earth size and apparent separation.
The level corridor is compact; reciprocal PlanetAnchor settings reuse the
existing runtime shrink curve. Real fragment collision retains the empty gaps.
"""
import json
import math
from pathlib import Path
import unreal

FOLDER = '/Game/Space/Environment/EarthFragments'
MESH = FOLDER + '/SM_EarthFragments'
LABEL = 'EarthFragments_NearMoon'
EARTH_RADIUS_KM = 6371.0
MOON_RADIUS_KM = 1737.4
EARTH_MOON_DISTANCE_KM = 384400.0
EARTH_SIZE_MULTIPLIER = 2.0
LOCAL_FREE_FLIGHT_CORRIDOR_CM = 160000.0
EA = unreal.EditorAssetLibrary


def vec(p):
    return [p.x, p.y, p.z]


def normalized(p):
    return p / p.length()


def configure():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    assert not editor.get_game_world(), 'Stop PIE before authoring.'
    assert editor.get_editor_world().get_path_name() == '/Game/Space/Maps/L_SpaceWorld.L_SpaceWorld'
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = subsystem.get_all_level_actors()
    moon = next(a for a in actors if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'Moon')
    landing = next(a for a in actors if a.get_actor_label() == 'Moon_LandingAnchor')
    radius = moon.get_editor_property('approximate_radius')
    center = moon.get_planet_center()
    diameter = radius * 2.0 * EARTH_RADIUS_KM / MOON_RADIUS_KM * EARTH_SIZE_MULTIPLIER
    apparent_distance = radius * EARTH_MOON_DISTANCE_KM / MOON_RADIUS_KM
    direction = normalized(unreal.Vector(0.55, 0.70, 0.45))
    landing_up = normalized(landing.get_actor_location() - center)
    # Keep the Moon's authored terrain/POIs fixed. Choose a static orbital frame
    # from its existing pole; Earth's pole has approximately 23.44 deg obliquity.
    moon_visual = moon.get_gameplay_surface_actor()
    moon_pole = moon_visual.get_actor_up_vector()
    orbit_normal = normalized(moon_pole - direction * unreal.MathLibrary.dot_vector_vector(moon_pole, direction))
    tilt_direction = normalized(landing_up - orbit_normal * unreal.MathLibrary.dot_vector_vector(landing_up, orbit_normal))
    obliquity = math.radians(23.44)
    local_z = normalized(orbit_normal * math.cos(obliquity) + tilt_direction * math.sin(obliquity))
    # Detailed front remains facing the Moon as closely as that axial tilt permits.
    local_y = normalized(direction - local_z * unreal.MathLibrary.dot_vector_vector(direction, local_z))
    local_x = unreal.MathLibrary.cross_vector_vector(local_y, local_z)
    rotation = unreal.MathLibrary.make_rotation_from_axes(local_x, local_y, local_z)
    mesh = unreal.load_asset(MESH)
    if not mesh:
        source = Path(unreal.Paths.project_dir()).resolve() / 'earth fragments 3d model/earth+fragments+3d+model.fbx'
        options = unreal.FbxImportUI()
        options.automated_import_should_detect_type = False
        options.import_as_skeletal = False
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        options.import_mesh = True
        options.import_materials = True
        options.import_textures = True
        options.static_mesh_import_data.combine_meshes = True
        options.static_mesh_import_data.auto_generate_collision = False
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = FOLDER
        task.destination_name = 'SM_EarthFragments'
        task.automated = True
        task.save = False
        task.options = options
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        mesh = unreal.load_asset(MESH)
    assert mesh, 'Earth FBX import failed.'
    box = mesh.get_bounding_box()
    size = box.max - box.min
    # This generated asset has no surviving whole globe or rest pose. Treat its
    # complete authored debris envelope as the conservative Earth diameter.
    source_diameter = max(size.x, size.y, size.z)
    source_center = (box.min + box.max) * 0.5
    scale = diameter / source_diameter
    q = rotation.quaternion()
    axes = [q.rotate_vector(unreal.Vector(size.x * scale * 0.5, 0, 0)),
            q.rotate_vector(unreal.Vector(0, size.y * scale * 0.5, 0)),
            q.rotate_vector(unreal.Vector(0, 0, size.z * scale * 0.5))]
    earth_visual_radius = max(sum(abs(getattr(v, axis)) for v in axes) for axis in ('x', 'y', 'z'))
    release = 8000.0 * (math.exp(14.0 / 12.0) - 1.0)
    # Preserve each body's original near-flight shell, then leave a scale/growth
    # corridor between them. Travel time is not a layout constraint.
    distance = moon.get_visual_radius() + earth_visual_radius + 2.0 * release + LOCAL_FREE_FLIGHT_CORRIDOR_CM
    existing_earth = next((a for a in actors if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'EarthFragments'), None)
    earth_center = existing_earth.get_planet_center() if existing_earth else center + direction * distance
    distance = (earth_center - center).length()
    location = earth_center - rotation.quaternion().rotate_vector(source_center * scale)
    mel = unreal.MaterialEditingLibrary
    materials = [s.material_interface for s in mesh.static_materials]
    assert len(materials) == 67 and all(materials), 'Expected all 67 supplied parts and material slots.'
    for mat in materials:
        assert isinstance(mat, unreal.Material), mat
        # A small fill preserves the supplied ocean, continent and hot-interior
        # colors in the existing dark space lighting without a new scene light.
        expression = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
        assert expression, 'Missing original base-color texture: ' + mat.get_path_name()
        if not mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR):
            fill = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 0, 200)
            fill.set_editor_property('const_b', 0.35)
            mel.connect_material_expressions(expression, 'RGB', fill, 'A')
            mel.connect_material_property(fill, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            mel.recompile_material(mat)
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(mesh)
    body_setup = mesh.get_editor_property('body_setup')
    body_setup.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    body_setup.set_editor_property('double_sided_geometry', True)
    # The original 67 parts remain disconnected triangles. A single convex hull
    # would seal the gaps and is deliberately not generated.
    mesh_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    mesh_editor.set_lod_build_settings(mesh, 0, mesh_editor.get_lod_build_settings(mesh, 0))
    actor = next((a for a in actors if a.get_actor_label() == LABEL), None)
    with unreal.ScopedEditorTransaction('Place shattered Earth beside Moon'):
        if not actor:
            actor = subsystem.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
        actor.modify()
        actor.set_actor_label(LABEL)
        actor.set_folder_path('Planets/EarthFragments')
        actor.tags = ['EarthFragments', 'MoonSkyVisual']
        component = actor.static_mesh_component
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_static_mesh(mesh)
        component.set_collision_profile_name('BlockAll')
        component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        component.set_editor_property('generate_overlap_events', False)
        component.set_editor_property('can_ever_affect_navigation', False)
        component.set_cast_shadow(False)
        component.set_editor_property('min_draw_distance', 0.0)
        component.set_editor_property('ld_max_draw_distance', 0.0)
        component.set_editor_property('never_distance_cull', True)
        actor.set_actor_location(location, False, False)
        actor.set_actor_rotation(rotation, False)
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        earth = next((a for a in actors if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'EarthFragments'), None)
        if not earth:
            earth = subsystem.spawn_actor_from_class(unreal.JTSPlanetAnchor, earth_center)
        earth.modify()
        earth.set_actor_label('EarthFragments_PlanetAnchor')
        earth.set_folder_path('Planets/EarthFragments')
        earth.set_actor_location(earth_center, False, False)
        earth.set_editor_property('planet_id', 'EarthFragments')
        earth.set_editor_property('approximate_radius', diameter * 0.5)
        earth.set_editor_property('gameplay_surface_actor', actor)
        earth.set_editor_property('enable_gravity', True)
        for property_name in ('gravity_strength', 'gravity_influence_range', 'space_exit_range',
                              'takeoff_transition_altitude', 'space_flight_altitude', 'landing_assist_altitude'):
            earth.set_editor_property(property_name, moon.get_editor_property(property_name))
        earth.set_editor_property('flight_collision_clearance', 0.0)
        earth.set_editor_property('fragmented_gameplay_surface', True)
        earth.set_editor_property('heliocentric_distance_kilometers', moon.get_heliocentric_distance_kilometers())
        moon.modify()
        moon.set_editor_property('parent_planet_id', 'EarthFragments')
        for body, companion in [(moon, earth), (earth, moon)]:
            body.set_editor_property('celestial_companion', companion)
            body.set_editor_property('companion_apparent_separation_centimeters', apparent_distance)
            body.set_editor_property('companion_distant_size_multiplier', 2.0 if body == moon else 1.0)
        assert distance > moon.get_visual_radius() + earth.get_visual_radius() + 2.0 * release, 'Local full-size bands must not overlap.'
    for path in EA.list_assets(FOLDER, recursive=True, include_folder=False):
        assert EA.save_asset(path, only_if_is_dirty=False), path
    # Keep the rounded fragment geometry in a separate authored DataAsset. Its
    # full-size coordinates remain independent of the distant presentation scale.
    import runpy
    runpy.run_path(str(Path(__file__).with_name('configure_fragment_envelope.py')))['configure']()
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    report = dict(actor=actor.get_path_name(), mesh=MESH, moon_radius_cm=radius,
                  earth_envelope_diameter_cm=diameter, center_distance_cm=distance,
                  earth_size_multiplier=EARTH_SIZE_MULTIPLIER, fragment_flight_clearance_cm=0.0,
                  fragment_envelope_asset=FOLDER + '/DA_EarthFragmentsEnvelope',
                  moon_near_flight_physical_altitude_cm=release, free_flight_corridor_cm=LOCAL_FREE_FLIGHT_CORRIDOR_CM,
                  apparent_center_distance_cm=apparent_distance,
                  earth_moon_diameter_ratio=diameter / (2 * radius),
                  earth_angular_diameter_from_moon_degrees=math.degrees(2 * math.atan((diameter / 2) / (apparent_distance - radius))),
                  moon_angular_diameter_from_earth_degrees=math.degrees(2 * math.atan(2.0 * radius / (apparent_distance - diameter / 2))),
                  moon_distant_size_multiplier=2.0, fragmented_gameplay_surface=True,
                  normal_flight_speed_cm_s=4200.0,
                  straight_surface_to_fragments_seconds=(distance - radius - diameter/2) / 4200.0,
                  earth_anchor=earth.get_path_name(),
                  earth_axis_obliquity_degrees=23.44,
                  earth_center_cm=vec(earth_center), actor_location_cm=vec(location),
                  rotation=dict(pitch=rotation.pitch, yaw=rotation.yaw, roll=rotation.roll),
                  uniform_scale=scale, source_envelope_center_cm=vec(source_center),
                  material_slots=len(materials), triangles=mesh.get_num_triangles(0),
                  scale_reference='Complete supplied debris envelope; original intact diameter is unavailable.',
                  source_reference='https://nssdc.gsfc.nasa.gov/planetary/factsheet/moonfact.html')
    root = Path(unreal.Paths.project_saved_dir()).resolve() / 'EarthFragments'
    root.mkdir(parents=True, exist_ok=True)
    (root / 'placement.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('EARTH_FRAGMENTS_PLACED', json.dumps(report))
    return report


if __name__ == '__main__':
    configure()
