"""Replace the two Mars moon assets with project-supplied FBXs.

Use mars_moons_mcp.py in the project's Unreal editor. Reports go to
Saved/MoonModels. Standalone inspect can also run in a PythonScript commandlet.
Only asset data and authored level instances are changed; no gameplay rules.
"""
import json
import math
import os
from pathlib import Path
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/MoonModels'
OUT.mkdir(parents=True, exist_ok=True)
EA = unreal.EditorAssetLibrary
REG = unreal.AssetRegistryHelpers.get_asset_registry()
REG.search_all_assets(True)
MAPS = ['/Game/Space/Maps/L_SpaceWorld', '/Game/Space/Maps/L_SpaceWorld_Authoring']
FOLDER = '/Game/Space/Environment/MarsSystem'
SOURCES = {
    'MarI': ('MarI 3d model/asteroid_rock_3d_model.fbx',
             'MarI 3d model/asteroid_rock_3d_model.fbm/asteroid_rock_3d_model_basecolor.jpg'),
    'MarII': ('MarII 3d model/rock_3d_model.fbx',
              'MarII 3d model/rock_3d_model.fbm/rock_3d_model_basecolor.jpg'),
}
LEGACY = ['/Game/ThirdParty/MarII/MarII__2_',
          '/Game/ThirdParty/Environment/UltimateSpace/Meshes/MarII']


def vec(v):
    return [v.x, v.y, v.z]


def save(asset):
    assert EA.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def refs(path):
    return [str(x) for x in REG.get_referencers(path, unreal.AssetRegistryDependencyOptions())]


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def mesh_info(mesh):
    bounds = mesh.get_bounding_box()
    return dict(path=mesh.get_path_name(), triangles=mesh.get_num_triangles(0),
                min=vec(bounds.min), max=vec(bounds.max),
                materials=[str(m.material_interface.get_path_name()) if m.material_interface else None
                           for m in mesh.static_materials],
                import_files=list(mesh.get_editor_property('asset_import_data').extract_filenames()),
                collision=str(mesh.get_editor_property('body_setup').get_editor_property('collision_trace_flag')))


def inspect():
    result = {'assets': {}, 'maps': {}}
    for path in [FOLDER + '/MarI/SM_MarI', *LEGACY]:
        mesh = unreal.load_asset(path)
        if mesh:
            result['assets'][path] = dict(**mesh_info(mesh), referencers=refs(path))
    for path in EA.list_assets('/Game/ThirdParty/MarII', recursive=True, include_folder=False):
        package = path.split('.')[0]
        result['assets'].setdefault(package, dict(referencers=refs(package)))
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    for path in MAPS:
        assert level.load_level(path)
        rows = []
        for a in actors():
            if isinstance(a, unreal.JTSPlanetAnchor):
                surface = a.get_gameplay_surface_actor()
                rows.append(dict(label=a.get_actor_label(), id=str(a.get_planet_id()),
                                 center=vec(a.get_planet_center()), radius=a.get_approximate_radius(),
                                 surface=surface.get_path_name() if surface else None))
            elif isinstance(a, unreal.StaticMeshActor):
                mesh = a.static_mesh_component.static_mesh
                if mesh and ('MarI' in mesh.get_path_name()):
                    center, extent = a.get_actor_bounds(False)
                    rows.append(dict(label=a.get_actor_label(), path=a.get_path_name(),
                                     mesh=mesh.get_path_name(), location=vec(a.get_actor_location()),
                                     rotation=str(a.get_actor_rotation()), scale=vec(a.get_actor_scale3d()),
                                     center=vec(center), extent=vec(extent),
                                     overrides=[str(x) for x in a.static_mesh_component.get_editor_property('override_materials')]))
            elif isinstance(a, (unreal.JTSPlanetArrivalAnchor, unreal.JTSPlanetSurfaceAnchor)):
                rows.append(dict(label=a.get_actor_label(), path=a.get_path_name(), location=vec(a.get_actor_location()),
                                 rotation=str(a.get_actor_rotation())))
        result['maps'][path] = rows
    return result


