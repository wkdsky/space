"""Scoped MCP configuration and inspection for the existing Moon population templates."""
import json
import unreal

if '_probe' not in globals():
    _probe = None


def _recovery(delay, duration, distance, batch):
    value = unreal.JTSPopulationRecoverySettings()
    for name, setting in dict(enabled=True, delay=delay, duration=duration, front_load=3,
                              check_interval=5, batch_size=batch, minimum_player_distance=distance).items():
        value.set_editor_property(name, setting)
    return value


def _species(habitat):
    cls = habitat.get_editor_property('enemy_class')
    name = cls.get_name() if cls else ''
    return 'Mirror' if name.startswith('BP_MirrorSkimmer') else 'Rift' if name.startswith('BP_Riftlip') else None


def _configure_habitat(habitat):
    species = _species(habitat)
    if not species:
        return False
    habitat.modify()
    habitat.set_editor_property('maintain_population', True)
    habitat.set_editor_property('recovery', _recovery(40 if species == 'Mirror' else 60,
                                                    420 if species == 'Mirror' else 600,
                                                    1000 if species == 'Mirror' else 1200, 1))
    return True


def _configure_resources(spawner):
    spawner.modify()
    patches = unreal.JTSMoonResourcePatchSettings()
    patches.set_editor_property('enabled', True)
    patches.set_editor_property('patch_size', 3500)
    patches.set_editor_property('density_contrast', .85)
    patches.set_editor_property('composition_contrast', 2.5)
    spawner.set_editor_property('patches', patches)
    spawner.set_editor_property('recovery', _recovery(45, 600, 1800, 4))
    spawner.set_editor_property('initial_spawn_batch_size', 4)


def configure():
    import moon_surface_unreal as surface
    world = surface.editor_world()
    actors = list(surface.actors().values())
    nests = {a.get_actor_label(): a.get_actor_transform() for a in actors if a.get_actor_label().startswith('Moon_Nest_')}
    assert len(nests) == 9
    resources = [a for a in actors if isinstance(a, unreal.JTSMoonResourceSpawner)]
    for spawner in resources:
        _configure_resources(spawner)
    habitats = [a for a in actors if isinstance(a, unreal.JTSPlanetEcologyHabitat) and _configure_habitat(a)]
    bp = unreal.load_asset('/Game/Space/Blueprints/Planets/Moon/BP_MoonResourceSpawner')
    _configure_resources(unreal.get_default_object(bp.generated_class()))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    for name in ['BP_MoonMirrorFeedingArea', 'BP_MoonResourceForagers_Mirror', 'BP_MoonResourceForagers_Rift']:
        resident_bp = unreal.load_asset(surface.BPS + '/' + name)
        assert resident_bp
        _configure_habitat(unreal.get_default_object(resident_bp.generated_class()))
        unreal.BlueprintEditorLibrary.compile_blueprint(resident_bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(resident_bp)
    # Generated residents use these Blueprint policies and retain their food/refuge dependencies.
    # No meshes, collision, protected nest transforms, density, or exclusion regions are changed here.
    for a in actors:
        if a.get_actor_label() in nests:
            assert a.get_actor_transform().equals(nests[a.get_actor_label()])
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return json.dumps(dict(resources=len(resources), renewable_habitats=len(habitats),
                           protected_nests=len(nests), level=world.get_path_name()))


def inspect():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = subsystem.get_game_world()
    if world:
        actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    else:
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    resources, habitats = [], []
    for a in actors:
        if isinstance(a, unreal.JTSMoonResourceSpawner):
            resources.append(dict(name=a.get_name(), live=a.get_live_resource_count(),
                                  capacity=a.get_editor_property('resource_count'),
                                  recovery=a.get_editor_property('recovery').get_editor_property('enabled'),
                                  patches=a.get_editor_property('patches').get_editor_property('enabled')))
        elif isinstance(a, unreal.JTSPlanetEcologyHabitat) and _species(a):
            habitats.append(dict(name=a.get_name(), species=_species(a), live=a.get_spawned_enemy_count(),
                                 capacity=a.get_editor_property('enemy_count'),
                                 renewable=a.get_editor_property('maintain_population'),
                                 recovery=a.get_editor_property('recovery').get_editor_property('enabled')))
    return json.dumps(dict(playing=bool(world), resources=resources, habitats=habitats))


def probe(stage):
    """Accelerated depletion probe in a disposable PIE world; never edits map/Blueprint settings."""
    global _probe
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world, 'Start PIE before testing population recovery'
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    resources = [a for a in actors if isinstance(a, unreal.JTSMoonResourceSpawner)]
    habitats = [a for a in actors if isinstance(a, unreal.JTSPlanetEcologyHabitat)
                and _species(a) and a.get_editor_property('maintain_population')]
    now = unreal.GameplayStatics.get_time_seconds(world)
    def counts():
        return dict(resources=sum(a.get_live_resource_count() for a in resources),
                    mirror=sum(a.get_spawned_enemy_count() for a in habitats if _species(a) == 'Mirror'),
                    rift=sum(a.get_spawned_enemy_count() for a in habitats if _species(a) == 'Rift'))
    if stage == 'deplete':
        _probe = dict(start=now, baseline=counts())
        for owner in resources + habitats:
            settings = owner.get_editor_property('recovery')
            settings.set_editor_property('delay', 2)
            settings.set_editor_property('duration', 30)
            settings.set_editor_property('minimum_player_distance', 0)
            owner.set_editor_property('recovery', settings)
        destroyed, killed = 0, 0
        for a in actors:
            if a.get_owner() in resources and isinstance(a, (unreal.JTSMoonResourceActor, unreal.JTSWorldPickupActor)):
                a.destroy_actor()
                destroyed += 1
            elif isinstance(a, unreal.JTSPlanetSurfaceCreature) and a.get_editor_property('habitat') in habitats:
                health = a.get_component_by_class(unreal.JTSHealthComponent)
                if health and not health.is_dead:
                    health.apply_damage(10000, None, None)
                    killed += 1
        return json.dumps(dict(stage=stage, baseline=_probe['baseline'], after=counts(), destroyed=destroyed, killed=killed))
    assert stage == 'sample' and _probe, 'Use deplete before sample'
    return json.dumps(dict(stage=stage, elapsed=now-_probe['start'], baseline=_probe['baseline'], live=counts()))
