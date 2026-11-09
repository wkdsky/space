"""MCP authoring and PIE checks for the frozen Moon distribution and persistent entrances."""
import json
import math
import unreal

DATA = '/Game/Space/Data/Planets/Moon/PDA_MoonSurfaceGameplay'
FX = '/Game/Space/Effects/Moon/NS_EntranceCollapseDust'


def _presentation(rebuild_rubble=False):
    material_path = '/Game/Space/Effects/Moon/M_EntranceDust'
    mat = unreal.load_asset(material_path)
    if not mat:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_EntranceDust', '/Game/Space/Effects/Moon', unreal.Material, unreal.MaterialFactoryNew())
        mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property('two_sided', True)
        lib = unreal.MaterialEditingLibrary
        def node(cls):
            return lib.create_material_expression(mat, cls)
        uv = node(unreal.MaterialExpressionTextureCoordinate)
        center = node(unreal.MaterialExpressionConstant2Vector)
        center.set_editor_property('r', .5)
        center.set_editor_property('g', .5)
        distance = node(unreal.MaterialExpressionDistance)
        lib.connect_material_expressions(uv, '', distance, 'A')
        lib.connect_material_expressions(center, '', distance, 'B')
        twice = node(unreal.MaterialExpressionMultiply)
        twice.set_editor_property('const_b', 2)
        lib.connect_material_expressions(distance, '', twice, 'A')
        inverse = node(unreal.MaterialExpressionOneMinus)
        lib.connect_material_expressions(twice, '', inverse, '')
        clamp = node(unreal.MaterialExpressionSaturate)
        lib.connect_material_expressions(inverse, '', clamp, '')
        power = node(unreal.MaterialExpressionPower)
        power.set_editor_property('const_exponent', 1.7)
        lib.connect_material_expressions(clamp, '', power, 'Base')
        particles = node(unreal.MaterialExpressionParticleColor)
        opacity = node(unreal.MaterialExpressionMultiply)
        lib.connect_material_expressions(power, '', opacity, 'A')
        lib.connect_material_expressions(particles, 'A', opacity, 'B')
        lib.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
        lib.connect_material_property(particles, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
        rough = node(unreal.MaterialExpressionConstant)
        rough.set_editor_property('r', 1)
        lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
        lib.recompile_material(mat)
    mat.set_editor_property('used_with_niagara_sprites', True)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    # Dust should remain visible on the unlit side of a spherical surface.
    color = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionParticleColor)
    unreal.MaterialEditingLibrary.connect_material_property(color, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
    path = '/Game/Space/Environment/MoonV3/Meshes/SM_MS3_EntranceRubble'
    mesh = unreal.load_asset(path)
    if not mesh or rebuild_rubble:
        import moon_scene_polish as polish
        import moon_surface_unreal as surface
        dm = unreal.DynamicMesh()
        # Preserve the existing mound silhouette; add individual faceted stones above it.
        old = polish.author_blocker()
        unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(old, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
        vertices, triangles, normals, uv = [], [], [], []
        def tri(a, b, c):
            base = len(vertices)
            vertices.extend([a, b, c])
            triangles.append(unreal.IntVector(base, base + 1, base + 2))
            normal = unreal.MathLibrary.cross_vector_vector(b - a, c - a).normal()
            normals.extend([normal] * 3)
            uv.extend([unreal.Vector2D(p.x / 100, p.y / 100) for p in (a, b, c)])
        for i in range(13):
            angle = i * 2.399963
            radius = 13 + (i % 4) * 10
            x, y = -4 + math.cos(angle) * radius, math.sin(angle) * radius * .73
            z = (50 - radius * 7 / 26 if radius <= 26 else 43 - (radius - 26) * 20 / 22) + 2
            size = 7 + (i * 7 % 9)
            ring = [unreal.Vector(x + math.cos(angle + k * math.pi / 3) * size,
                                  y + math.sin(angle + k * math.pi / 3) * size * .8, z - size * .3) for k in range(6)]
            upper = [unreal.Vector(x + (p.x - x) * .65 + 1, y + (p.y - y) * .65 - 1, z + size * .5) for p in ring]
            top = unreal.Vector(x + 1, y - 1, z + size * .65)
            for k in range(6):
                j = (k + 1) % 6
                tri(ring[k], ring[j], upper[j])
                tri(ring[k], upper[j], upper[k])
                tri(upper[k], upper[j], top)
        dm.append_buffers_to_mesh(unreal.GeometryScriptSimpleMeshBuffers(vertices=vertices, triangles=triangles, normals=normals, uv0=uv), 1)
        if mesh:
            unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dm, mesh, unreal.GeometryScriptCopyMeshToAssetOptions(), unreal.GeometryScriptMeshWriteLOD())
        else:
            mesh, _ = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm, path,
                unreal.GeometryScriptCreateNewStaticMeshAssetOptions(enable_collision=False, enable_recompute_normals=True, enable_recompute_tangents=True))
        assert mesh
        mesh.set_editor_property('static_materials', [
            unreal.StaticMaterial(material_interface=unreal.load_asset(surface.MATERIALS + '/TunnelRegolith'), material_slot_name='Regolith'),
            unreal.StaticMaterial(material_interface=unreal.load_asset('/Game/Space/Environment/MoonResources/Materials/M_MR_Fresh'), material_slot_name='Rocks')])
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    for a in _actors():
        if a.get_actor_label().startswith('Moon_Nest_'):
            a.modify()
            a.get_editor_property('entrance_blocker').set_static_mesh(mesh)
    return dict(material=material_path, rubble=path)