def import_models():
    at = unreal.AssetToolsHelpers.get_asset_tools()
    sme = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    result = {'assets': {}}
    for key, (fbx, jpg) in SOURCES.items():
        path = FOLDER + '/' + key
        options = unreal.FbxImportUI()
        options.automated_import_should_detect_type = False
        options.import_as_skeletal = False
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        options.import_mesh = True
        options.import_materials = False
        options.import_textures = False
        options.static_mesh_import_data.combine_meshes = True
        options.static_mesh_import_data.auto_generate_collision = False
        for filename, name, opts in [(fbx, 'SM_' + key, options), (jpg, 'T_' + key + '_BaseColor', None)]:
            task = unreal.AssetImportTask()
            task.filename = str(ROOT / filename)
            task.destination_path = path
            task.destination_name = name
            task.automated = True
            task.replace_existing = True
            task.replace_existing_settings = True
            task.save = False
            if opts:
                task.factory = unreal.FbxFactory()
                task.options = opts
            at.import_asset_tasks([task])
            assert task.imported_object_paths, filename
        mesh = unreal.load_asset(path + '/SM_' + key)
        texture = unreal.load_asset(path + '/T_' + key + '_BaseColor')
        assert mesh and texture
        texture.set_editor_property('max_texture_size', 2048)
        texture.set_editor_property('srgb', True)
        material_path = path + '/M_' + key + '_Surface'
        material = unreal.load_asset(material_path)
        if not material:
            material = EA.duplicate_asset(FOLDER + '/MarI/M_MarI_Surface', material_path)
        assert material
        sample = unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
        assert isinstance(sample, unreal.MaterialExpressionTextureSample)
        sample.set_editor_property('texture', texture)
        unreal.MaterialEditingLibrary.recompile_material(material)
        assert len(mesh.static_materials) == 1
        mesh.set_material(0, material)
        sme.remove_collisions(mesh)
        body = mesh.get_editor_property('body_setup')
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if key == 'MarI' else unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
        body.set_editor_property('double_sided_geometry', key == 'MarI')
        sme.set_lod_build_settings(mesh, 0, sme.get_lod_build_settings(mesh, 0))
        for asset in (texture, material, mesh):
            save(asset)
        result['assets'][key] = mesh_info(mesh)
    result['scene'] = scan()
    return result


def scan():
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    result = {'maps': {}}
    for path in MAPS:
        assert level.load_level(path)
        rows = []
        for a in actors():
            row = dict(label=a.get_actor_label(), cls=a.get_class().get_name(), path=a.get_path_name(), location=vec(a.get_actor_location()))
            if isinstance(a, unreal.JTSPlanetAnchor):
                row['properties'] = {key: str(a.get_editor_property(key)) for key in ['planet_id', 'landing_anchor_actor', 'approach_entry_anchor_actor', 'center_actor', 'celestial_background_actors', 'background_navigation', 'celestial_background_companion', 'celestial_companion']}
            if isinstance(a, unreal.JTSSpaceWorldManager):
                compressed = a.get_editor_property('compressed_space')
                row['compressed'] = str(compressed.get_editor_property('bodies'))
                row['initial_planet'] = str(a.get_editor_property('initial_planet'))
            rows.append(row)
        result['maps'][path] = rows
    bp = unreal.load_asset('/Game/Space/Blueprints/Modes/BP_SpaceWorldGameMode')
    result['controllers'] = str(unreal.get_default_object(bp.generated_class()).get_editor_property('surface_gameplay_controllers'))
    result['support_api'] = unreal.JTSSpacecraftActor.get_landing_support_component.__doc__
    return result


def set_properties(obj, values):
    obj.modify()
    for key, value in values.items():
        obj.set_editor_property(key, value)


def fit_visual(actor, mesh, old):
    """Preserve the authored world bounds centre and largest extent across FBX units/pivots."""
    actor.modify()
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh)
    comp.set_editor_property('override_materials', [])
    actor.set_actor_scale3d(unreal.Vector(1, 1, 1))
    _, extent = actor.get_actor_bounds(False)
    scale = max(old['extent']) / max(vec(extent))
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    center = actor.get_actor_bounds(False)[0]
    actor.set_actor_location(actor.get_actor_location() + unreal.Vector(*old['center']) - center, False, True)
    assert (actor.get_actor_bounds(False)[0] - unreal.Vector(*old['center'])).length() < .1
    return dict(label=actor.get_actor_label(), mesh=mesh.get_path_name(), center=vec(actor.get_actor_bounds(False)[0]),
                extent=vec(actor.get_actor_bounds(False)[1]), scale=scale)


def surface_collision(actor, enabled):
    actor.modify()
    actor.set_actor_enable_collision(enabled)
    comp = actor.static_mesh_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_collision_profile_name('BlockAll' if enabled else 'NoCollision')
    comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if enabled else unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property('generate_overlap_events', False)
    comp.set_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA, unreal.CollisionResponseType.ECR_IGNORE)


def configure_mars(placed, deimos, visual):
    es = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    center, extent = visual.get_actor_bounds(False)
    radius = max(vec(extent))
    mars = next((a for a in placed if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'Mars'), None)
    mars = mars or es.spawn_actor_from_class(unreal.JTSPlanetAnchor, center)
    mars.set_actor_label('Mars_PlanetAnchor')
    mars.set_folder_path('Planets/Mars')
    set_properties(mars, dict(planet_id='Mars', navigation_display_name=unreal.Text('火星 / Mars'),
        approximate_radius=radius, gameplay_surface_actor=visual, gameplay_surface_component=visual.static_mesh_component,
        gravity_strength=375.0, gravity_influence_range=radius+20000.0, space_exit_range=radius+22000.0,
        heliocentric_distance_kilometers=227940000.0, parent_planet_id='None',
        takeoff_transition_altitude=17690.0, space_flight_altitude=17690.0, approach_transition_altitude=17690.0,
        near_flight_reference_radius=21000.0, near_flight_size_exponent=.5, near_flight_max_speed_multiplier=3.0,
        cruise_approach_exponent_override=.1, navigation_palette_index=1, navigation_marker_range_kilometers=200000.0,
        bShowOnStarChart=True, bShowScreenMarker=True))
    visual.set_actor_label('Mars_Surface')
    visual.set_editor_property('tags', [unreal.Name('Mars')])
    surface_collision(visual, True)
    mesh = visual.static_mesh_component.static_mesh
    body = mesh.get_editor_property('body_setup')
    set_properties(body, dict(collision_trace_flag=unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE, double_sided_geometry=True))
    sme = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    sme.set_lod_build_settings(mesh, 0, sme.get_lod_build_settings(mesh, 0))
    save(mesh)
    return mars


def configure_deimos(placed, visual):
    es = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    center, extent = visual.get_actor_bounds(False)
    radius = max(vec(extent))
    planet = next((a for a in placed if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'MarII'), None)
    planet = planet or es.spawn_actor_from_class(unreal.JTSPlanetAnchor, center)
    planet.set_actor_label('MarII_PlanetAnchor')
    planet.set_folder_path('Planets/MarII')
    planet.set_actor_location(center, False, True)
    set_properties(planet, dict(planet_id='MarII', navigation_display_name=unreal.Text('火卫二 / Deimos'),
        approximate_radius=radius, gameplay_surface_actor=visual, gameplay_surface_component=visual.static_mesh_component,
        gravity_strength=980.0, gravity_influence_range=radius+20000.0, space_exit_range=radius+22000.0,
        heliocentric_distance_kilometers=227940000.0, parent_planet_id='None',
        takeoff_transition_altitude=17690.0, space_flight_altitude=17690.0, approach_transition_altitude=17690.0,
        near_flight_reference_radius=21000.0, near_flight_size_exponent=.5, near_flight_max_speed_multiplier=3.0,
        cruise_approach_exponent_override=.1, bShowOnStarChart=False, bShowScreenMarker=False))
    visual.set_editor_property('tags', [unreal.Name('MarII')])
    surface_collision(visual, True)
    mesh = visual.static_mesh_component.static_mesh
    body = mesh.get_editor_property('body_setup')
    set_properties(body, dict(collision_trace_flag=unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE, double_sided_geometry=True))
    sme = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    sme.set_lod_build_settings(mesh, 0, sme.get_lod_build_settings(mesh, 0))
    save(mesh)
    return planet