def _actors():
    return list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())


def _configure():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    actors = _actors()
    planet = next(a for a in actors if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'Moon')
    nests = sorted([a for a in actors if a.get_actor_label().startswith('Moon_Nest_')], key=lambda a: a.get_actor_label())
    assert len(nests) == 9
    before = {a.get_actor_label(): a.get_actor_transform() for a in nests}
    data = unreal.load_asset(DATA)
    assert isinstance(data, unreal.JTSMoonSurfaceGameplayData)
    fx = unreal.load_asset(FX)
    if not fx:
        fx = unreal.EditorAssetLibrary.duplicate_asset('/Niagara/DefaultAssets/Templates/Systems/RadialBurst', FX)
    assert fx
    for nest in nests:
        nest.modify()
        nest.set_editor_property('can_block_entrance', True)
        nest.set_editor_property('destructible_entrance', False)
        work = nest.get_editor_property('blockade')
        work.set_editor_property('damage_per_strike', .2)
        work.set_editor_property('decay_delay', 12.0)
        work.set_editor_property('decay_per_second', .02)
        work.set_editor_property('persistent_entrance_id', 'Moon.' + nest.get_actor_label())
        work.set_editor_property('collapse_dust', fx)
        work.set_editor_property('dust_scale', unreal.Vector(1.5, 1.5, 1.0))
        work.set_editor_property('defense_population', 10)
        work.set_editor_property('defense_leash_distance', 1800)
        blocker = nest.get_editor_property('entrance_blocker')
        assert blocker.get_editor_property('static_mesh'), 'Configure the existing rubble mesh before baking'
    habitats = sorted([a for a in actors if isinstance(a, unreal.JTSPlanetEcologyHabitat) and a.get_owner() == planet], key=lambda a: a.get_name())
    spawners = sorted([a for a in actors if isinstance(a, unreal.JTSMoonResourceSpawner) and a.get_editor_property('coverage_planet') == planet], key=lambda a: a.get_name())
    assert habitats and spawners
    for actor in habitats + spawners:
        actor.modify()
    assert data.bake_distribution(planet, habitats, spawners)
    controllers = [a for a in actors if isinstance(a, unreal.JTSMoonSurfaceController) and str(a.get_planet_id()) == 'Moon']
    assert len(controllers) == 1
    controllers[0].modify()
    controllers[0].set_editor_property('distribution_data', data)
    for nest in nests:
        assert nest.get_actor_transform().equals(before[nest.get_actor_label()])
    item_path = '/Game/Space/Data/Items/DA_Item_Pickaxe'
    item = unreal.load_asset(item_path)
    if not item:
        source = unreal.JTSItemDefinitionLibrary.get_item_definition(None, unreal.JTSItemId.PICKAXE)
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.JTSItemDefinition)
        item = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_Item_Pickaxe', '/Game/Space/Data/Items', unreal.JTSItemDefinition, factory)
        for name in ['capability_mask', 'primary_category', 'shop_categories', 'melee_attack_class', 'held_presentation', 'affinity_tags', 'accent_color', 'max_stack_size', 'default_durability']:
            item.set_editor_property(name, source.get_editor_property(name))
    assert item
    item.set_editor_property('item_id', unreal.JTSItemId.PICKAXE)
    item.set_editor_property('display_name', '\u9550\u5b50')
    item.set_editor_property('description', '\u91c7\u77ff\u4e0e\u7834\u574f\u5de2\u7a74\u6d1e\u53e3\u3002\u88c5\u5907\u540e\u957f\u6309\u6216\u53cd\u590d\u6309 E \u65bd\u5de5\u3002')
    item.set_editor_property('combat_damage', 18)
    item.set_editor_property('melee_attack_interval', 0.325)
    item.set_editor_property('melee_swing_play_rate', 2.0)
    item.set_editor_property('mining_work', 5)
    costs = []
    for resource, count in [(unreal.JTSResourceType.ROCK, 2), (unreal.JTSResourceType.ORE, 1)]:
        cost = unreal.JTSItemCost()
        cost.set_editor_property('resource_type', resource)
        cost.set_editor_property('amount', count)
        costs.append(cost)
    item.set_editor_property('shop_costs', costs)
    held = item.get_editor_property('held_presentation')
    held.set_editor_property('body_scale', unreal.Vector(.9, .045, .045))
    held.set_editor_property('barrel_scale', unreal.Vector(.08, .48, .08))
    held.set_editor_property('barrel_location', unreal.Vector(52, 0, 0))
    item.set_editor_property('held_presentation', held)
    for asset in [data, item, fx]:
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return _inspect()


def _inspect():
    data = unreal.load_asset(DATA)
    assert data
    regions = data.get_editor_property('resource_layouts')
    scatters = data.get_editor_property('scatter_layouts')
    return dict(asset=DATA, actors=len(data.get_editor_property('actor_poses')),
                resource_regions=len(regions), resources=sum(len(x.get_editor_property('resources')) for x in regions),
                scatter_layouts=len(scatters), props=sum(len(x.get_editor_property('props')) for x in scatters))


def _cleanup_bake():
    import moon_surface_refinement as refinement
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    data = unreal.load_asset(DATA)
    original = {str(row.get_editor_property('actor_name')) for row in data.get_editor_property('actor_poses')}
    actors = _actors()
    planet = next(a for a in actors if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'Moon')
    extras = [a for a in actors if a.get_owner() == planet
              and isinstance(a, (unreal.JTSPlanetSurfaceAttachment, unreal.JTSPlanetEcologyHabitat))
              and (a.get_name() not in original or (isinstance(a, unreal.JTSPlanetEcologyHabitat)
                   and str(a.get_editor_property('surface_scatter').get_editor_property('layout_key')) != 'None'))]
    generated_names = {a.get_name() for a in extras}
    data.modify()
    data.set_editor_property('actor_poses', [row for row in data.get_editor_property('actor_poses')
                                           if str(row.get_editor_property('actor_name')) not in generated_names])
    layouts, seen = [], set()
    for row in data.get_editor_property('scatter_layouts'):
        key = str(row.get_editor_property('key'))
        if key not in seen: layouts.append(row); seen.add(key)
    data.set_editor_property('scatter_layouts', layouts)
    assert unreal.EditorAssetLibrary.save_loaded_asset(data)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in actors:
        if not isinstance(a, unreal.JTSPlanetEcologyHabitat) or a in extras: continue
        for field in ['refuges', 'food_points', 'refuge_providers']:
            a.modify()
            a.set_editor_property(field, [ref for ref in a.get_editor_property(field) if ref not in extras])
    for a in extras: assert subsystem.destroy_actor(a)
    assert refinement.check_protected()
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return dict(removed=len(extras), protected=True)


def _world():
    return next(w for w in unreal.EditorLevelLibrary.get_pie_worlds(False) if unreal.GameplayStatics.get_game_mode(w))


def _layout():
    import hashlib
    world = _world()
    classes = (unreal.JTSMoonResourceActor, unreal.JTSWorldPickupActor, unreal.JTSPlanetSurfaceAttachment, unreal.JTSPlanetEcologyHabitat)
    rows = []
    for cls in classes:
        for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls):
            p = a.get_actor_location()
            r = a.get_actor_rotation()
            scale = a.get_actor_scale3d()
            mesh = a.get_component_by_class(unreal.StaticMeshComponent)
            mesh = mesh.get_editor_property('static_mesh') if mesh else None
            rows.append((cls.__name__, round(p.x, 2), round(p.y, 2), round(p.z, 2),
                         round(r.pitch, 2), round(r.yaw, 2), round(r.roll, 2),
                         round(scale.x, 3), round(scale.y, 3), round(scale.z, 3), mesh.get_path_name() if mesh else ''))
    return dict(count=len(rows), by_class={cls.__name__: sum(row[0] == cls.__name__ for row in rows) for cls in classes},
                signature=hashlib.sha256(json.dumps(sorted(rows)).encode()).hexdigest())