def choose_surface_landing(mars, key):
    """Use the production four-foot support fitter on the real spherical mesh."""
    es = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cls = unreal.load_class(None, '/Game/Space/Ships/TwinforkR1/BP_Twinfork_R1.BP_Twinfork_R1_C')
    ship = es.spawn_actor_from_class(cls, mars.get_planet_center() + unreal.Vector(0, 0, 200000))
    assert ship
    sun = next((a for a in actors() if isinstance(a, unreal.DirectionalLight)), None)
    light_dir = -sun.get_actor_forward_vector() if sun else unreal.Vector(0, 0, 1)
    directions = [light_dir]
    for i in range(96):
        z = 1 - 2*(i+.5)/96
        angle = i * math.pi * (3-math.sqrt(5))
        directions.append(unreal.Vector(math.sqrt(1-z*z)*math.cos(angle), math.sqrt(1-z*z)*math.sin(angle), z))
    directions.sort(key=lambda v: unreal.MathLibrary.dot_vector_vector(v, light_dir), reverse=True)
    selected = None
    attempts = 0
    try:
        for radial in directions:
            forward = unreal.MathLibrary.cross_vector_vector(radial, unreal.Vector(0, 1, 0)).normal()
            query = mars.get_planet_center() + radial * (mars.get_visual_radius() * 3)
            frame = mars.get_surface_frame_at(query, forward)
            if not frame:
                continue
            for heading in range(8):
                attempts += 1
                heading_forward = unreal.MathLibrary.rotate_angle_axis(frame.forward, heading*45, frame.up)
                pose = unreal.Transform(location=frame.location, rotation=unreal.MathLibrary.make_rot_from_xz(heading_forward, frame.up))
                if ship.snap_spacecraft_to_surface_transform(mars, pose):
                    selected = mars.get_surface_frame_at(query, heading_forward)
                    break
            if selected:
                break
        assert selected, 'No real %s surface position supports the production spacecraft' % key
        contacts = ship.get_landing_support_component().get_foot_contacts()
        assert len(contacts) == 4, 'Expected all four landing feet to contact real terrain'
        result = dict(surface=vec(selected.location), up=vec(selected.up), forward=vec(selected.forward),
                      fitted_ship=vec(ship.get_actor_location()), contacts=len(contacts), attempts=attempts)
    finally:
        es.destroy_actor(ship)
    placed = actors()
    landing = next((a for a in placed if a.get_actor_label() == key + '_LandingAnchor'), None)
    landing = landing or es.spawn_actor_from_class(unreal.JTSPlanetSurfaceAnchor, selected.location)
    landing.set_actor_label(key + '_LandingAnchor')
    set_properties(landing, dict(planet_anchor=mars, planet_id=key, surface_clearance=0.0))
    landing.set_actor_transform(selected.transform, False, True)
    assert landing.snap_to_planet_surface()
    arrival = next((a for a in placed if a.get_actor_label() == key + '_ArrivalAnchor'), None)
    arrival = arrival or es.spawn_actor_from_class(unreal.JTSPlanetArrivalAnchor, selected.location)
    arrival.set_actor_label(key + '_ArrivalAnchor')
    set_properties(arrival, dict(planet_anchor=mars, planet_id=key, priority=100))
    transform = selected.transform
    transform.translation = selected.location - selected.forward*900 + selected.up*150
    arrival.set_actor_transform(transform, False, True)
    radial = (selected.location - mars.get_planet_center()).normal()
    desired_ship_query = selected.location + radial*600
    ship_relative = arrival.get_actor_transform().inverse_transform_location(desired_ship_query)
    arrival.get_editor_property('spacecraft_arrival_point').set_relative_location(ship_relative, False, True)
    for a in (landing, arrival):
        a.set_folder_path('Planets/' + key)
    mars.set_editor_property('landing_anchor_actor', landing)
    result['arrival'] = vec(arrival.get_spacecraft_arrival_transform().translation)
    return result