def _probe(visual_only=False, tool_checks=False):
    import time
    import moon_surface_unreal as surface
    world = _world()
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    interaction = pawn.get_component_by_class(unreal.InteractionComponent)
    inventory = pawn.get_component_by_class(unreal.JTSInventoryComponent)
    hud = next((o for o in unreal.ObjectIterator(unreal.JTSPrototypeHUDWidget) if o.get_owning_player_pawn() == pawn), None) if tool_checks else None
    if tool_checks:
        assert hud, 'Gameplay HUD is missing'
        def owned_widget(cls, name):
            return next(o for o in unreal.ObjectIterator(cls) if o.get_name() == name and o.get_path_name().startswith(hud.get_path_name() + '.'))
        reticle = owned_widget(unreal.TextBlock, 'CrosshairText')
        ring = owned_widget(unreal.JTSCircularProgressWidget, 'EntranceProgressWidget')
    melee = pawn.get_component_by_class(unreal.JTSMeleeComponent)
    if inventory.get_active_item_id() == unreal.JTSItemId.PICKAXE:
        inventory.select_quickbar_slot(next(i for i in range(inventory.get_inventory_capacity())
            if inventory.get_item_at_slot(i).get_editor_property('item_id') != unreal.JTSItemId.PICKAXE))
    health = pawn.get_component_by_class(unreal.JTSHealthComponent)
    old_max_health = health.get_max_health()
    health.set_max_health(10000, True)
    subsystem_library = unreal.load_object(None, '/Script/Engine.Default__SubsystemBlueprintLibrary')
    expedition = subsystem_library.call_method('GetGameInstanceSubsystem', (world, unreal.JTSExpeditionSubsystem.static_class()))
    old_prefix = unreal.get_default_object(unreal.JTSExpeditionSubsystem).get_editor_property('save_slot_prefix')
    prefix = 'MoonBlockadeQA_' + str(int(time.time()))
    expedition.set_editor_property('save_slot_prefix', prefix)
    assert expedition.begin_new_expedition_in_slot(1, 'Temporary Moon blockade validation')
    nests = sorted([a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetEcologyHabitat)
                    if a.get_editor_property('can_block_entrance')], key=lambda a: a.get_actor_label())
    assert len(nests) == 9
    saved, old_target = pawn.get_actor_transform(), pc.get_view_target()
    movement = pawn.character_movement
    mode = movement.get_editor_property('movement_mode')
    movement.disable_movement()
    lib = unreal.get_default_object(unreal.GameplayStatics.static_class())
    def temporary(cls, pose=None, setup=None):
        pose = pose or unreal.Transform()
        a = lib.call_method('BeginDeferredActorSpawnFromClass', (world, cls.static_class(), pose, unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN, None, unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        if setup: setup(a)
        return lib.call_method('FinishSpawningActor', (a, pose, unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    camera = temporary(unreal.CameraActor)
    pc.set_view_target_with_blend(camera, 0)
    capture = temporary(unreal.SceneCapture2D)
    lamp = temporary(unreal.PointLight)
    lamp.point_light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    lamp.point_light_component.set_editor_property('intensity_units', unreal.LightUnits.CANDELAS)
    lamp.point_light_component.set_intensity(20)
    lamp.point_light_component.set_attenuation_radius(1000)
    c = capture.capture_component2d
    tex = unreal.RenderingLibrary.create_render_target2d(world, 1280, 800, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    c.set_editor_property('texture_target', tex)
    c.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    c.set_editor_property('fov_angle', 52)
    c.set_editor_property('capture_every_frame', False)
    c.set_editor_property('always_persist_rendering_state', True)
    c.set_editor_property('hidden_actors', [])
    p = c.get_editor_property('post_process_settings')
    p.set_editor_property('override_auto_exposure_method', True)
    p.set_editor_property('auto_exposure_method', unreal.AutoExposureMethod.AEM_MANUAL)
    p.set_editor_property('override_auto_exposure_apply_physical_camera_exposure', True)
    p.set_editor_property('auto_exposure_apply_physical_camera_exposure', False)
    c.set_editor_property('post_process_settings', p)
    c.set_editor_property('post_process_blend_weight', 1)
    result = dict(pending=True, targets=[], layout=_layout(), original_max_health=old_max_health)
    unreal._moon_blockade_result = result
    class Pending:
        def __init__(self):
            self.phase, self.index = 'targets', 0
            self.place(nests[0])
            if tool_checks:
                self.catalog = [getattr(unreal.JTSItemId, name) for name in ('PICKAXE', 'SHORT_BLADE', 'SHOCK_POLE', 'POWER_HAMMER',
                    'RAIL_PISTOL', 'ASSAULT_RIFLE', 'SHOTGUN', 'RAIL_SNIPER', 'HEAVY_MACHINE_GUN', 'GRENADE_LAUNCHER', 'ARC_GUN', 'WAIST_LAMP')]
                self.phase = 'reticle_catalog'
                self.equip_check_item()
                melee.on_attack_started.add_callable(self.swing)
            self.handle = unreal.register_slate_post_tick_callback(self.tick)
        def swing(self, attack_type):
            result.setdefault('swing_times', []).append(dict(seconds=unreal.GameplayStatics.get_time_seconds(world), phase=self.phase))
        def equip_check_item(self):
            self.check_item = self.catalog[self.index]
            self.added_item = inventory.get_item_count(self.check_item) == 0
            if self.added_item:
                if not inventory.can_add_item(self.check_item, 1):
                    assert inventory.select_quickbar_slot(0)
                    old = inventory.get_item_at_slot(0)
                    inventory.call_method('ServerDestroyItemQuantityAtSlot', (0, old.get_editor_property('stack_count')))
                assert inventory.try_add_item_by_id(self.check_item, 1)
            slot = next(i for i in range(inventory.get_inventory_capacity()) if inventory.get_item_at_slot(i).get_editor_property('item_id') == self.check_item)
            assert inventory.select_quickbar_slot(slot)
            if self.check_item == unreal.JTSItemId.WAIST_LAMP:
                pawn.get_component_by_class(unreal.JTSRangedWeaponComponent).start_aim()
            self.at = unreal.GameplayStatics.get_time_seconds(world)
        def place(self, a):
            mesh = a.get_editor_property('refuge_mesh')
            target = a.get_interaction_anchor_world_location()
            pos = mesh.get_world_location() - mesh.get_forward_vector() * 120 + mesh.get_up_vector() * 100
            rot = unreal.MathLibrary.make_rot_from_xz(mesh.get_forward_vector(), mesh.get_up_vector())
            pawn.set_actor_location(pos, False, True)
            pawn.set_actor_rotation(rot, True)
            pawn.initialize_planet_frame()
            camera.set_actor_location(pos + mesh.get_up_vector()*60, False, True)
            camera.set_actor_rotation(unreal.MathLibrary.make_rot_from_xz(target-camera.get_actor_location(),mesh.get_up_vector()),True)
            view=mesh.get_world_location()-mesh.get_forward_vector()*650-mesh.get_right_vector()*180+mesh.get_up_vector()*330
            for actor in (capture, lamp):
                actor.set_actor_location(view, False, True)
                actor.set_actor_rotation(unreal.MathLibrary.make_rot_from_xz(target-mesh.get_forward_vector()*70-view,mesh.get_up_vector()),True)
            self.at = unreal.GameplayStatics.get_time_seconds(world)
        def image(self, name):
            c.capture_scene()
            unreal.RenderingLibrary.export_render_target(world, tex, str(surface.QA), name + '.png')
            result[name] = str(surface.QA / (name + '.png'))
            if tool_checks and name in ('blockade_before', 'earthen_swarm_1_6', 'earthen_swarm_3_2'):
                file = surface.QA / ('pickaxe_hud_' + name + '.png')
                unreal.SystemLibrary.execute_console_command(world, 'Shot SHOWUI filename="' + str(file) + '" -nosuffix', pc)
                result['hud_' + name] = str(file)
        def finish(self):
            interaction.cancel_interaction()
            if tool_checks: melee.on_attack_started.remove_callable(self.swing)
            health.set_max_health(old_max_health, False)
            pc.set_view_target_with_blend(old_target, 0)
            pawn.set_actor_transform(saved, False, True)
            movement.set_movement_mode(mode)
            for actor in (camera, capture, lamp): actor.destroy_actor()
            for slot in (1, 2): expedition.delete_expedition_slot(slot)
            expedition.set_editor_property('save_slot_prefix', old_prefix)
            unreal.unregister_slate_post_tick_callback(self.handle)
            result['pending'] = False
        def residents(self):
            return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetSurfaceCreature)
                    if a.get_editor_property('refuge_actor') == nests[0] or a.get_editor_property('habitat') == nests[0]]
        def tick(self, dt):
            try:
                now = unreal.GameplayStatics.get_time_seconds(world)
                if self.phase == 'reticle_catalog':
                    if now - self.at < .3: return
                    definition = unreal.JTSItemDefinitionLibrary.get_item_definition(pawn, self.check_item)
                    expected = definition.is_holdable() and (definition.get_editor_property('combat_damage') > 0 or definition.get_editor_property('ranged_damage') > 0)
                    visible = reticle.is_visible()
                    result.setdefault('reticles', []).append(dict(item=str(self.check_item), visible=visible, expected=expected))
                    assert visible == expected, result['reticles'][-1]
                    assert reticle.get_editor_property('Font').get_editor_property('OutlineSettings').get_editor_property('OutlineSize') >= 2
                    pawn.get_component_by_class(unreal.JTSRangedWeaponComponent).stop_aim()
                    if self.added_item:
                        inventory.call_method('ServerDestroyItemQuantityAtSlot', (inventory.get_selected_quickbar_slot(), 1))
                        assert inventory.get_item_count(self.check_item) == 0
                    self.index += 1
                    if self.index < len(self.catalog): self.equip_check_item(); return
                    self.index = 0; self.phase = 'mouse_swing'; self.equip_check_item()
                    melee.attack_pressed(); return
                if self.phase == 'mouse_swing':
                    arm = pawn.mesh.get_socket_location('Wrist_R') - pawn.mesh.get_socket_location('LowerArm_R')
                    up = pawn.get_actor_up_vector()
                    result.setdefault('mouse_arm', []).append(dict(seconds=now, arm_up=(arm.x*up.x+arm.y*up.y+arm.z*up.z)/max(.01,arm.length())))
                    if now - self.at < 2: return
                    melee.attack_released()
                    stamps = [r['seconds'] for r in result['swing_times'] if r['phase'] == 'mouse_swing']
                    intervals = [b-a for a,b in zip(stamps,stamps[1:])]
                    result['mouse_swing_intervals'] = intervals
                    assert len(intervals) >= 4 and sum(intervals)/len(intervals) < .45, intervals
                    self.phase, self.at = 'mouse_settle', now; return
                if self.phase == 'mouse_settle':
                    if now - self.at < .5: return
                    inventory.select_quickbar_slot(next(i for i in range(inventory.get_inventory_capacity())
                        if inventory.get_item_at_slot(i).get_editor_property('item_id') != unreal.JTSItemId.PICKAXE))
                    self.phase = 'targets'; self.place(nests[0]); return
                if tool_checks and self.phase in ('partial', 'decay', 'taps', 'tap_result', 'finish'):
                    actual = nests[0].get_editor_property('blockade').get_progress()
                    shown = ring.get_progress()
                    angle = ring.get_editor_property('RenderTransform').get_editor_property('Angle')
                    result.setdefault('hud_progress', []).append(dict(seconds=now, actual=actual, shown=shown, angle=angle, visible=ring.is_visible()))
                    arm = pawn.mesh.get_socket_location('Wrist_R') - pawn.mesh.get_socket_location('LowerArm_R')
                    up = pawn.get_actor_up_vector()
                    result['hud_progress'][-1]['arm_up'] = (arm.x*up.x + arm.y*up.y + arm.z*up.z) / max(.01, arm.length())
                    result['hud_progress'][-1]['phase'] = self.phase
                    import mining_feedback
                    result['hud_progress'][-1].update(mining_feedback.sample_strike(pawn))
                    assert abs(angle) < .001, 'The progress ring is still spinning'
                    if not nests[0].is_entrance_blocked():
                        assert abs(actual-shown) < .08, 'HUD does not follow authoritative progress'
                    assert reticle.is_visible(), 'Pickaxe reticle disappeared'
                result['damage_observed'] = max(result.get('damage_observed', 0), health.get_max_health() - health.get_health())
                if hasattr(self,'alarm_at') and now-self.alarm_at < 5:
                    if result.get('damage_observed',0)>0 and 'first_attack_seconds' not in result:
                        result['first_attack_seconds']=round(now-self.alarm_at,3)
                    cohort=self.residents()
                    frame=dict(seconds=round(now-self.alarm_at,3),count=len(cohort),
                        visible=sum(a.get_editor_property('mesh').is_visible() for a in cohort),
                        positions=[dict(name=a.get_name(),point=surface.xyz(a.get_actor_location()),state=str(a.get_editor_property('state'))) for a in cohort])
                    result.setdefault('emergence_frames',[]).append(frame)
                    for threshold in (.3,.8,1.6,2.4,3.2):
                        key='earthen_swarm_'+str(threshold).replace('.','_')
                        if now-self.alarm_at>=threshold and key not in result:self.image(key)
                health.heal(100)
                if self.phase == 'targets':
                    if now - self.at < .25: return
                    a = nests[self.index]
                    interaction.refresh_interactable()
                    row = dict(nest=a.get_actor_label(), visible=interaction.is_interactable_in_view(a), selected=interaction.get_current_interactable() == a)
                    mesh=a.get_editor_property('refuge_mesh')
                    start=pawn.get_actor_location()
                    inside=mesh.get_world_location()-mesh.get_forward_vector()*10+mesh.get_up_vector()*100
                    pawn.set_actor_location(inside,True,True)
                    row['pawn_stopped_before_opening']=(pawn.get_actor_location()-inside).length()>20
                    pawn.set_actor_location(start,False,True)
                    result['targets'].append(row)
                    assert row['visible'] and row['selected'], row
                    assert row['pawn_stopped_before_opening'],row
                    self.index += 1
                    if self.index < len(nests): self.place(nests[self.index]); return
                    self.place(nests[0]); self.phase = 'no_tool'; return
                if self.phase == 'no_tool':
                    if now - self.at < .3: return
                    assert not nests[0].call_method('CanExecuteInteraction', (pawn,))
                    result['no_tool_prompt'] = str(nests[0].call_method('GetInteractionPrompt', (pawn,)))
                    self.image('blockade_before')
                    if visual_only:
                        self.finish();return
                    if not inventory.get_item_count(unreal.JTSItemId.PICKAXE) and not inventory.can_add_item(unreal.JTSItemId.PICKAXE, 1):
                        assert inventory.select_quickbar_slot(0)
                        inventory.call_method('ServerDestroyItemQuantityAtSlot', (0, inventory.get_item_at_slot(0).get_editor_property('stack_count')))
                    if not inventory.get_item_count(unreal.JTSItemId.PICKAXE):
                        assert inventory.try_add_item_by_id(unreal.JTSItemId.PICKAXE, 1)
                    slot = next(i for i in range(inventory.get_inventory_capacity()) if inventory.get_item_at_slot(i).get_editor_property('item_id') == unreal.JTSItemId.PICKAXE)
                    assert inventory.select_quickbar_slot(slot)
                    assert nests[0].call_method('CanExecuteInteraction', (pawn,))
                    self.normal = len(self.residents())
                    interaction.refresh_interactable()
                    assert interaction.try_interact()
                    result['initial_defenders'] = len(self.residents())
                    self.alarm_at=now
                    self.at, self.phase = now, 'partial'; return
                if self.phase == 'partial':
                    if now - self.at < .9: return
                    self.partial = nests[0].get_editor_property('blockade').get_progress()
                    result['partial_progress'] = self.partial
                    result['defenders'] = len(self.residents())
                    assert 0 < self.partial < 1 and result['defenders'] <= 10, result
                    interaction.cancel_interaction()
                    self.at, self.phase = now, 'decay'; return
                if self.phase == 'decay':
                    if now - self.at < 1.3: return
                    result['defenders_spawned']=len(self.residents())
                    assert result['defenders_spawned']==10,result['defenders_spawned']
                    visible=[a.get_actor_location() for a in self.residents() if a.get_editor_property('mesh').is_visible()]
                    distinct=[]
                    for point in visible:
                        if all((point-other).length()>28 for other in distinct):distinct.append(point)
                    result['distinct_visible_defenders']=len(distinct)
                    assert len(distinct)>=8,'Defense bodies still overlap'
                    progress = nests[0].get_editor_property('blockade').get_progress()
                    result['decayed_progress'] = progress
                    assert progress < self.partial
                    self.at, self.phase = now, 'taps'; self.tap_at = now; self.taps = 0; return
                if self.phase == 'taps':
                    if now >= self.tap_at:
                        interaction.refresh_interactable()
                        assert interaction.try_interact()
                        interaction.cancel_interaction()
                        self.tap_at = now + .25; self.taps += 1
                    if self.taps < 5: return
                    self.at, self.phase = now, 'tap_result'; return
                if self.phase == 'tap_result':
                    if now - self.at < .3: return
                    result['tap_progress'] = nests[0].get_editor_property('blockade').get_progress()
                    assert result['tap_progress'] > .08
                    interaction.refresh_interactable(); assert interaction.try_interact()
                    self.at, self.phase = now, 'finish'; return
                if self.phase == 'finish':
                    if nests[0].is_entrance_blocked():
                        assert result.get('damage_observed', 0) > 0, 'Defenders approached but never attacked'
                        if tool_checks:
                            stamps = [r['seconds'] for r in result.get('swing_times', []) if r['phase'] == 'finish']
                            intervals = [b-a for a,b in zip(stamps,stamps[1:])]
                            result['work_swing_intervals'] = intervals
                            assert len(intervals) >= 8 and sum(intervals)/len(intervals) < .5, intervals
                            frames = [r for r in result['hud_progress'] if r['phase'] == 'finish']
                            contacts = {}
                            for frame in frames:
                                if frame['impact_feedback'] > .6:
                                    count = frame['impact_count']
                                    contacts[count] = min(contacts.get(count, 10000), frame['tip_contact_error'])
                            result['physical_contact_errors'] = contacts
                            assert len(contacts) >= 8, contacts
                            assert max(contacts.values()) < 12, contacts
                            cuts = [b['seconds'] for a,b in zip(frames,frames[1:])
                                if b['seconds']-a['seconds'] < .1 and b['tip_height']-a['tip_height'] < -8]
                            result['downward_cut_frames'] = len(cuts)
                            assert len(cuts) >= 8, 'The tool tip does not visibly chip downwards'
                        result['dust_components'] = len([o for o in unreal.ObjectIterator(unreal.NiagaraComponent)
                            if o.get_editor_property('asset') == unreal.load_asset(FX)])
                        self.at, self.phase = now, 'dust'; return
                    result['finish_progress'] = nests[0].get_editor_property('blockade').get_progress()
                    result['finish_target'] = str(interaction.get_current_interactable())
                    result['finish_health'] = health.get_health()
                    result['finish_dead'] = health.get_editor_property('is_dead')
                    result['finish_item'] = str(inventory.get_active_item_id())
                    if not interaction.get_current_interactable():
                        hit = unreal.SystemLibrary.line_trace_single(world, camera.get_actor_location(), nests[0].get_interaction_anchor_world_location(),
                            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [pawn], unreal.DrawDebugTrace.NONE, True)
                        result['occlusion'] = str(lib.call_method('BreakHitResult', (hit,))) if hit else 'no hit'
                    assert now - self.at < 9, 'Hold did not complete'
                    return
                if self.phase == 'dust':
                    if now - self.at < .25: return
                    self.image('blockade_dust')
                    self.phase = 'rubble'; return
                if self.phase == 'rubble':
                    if now - self.at < 4: return
                    assert nests[0].get_editor_property('entrance_blocker').is_visible()
                    assert not nests[0].call_method('CanInteract', (pawn,))
                    self.image('blockade_after')
                    result['blocked'] = True
                    self.at, self.phase = now, 'withdraw'
                    self.cohort = {a.get_name() for a in self.residents()}
                    planet = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetAnchor) if str(a.get_planet_id()) == 'Moon')
                    mesh = nests[0].get_editor_property('refuge_mesh')
                    ground = planet.project_point_to_surface(mesh.get_world_location() - mesh.get_forward_vector() * 5000)
                    retreat = ground.impact_point + ground.impact_normal * 100
                    result['retreat_distance'] = (retreat - nests[0].get_interaction_anchor_world_location()).length()
                    assert result['retreat_distance'] > nests[0].get_editor_property('blockade').get_editor_property('defense_leash_distance')
                    pawn.set_actor_location(retreat, False, True)
                    return
                if self.phase == 'withdraw':
                    if now - self.at < 12: return
                    live = {a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetSurfaceCreature)}
                    remaining = self.cohort.intersection(live)
                    result['remaining_after_retreat'] = len(remaining)
                    assert not remaining, 'Defenders did not withdraw'
                    self.check_save()
                    self.finish()
            except Exception:
                import traceback
                result['error'] = traceback.format_exc()
                self.finish()
        def check_save(self):
            assert expedition.begin_new_expedition_in_slot(2, 'Temporary independent save')
            def clone():
                def setup(a):
                    a.set_editor_property('can_block_entrance', True)
                    a.get_editor_property('refuge_mesh').set_static_mesh(nests[0].get_editor_property('refuge_mesh').get_editor_property('static_mesh'))
                    a.get_editor_property('blockade').set_editor_property('persistent_entrance_id', 'Moon.' + nests[0].get_actor_label())
                return temporary(unreal.JTSPlanetEcologyHabitat, nests[0].get_actor_transform(), setup)
            fresh = clone()
            result['independent_save_open'] = not fresh.is_entrance_blocked(); fresh.destroy_actor()
            assert result['independent_save_open']
            assert expedition.load_expedition_slot(1)
            restored = clone()
            result['loaded_save_blocked'] = restored.is_entrance_blocked()
            result['loaded_rubble_visible'] = restored.get_editor_property('entrance_blocker').is_visible()
            restored.destroy_actor()
            assert result['loaded_save_blocked'] and result['loaded_rubble_visible']
    unreal._moon_blockade_probe = Pending()
    return result