def apply():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE first'
    before = json.loads((OUT / 'inspect.json').read_text(encoding='utf-8'))
    es = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    result = {'maps': {}}
    for path in MAPS:
        assert level.load_level(path)
        placed = actors()
        changes = dict(visuals=[], removed=[])
        for old in before['maps'][path]:
            if 'mesh' not in old:
                continue
            key = 'MarI' if '/MarI/SM_MarI.' in old['mesh'] else 'MarII'
            actor = next(a for a in placed if a.get_path_name() == old['path'])
            mesh = unreal.load_asset(FOLDER + '/' + key + '/SM_' + key)
            changes['visuals'].append(fit_visual(actor, mesh, old))
            surface_collision(actor, key == 'MarI')
            if key == 'MarII':
                actor.set_actor_label('MarII_Background')
                actor.set_editor_property('tags', [unreal.Name('MarII'), unreal.Name('CelestialBackground')])
        deimos = next((a for a in placed if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'MarII'), None)
        deimos_visual = next(a for a in actors() if a.get_actor_label() == 'MarII_Background')
        deimos = configure_deimos(placed, deimos_visual)
        changes['deimos_landing'] = choose_surface_landing(deimos, 'MarII')
        phobos = next((a for a in placed if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'MarI'), None)
        if phobos:
            changes['phobos_landing'] = choose_surface_landing(phobos, 'MarI')
        manager = next(a for a in placed if isinstance(a, unreal.JTSSpaceWorldManager))
        mars_visual = next((a for a in placed if isinstance(a, unreal.StaticMeshActor) and a.actor_has_tag('Mars')), None)
        if mars_visual:
            mars = configure_mars(placed, deimos, mars_visual)
            compressed = manager.get_editor_property('compressed_space')
            entries = compressed.get_editor_property('bodies')
            for i, entry in enumerate(entries):
                if entry.actor == mars_visual:
                    entry.actor = mars
                    interference = entry.get_editor_property('interference')
                    interference.set_editor_property('bEnabled', False)
                    entry.set_editor_property('interference', interference)
                elif entry.actor == deimos or entry.actor == deimos_visual:
                    entry.actor = deimos
                    entry.set_editor_property('bEnableNearFlight', True)
                entries[i] = entry
            set_properties(compressed, dict(bodies=entries))
            set_properties(manager, dict(initial_planet=mars, initial_planet_id='Mars'))
            backgrounds = [a for a in mars.get_editor_property('celestial_background_actors') if a != deimos_visual]
            set_properties(mars, dict(celestial_background_actors=backgrounds))
            changes['landing'] = choose_surface_landing(mars, 'Mars')
            for debug in [a for a in placed if isinstance(a, unreal.JTSSpaceDebugTravelManager)]:
                destinations = debug.get_editor_property('destinations')
                for i, dest in enumerate(destinations):
                    if str(dest.planet_id) == 'MarII':
                        dest.planet_id = 'Mars'
                        dest.display_name = unreal.Text('火星 / Mars')
                    destinations[i] = dest
                set_properties(debug, dict(destinations=destinations))
            compressed.capture_authored_bodies(False)
        elif deimos and manager.get_editor_property('initial_planet') == deimos:
            moon = next(a for a in placed if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'Moon')
            set_properties(manager, dict(initial_planet=moon, initial_planet_id='Moon'))
        assert level.save_current_level()
        result['maps'][path] = changes
    controller_path = '/Game/Space/Blueprints/Planets/Mars/BP_MarsPassiveSurfaceController'
    bp = unreal.load_asset(controller_path)
    if not bp:
        bp = EA.duplicate_asset('/Game/Space/Blueprints/Planets/MarI/BP_MarIPassiveSurfaceController', controller_path)
    set_properties(unreal.get_default_object(bp.generated_class()), dict(planet_id='Mars'))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(bp)
    gm_bp = unreal.load_asset('/Game/Space/Blueprints/Modes/BP_SpaceWorldGameMode')
    gm = unreal.get_default_object(gm_bp.generated_class())
    definitions = [d for d in gm.get_editor_property('surface_gameplay_controllers') if str(d.planet_id) not in ('MarII', 'Mars')]
    for key in ('Mars', 'MarII'):
        target = '/Game/Space/Blueprints/Planets/%s/BP_%sPassiveSurfaceController' % (key, key)
        controller = unreal.load_asset(target) or EA.duplicate_asset(controller_path, target)
        set_properties(unreal.get_default_object(controller.generated_class()), dict(planet_id=key))
        unreal.BlueprintEditorLibrary.compile_blueprint(controller)
        save(controller)
        definition = unreal.JTSSurfaceGameplayControllerDefinition()
        assert definition.import_text('(PlanetId=%s,ControllerClass="%s")' % (key, controller.generated_class().get_path_name()))
        definitions.append(definition)
    set_properties(gm, dict(surface_gameplay_controllers=definitions))
    unreal.BlueprintEditorLibrary.compile_blueprint(gm_bp)
    save(gm_bp)
    assert level.load_level(MAPS[0])
    result['controllers'] = [str(d.planet_id) for d in definitions]
    return result