def run(stage):
    if stage == 'tool_perf':
        settings = unreal.load_object(None, '/Script/UnrealEd.Default__EditorPerformanceSettings')
        unreal._moon_old_background_throttle = settings.get_editor_property('bThrottleCPUWhenNotForeground')
        settings.set_editor_property('bThrottleCPUWhenNotForeground', False)
        return json.dumps(dict(background_throttle=False))
    if stage == 'tool_perf_restore':
        unreal.load_object(None, '/Script/UnrealEd.Default__EditorPerformanceSettings').set_editor_property('bThrottleCPUWhenNotForeground', getattr(unreal, '_moon_old_background_throttle', True))
        return json.dumps(dict(restored=True))
    if stage == 'tools_cleanup':
        world = _world()
        lib = unreal.load_object(None, '/Script/Engine.Default__SubsystemBlueprintLibrary')
        expedition = lib.call_method('GetGameInstanceSubsystem', (world, unreal.JTSExpeditionSubsystem.static_class()))
        if str(expedition.get_editor_property('save_slot_prefix')).startswith('MoonBlockadeQA_'):
            for slot in (1,2): expedition.delete_expedition_slot(slot)
        return json.dumps(dict(cleaned=True))
    if stage == 'tool_tuning':
        item = unreal.load_asset('/Game/Space/Data/Items/DA_Item_Pickaxe')
        item.set_editor_property('melee_attack_interval', .325)
        item.set_editor_property('melee_swing_play_rate', 2.0)
        assert unreal.EditorAssetLibrary.save_loaded_asset(item)
        return json.dumps(dict(interval=.325, swing_play_rate=2.0))
    if stage == 'save':
        assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        return json.dumps(dict(saved=True))
    if stage == 'exit':
        unreal.SystemLibrary.quit_editor()
        return json.dumps(dict(closing=True))
    if stage == 'configure':
        return json.dumps(_configure())
    if stage == 'presentation':
        return json.dumps(_presentation())
    if stage == 'rubble':
        return json.dumps(_presentation(True))
    if stage == 'inspect':
        return json.dumps(_inspect())
    if stage == 'cleanup_bake':
        return json.dumps(_cleanup_bake())
    if stage == 'editor_audit':
        from collections import Counter
        data = unreal.load_asset(DATA)
        poses = {str(row.get_editor_property('actor_name')) for row in data.get_editor_property('actor_poses')}
        return json.dumps(dict(actors=dict(Counter(a.get_class().get_name() for a in _actors())),
            foragers=[dict(name=a.get_name(), owner=str(a.get_owner()), recorded=a.get_name() in poses,
                           key=str(a.get_editor_property('surface_scatter').get_editor_property('layout_key')))
                      for a in _actors() if a.get_name().startswith('BP_MoonResourceForagers_')],
            top_layouts=[dict(key=str(row.get_editor_property('key')), props=len(row.get_editor_property('props')))
                         for row in data.get_editor_property('scatter_layouts') if str(row.get_editor_property('key')).startswith('BP_MoonResourceForagers_')]))
    if stage == 'layout':
        return json.dumps(_layout())
    if stage == 'audit':
        import moon_surface_refinement as refinement
        world = _world()
        actors = {a.get_actor_label(): a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)}
        planet = next(a for a in actors.values() if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'Moon')
        return json.dumps(dict(protected=refinement.check_protected(actors), runtime_cuts=planet.get_editor_property('surface_cuts').get_cut_count()))
    if stage == 'defense':
        world = _world()
        nest = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetEcologyHabitat) if a.get_actor_label() == 'Moon_Nest_01')
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        rows = []
        for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetSurfaceCreature):
            if a.get_editor_property('habitat') != nest and a.get_editor_property('refuge_actor') != nest: continue
            rows.append(dict(name=a.get_name(), state=str(a.get_editor_property('state')),
                             distance=(a.get_actor_location() - nest.get_interaction_anchor_world_location()).length(),
                             alive=not a.get_component_by_class(unreal.JTSHealthComponent).get_editor_property('is_dead')))
        return json.dumps(dict(player_distance=(pawn.get_actor_location() - nest.get_interaction_anchor_world_location()).length(), creatures=rows))
    if stage == 'probe':
        try:
            return json.dumps(_probe())
        except Exception:
            import traceback
            return json.dumps(dict(error=traceback.format_exc()))
    if stage == 'tools_probe':
        return json.dumps(_probe(False, True))
    if stage == 'visual':
        return json.dumps(_probe(True))
    if stage == 'result':
        return json.dumps(getattr(unreal, '_moon_blockade_result', None))
    raise ValueError(stage)