def validate():
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    result = {'maps': {}}
    for path in MAPS:
        assert level.load_level(path)
        placed = actors()
        deimos = next(a for a in placed if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'MarII')
        visual = deimos.get_gameplay_surface_actor()
        assert visual.get_actor_enable_collision()
        assert visual.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS
        assert visual.static_mesh_component.static_mesh.get_path_name() == FOLDER + '/MarII/SM_MarII.SM_MarII'
        assert deimos.has_gameplay_surface()
        assert not deimos.get_editor_property('bShowOnStarChart') and not deimos.get_editor_property('bShowScreenMarker')
        row = dict(deimos_landable=True, deimos_navigation_hidden=True,
                   planets=[str(a.get_planet_id()) for a in placed if isinstance(a, unreal.JTSPlanetAnchor)])
        if path == MAPS[0]:
            mars = next(a for a in placed if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'Mars')
            manager = next(a for a in placed if isinstance(a, unreal.JTSSpaceWorldManager))
            assert str(manager.get_editor_property('initial_planet_id')) == 'Mars'
            debug = next(a for a in placed if isinstance(a, unreal.JTSSpaceDebugTravelManager))
            ids = [str(d.planet_id) for d in debug.get_editor_property('destinations')]
            assert 'Mars' in ids and 'MarII' not in ids
            compressed = manager.get_editor_property('compressed_space')
            entries = compressed.get_editor_property('bodies')
            assert not next(e for e in entries if e.actor == mars).get_editor_property('interference').get_editor_property('bEnabled')
            assert next(e for e in entries if e.actor == deimos).get_editor_property('bEnableNearFlight')
            assert mars.get_editor_property('bShowOnStarChart') and mars.get_editor_property('bShowScreenMarker')
            row['mars_navigation_visible'] = True
            row['debug_destinations'] = ids
            row['surface_queries'] = {}
            for key in ['MarI', 'Mars', 'MarII']:
                planet = next(a for a in placed if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == key)
                hits = 0
                for i in range(96):
                    z = 1 - 2*(i+.5)/96
                    angle = i * math.pi * (3-math.sqrt(5))
                    radial = unreal.Vector(math.sqrt(1-z*z)*math.cos(angle), math.sqrt(1-z*z)*math.sin(angle), z)
                    frame = planet.get_surface_frame_at(planet.get_planet_center()+radial*(planet.get_visual_radius()*3), unreal.Vector(1, 0, 0))
                    hits += int(bool(frame))
                assert hits == 96, (key, hits)
                row['surface_queries'][key] = dict(hits=hits, samples=96)
        row['landing_feet'] = {}
        for planet in [a for a in placed if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) in ('Mars', 'MarII')]:
            arrival = next(a for a in placed if isinstance(a, unreal.JTSPlanetArrivalAnchor) and a.get_planet_anchor() == planet)
            frame = planet.get_surface_frame_at(arrival.get_spacecraft_arrival_transform().translation, arrival.get_actor_forward_vector())
            assert frame
            es = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            ship = es.spawn_actor_from_class(unreal.load_class(None, '/Game/Space/Ships/TwinforkR1/BP_Twinfork_R1.BP_Twinfork_R1_C'), arrival.get_spacecraft_arrival_transform().translation)
            try:
                assert ship.snap_spacecraft_to_surface_transform(planet, frame.transform)
                row['landing_feet'][str(planet.get_planet_id())] = len(ship.get_landing_support_component().get_foot_contacts())
                assert row['landing_feet'][str(planet.get_planet_id())] == 4
            finally:
                es.destroy_actor(ship)
        result['maps'][path] = row
    assert level.load_level(MAPS[0])
    return result


def cleanup():
    """Delete retired meshes and their exclusively-owned dependencies after maps are saved."""
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    result = {'deleted': [], 'retained_shared': []}
    candidates = [*LEGACY,
        '/Game/ThirdParty/MarII/M_Cave_Interior_001',
        '/Game/ThirdParty/MarII/M_Phobos_Dusty_Rock',
        '/Game/ThirdParty/MarII/Phobos_Stylized_albedo',
        '/Game/ThirdParty/MarII/M_Cave_Interior']
    for path in candidates:
        if not EA.does_asset_exist(path):
            continue
        users = EA.find_package_referencers_for_asset(path, load_assets_to_confirm=True)
        if users:
            result['retained_shared'].append(dict(asset=path, referencers=list(users)))
            continue
        assert EA.delete_asset(path), path
        result['deleted'].append(path)
    for path in LEGACY:
        assert not EA.does_asset_exist(path), path
    return result


def pie_probe():
    if hasattr(unreal, '_moon_model_deimos_probe'):
        del unreal._moon_model_deimos_probe
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world, 'Start PIE first'
    unreal.SystemLibrary.execute_console_command(world, 't.IdleWhenNotForeground 0')
    planets = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetAnchor)
    deimos = next(a for a in planets if str(a.get_planet_id()) == 'MarII')
    mars = next(a for a in planets if str(a.get_planet_id()) == 'Mars')
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    ship = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpacecraftActor)[0]
    manager = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpaceWorldManager)[0]
    bodies = unreal.JTSCelestialNavigationLibrary.gather_bodies(manager, ship)
    mars_body = next(b for b in bodies if b.planet == mars)
    deimos_body = next(b for b in bodies if b.planet == deimos)
    assert mars_body.show_on_star_chart and mars_body.show_screen_marker
    assert not deimos_body.show_on_star_chart and not deimos_body.show_screen_marker
    assert ship.is_landed() and ship.get_landed_planet() == mars, 'Initial Mars arrival must park the ship on real terrain'
    assert len(ship.get_landing_support_component().get_foot_contacts()) == 4
    pc.call_method('ServerRequestDebugPlanet', ('Mars', True))
    debug = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpaceDebugTravelManager)[0]
    assert str(debug.get_editor_property('DefaultPlanetId')) == 'Mars'
    result = dict(initial_mars_landing=True, feet=4, default_saved=True,
                  mars_navigation_visible=True, deimos_navigation_hidden=True, ship_location=vec(ship.get_actor_location()),
                  travel_time=unreal.GameplayStatics.get_time_seconds(world),
                  player_planet=str(pc.get_controlled_pawn().get_gameplay_planet().get_planet_id()))
    pc.call_method('ServerRequestDebugPlanet', ('Moon', False))
    result['travel_to_moon'] = str(ship.get_landed_planet().get_planet_id()) == 'Moon'
    assert result['travel_to_moon']
    unreal._moon_model_pie_result = result
    return result


def pie_status():
    if hasattr(unreal, '_moon_model_deimos_probe'):
        return dict(unreal._moon_model_deimos_probe.result)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    ship = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpacecraftActor)[0]
    result = dict(unreal._moon_model_pie_result)
    result['now'] = unreal.GameplayStatics.get_time_seconds(world)
    if result['now'] - result['travel_time'] < 1.1:
        return dict(result, pending_cooldown=True)
    pc.call_method('ServerRequestDebugPlanet', ('Mars', False))
    result['return_to_mars'] = str(ship.get_landed_planet().get_planet_id()) == 'Mars'
    if not result['return_to_mars']:
        debug = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpaceDebugTravelManager)[0]
        return dict(result, status=str(debug.get_editor_property('Status')), diagnosis=diagnose())
    result['return_feet'] = len(ship.get_landing_support_component().get_foot_contacts())
    assert result['return_feet'] == 4
    pawn = pc.get_controlled_pawn()
    assert pawn.get_gameplay_planet() == ship.get_landed_planet()
    assert ship.try_board_player(pawn), 'Boarding on Mars must succeed'
    assert ship.begin_surface_takeoff(), 'Normal Mars takeoff must succeed'
    result['mars_boarding_and_takeoff'] = True
    return start_deimos_landing_probe(world, pc, ship, result)


def start_deimos_landing_probe(world, pc, ship, result):
    """PIE-only approach setup, followed by native descent and normal disembark RPC."""
    manager = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpaceWorldManager)[0]
    deimos = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetAnchor)
                  if str(a.get_planet_id()) == 'MarII')
    manager.get_editor_property('compressed_space').capture_authored_bodies(True)
    manager.set_current_planet(deimos)
    arrival = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetArrivalAnchor)
                   if a.get_planet_anchor() == deimos)
    frame = deimos.get_surface_frame_at(arrival.get_spacecraft_arrival_transform().translation,
                                       arrival.get_actor_forward_vector())
    assert frame and ship.snap_spacecraft_to_surface_transform(deimos, frame.transform)
    ship.set_actor_location(ship.get_actor_location() + frame.up*1200, False, True)
    ship.clear_grounded_planet()
    ship.get_flight_movement_component().stop_movement_immediately()
    manager.set_travel_state(unreal.JTSSpaceTravelState.APPROACH)

    class DeimosLandingProbe:
        def __init__(self):
            self.result = dict(result, deimos_descent='running', approach_setup='PIE-only position above actual mesh')
            self.started = unreal.GameplayStatics.get_time_seconds(world)
            self.handle = unreal.register_slate_post_tick_callback(self.tick)

        def intent(self, lift):
            state = unreal.JTSSpacecraftInputState()
            assert state.import_text('(Lift=%f,bFlightAssistEnabled=True,SpeedLimit=0.5)' % lift)
            ship.call_method('ServerSetFlightInput', (state,))

        def tick(self, delta):
            done = False
            try:
                self.result['deimos_descent_seconds'] = unreal.GameplayStatics.get_time_seconds(world) - self.started
                self.result['deimos_flight_state'] = str(ship.get_flight_state())
                self.result['deimos_landing_failure'] = str(ship.get_last_landing_failure())
                if ship.is_landed():
                    assert ship.get_landed_planet() == deimos
                    self.intent(0)
                    feet = len(ship.get_landing_support_component().get_foot_contacts())
                    assert feet == 4
                    pc.call_method('ServerRequestDisembarkSpacecraft', (ship,))
                    pawn = pc.get_controlled_pawn()
                    assert isinstance(pawn, unreal.JTSCharacter) and pawn.get_gameplay_planet() == deimos
                    self.result.update(deimos_descent='passed', deimos_landing_feet=feet,
                                       deimos_disembark=True, deimos_player_planet=str(pawn.get_gameplay_planet().get_planet_id()))
                    done = True
                elif self.result['deimos_descent_seconds'] > 40:
                    raise AssertionError('Native Deimos descent did not complete')
                else:
                    self.intent(-1)
            except Exception:
                self.result.update(deimos_descent='failed', traceback=traceback.format_exc())
                done = True
            if done:
                unreal.unregister_slate_post_tick_callback(self.handle)
                (OUT / 'deimos_landing_pie.json').write_text(json.dumps(self.result, ensure_ascii=False, indent=2), encoding='utf-8')

    unreal._moon_model_deimos_probe = DeimosLandingProbe()
    return dict(unreal._moon_model_deimos_probe.result)


def diagnose():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    debug = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpaceDebugTravelManager)[0]
    manager = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpaceWorldManager)[0]
    ships = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpacecraftActor)
    planets = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetAnchor)
    return dict(world=world.get_path_name(), pc=pc.get_path_name(), player_state=str(pc.get_editor_property('player_state')),
                host=pc.get_editor_property('player_state').is_expedition_host,
                destinations=[str(d.planet_id) for d in debug.get_editor_property('destinations')],
                manager_planet=str(manager.get_current_planet()),
                planets=[dict(id=str(p.get_planet_id()), has_surface=p.has_gameplay_surface(), surface=str(p.get_gameplay_surface_actor()), component=str(p.get_gameplay_surface_component())) for p in planets],
                ships=[dict(path=s.get_path_name(), planet=str(s.get_landed_planet()), location=vec(s.get_actor_location())) for s in ships])


def close_editor():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.SystemLibrary.quit_editor()
    return dict(closing=True)


def run():
    step = os.environ.get('JTS_MOON_MODEL_STEP', 'inspect')
    try:
        result = globals()[step]()
        result['success'] = True
    except Exception:
        result = dict(success=False, traceback=traceback.format_exc())
        unreal.log_error(result['traceback'])
    (OUT / (step + '.json')).write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('JTS_MOON_MODELS_RESULT ' + json.dumps(result, ensure_ascii=True))
    if os.environ.get('JTS_MOON_MODEL_CLOSE') == '1':
        unreal.SystemLibrary.quit_editor()


if __name__ == '__main__':
    run()
