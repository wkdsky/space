"""Scoped MCP inspection and regression captures for the existing player weapon chain."""
import json
import unreal

PLAYER = '/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2'

def xyz(v): return [v.x, v.y, v.z]
def path(v): return v.get_path_name() if v else None

def inspect_mesh(mesh):
    return dict(component=path(mesh), asset=path(mesh.get_skeletal_mesh_asset()),
        anim=path(mesh.get_anim_instance()),
        anim_class=path(mesh.get_editor_property('anim_class')),
        animation_mode=str(mesh.get_editor_property('animation_mode')),
        transform=str(mesh.get_world_transform()),
        sockets={n:mesh.does_socket_exist(n) for n in ('hand_r','hand_rSocket','RightHand','Hand_R','RightHandSocket','Wrist_R','Index1_R','LowerArm_R')})

def run(stage):
    if stage.startswith('weapon_finish_'):
        import importlib,moon_surface_finish
        return importlib.reload(moon_surface_finish).run(stage.removeprefix('weapon_finish_'))
    if stage == 'weapon_stop_watch':
        if hasattr(unreal,'_weapon_entrance_watch'):
            unreal.unregister_slate_post_tick_callback(unreal._weapon_entrance_watch.handle)
            del unreal._weapon_entrance_watch
        return 'Stopped entrance observation'
    if stage in ('weapon_net_settings','weapon_restore_net_settings'):
        settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
        keys=('PlayNumberOfClients','RunUnderOneProcess')
        if stage=='weapon_net_settings':
            unreal._weapon_old_net_settings={k:settings.get_editor_property(k) for k in keys}
            settings.set_editor_property('PlayNumberOfClients',2)
            settings.set_editor_property('RunUnderOneProcess',True)
        else:
            for k,v in unreal._weapon_old_net_settings.items():settings.set_editor_property(k,v)
        return json.dumps({k:str(settings.get_editor_property(k)) for k in keys})
    if stage == 'weapon_configure_scatter':
        import importlib,moon_surface_randomize
        return importlib.reload(moon_surface_randomize).configure()
    if stage == 'weapon_save_symbolic':
        import moon_surface_unreal as surface
        surface.editor_world()
        fixed=[]
        for a in surface.actors().values():
            if a.actor_has_tag(surface.TAG) and isinstance(a,unreal.StaticMeshActor):
                a.modify();a.static_mesh_component.modify()
                a.static_mesh_component.set_collision_profile_name('NoCollision')
                mesh=a.static_mesh_component.get_editor_property('static_mesh')
                if mesh and 'Refuge' in mesh.get_name():a.static_mesh_component.set_cast_shadow(False)
                fixed.append(a.get_actor_label())
        return json.dumps(dict(saved_profiles=len(fixed)))
    if stage == 'weapon_fix_entrances':
        import moon_surface_unreal as surface
        surface.editor_world()
        rows=[r for r in surface.recipe()['assets'] if any(k in r['name'] for k in ('Refuge','FixedNest'))]
        surface.import_tasks([surface.fbx_task(surface.ART/'FBX'/(r['name']+'.fbx'),surface.MESHES,r['name']) for r in rows])
        mats=surface.surface_materials()
        for row in rows:
            asset=unreal.load_asset(surface.MESHES+'/'+row['name'])
            slots=list(asset.static_materials)
            for slot in slots:slot.material_interface=mats[str(slot.material_slot_name)]
            asset.set_editor_property('static_materials',slots);surface.save(asset)
        fixed=[];symbolic=[]
        for a in surface.actors().values():
            if a.get_actor_label().startswith('Moon_Nest_'):
                a.set_editor_property('destructible_entrance',True)
                a.get_editor_property('refuge_mesh').set_cast_shadow(False)
                fixed.append(a.get_actor_label())
            elif a.actor_has_tag(surface.TAG) and isinstance(a,unreal.StaticMeshActor):
                mesh=a.static_mesh_component.get_editor_property('static_mesh')
                if mesh and 'Refuge' in mesh.get_name():
                    a.static_mesh_component.set_cast_shadow(False)
                    a.static_mesh_component.set_collision_profile_name('NoCollision')
                    symbolic.append(a.get_actor_label())
        return json.dumps(dict(imported=len(rows),destructible=fixed,symbolic=symbolic))
    if stage == 'weapon_assets':
        bp=unreal.load_asset(PLAYER)
        cdo=unreal.get_default_object(bp.generated_class())
        mesh=cdo.get_component_by_class(unreal.SkeletalMeshComponent)
        return json.dumps(inspect_mesh(mesh))
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world, 'Start PIE first'
    pawn=unreal.GameplayStatics.get_player_pawn(world,0)
    pc=unreal.GameplayStatics.get_player_controller(world,0)
    mesh=pawn.get_component_by_class(unreal.SkeletalMeshComponent)
    inv=pawn.get_component_by_class(unreal.JTSInventoryComponent)
    ranged=pawn.get_component_by_class(unreal.JTSRangedWeaponComponent)
    visual=pawn.get_component_by_class(unreal.JTSWeaponVisualComponent)
    if stage == 'weapon_align_camera':
        pc.set_control_rotation(pawn.get_actor_rotation())
        return 'Aligned disposable PIE view with the player forward direction'
    if stage == 'weapon_spawn_traversal':
        import math
        planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        creatures=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceCreature)
        invalid=[]
        for c in creatures:
            hit=planet.project_point_to_surface(c.get_actor_location())
            slope=math.degrees(math.acos(max(-1,min(1,hit.impact_normal.dot(planet.get_radial_up_vector(hit.impact_point))))))
            if slope>c.get_component_by_class(unreal.JTSPlanetSurfaceSteeringComponent).get_editor_property('max_slope_degrees')+.1:
                invalid.append(dict(creature=c.get_name(),slope=slope))
        return json.dumps(dict(checked=len(creatures),beyond_species_slope=invalid))
    if stage=='weapon_net_worlds':
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        result=[]
        for w in worlds:
            props=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.JTSPlanetSurfaceAttachment)
            players=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.JTSCharacter)
            result.append(dict(world=path(w),props=len(props),visible=sum(a.get_component_by_class(unreal.StaticMeshComponent).is_visible() or a.get_component_by_class(unreal.ProceduralMeshComponent).is_visible() for a in props),
                poses=[dict(local=a.is_locally_controlled(),location=xyz(a.get_actor_location()),weapon_parent=path(next(c for c in a.get_components_by_class(unreal.SceneComponent) if c.get_name()=='WeaponHandAttachmentAnchor').get_attach_parent())) for a in players],
                examples=[xyz(a.get_actor_location()) for a in props[:3]]))
        return json.dumps(result)
    if stage == 'weapon_scatter_runtime':
        from pathlib import Path
        props=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceAttachment)
        planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        rows=[]
        for a in props:
            solid=a.get_component_by_class(unreal.StaticMeshComponent);patch=a.get_component_by_class(unreal.ProceduralMeshComponent)
            rows.append(dict(location=xyz(a.get_actor_location()),mesh=path(solid.get_editor_property('static_mesh')),replicated=a.get_editor_property('replicates'),
                planet_owned=a.get_owner()==planet,collision=str(solid.get_collision_enabled()),patch_collision=str(patch.get_collision_enabled()),
                conforming=patch.is_visible(),sections=patch.get_num_sections(),ground_error_cm=(planet.project_point_to_surface(a.get_actor_location()).impact_point-a.get_actor_location()).length()))
        target=Path(unreal.Paths.project_dir())/'Saved/MoonSurfaceV3/runtime_scatter.json'
        previous=json.loads(target.read_text(encoding='utf-8')) if target.exists() else []
        target.write_text(json.dumps(rows,indent=2),encoding='utf-8')
        return json.dumps(dict(count=len(rows),conforming=sum(r['conforming'] for r in rows),empty_patches=sum(r['conforming'] and not r['sections'] for r in rows),
            replicated=all(r['replicated'] and r['planet_owned'] for r in rows),max_ground_error_cm=max((r['ground_error_cm'] for r in rows),default=0),
            changed_from_previous_run=bool(previous) and any(not any(sum((a-b)**2 for a,b in zip(r['location'],q['location']))<1 for q in previous) for r in rows),
            towers=[dict(label=a.get_actor_label(),location=xyz(a.get_actor_location())) for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetColonyNest)]))
    if stage == 'weapon_surface_collisions':
        modules=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.StaticMeshActor) if a.actor_has_tag('MoonEcologyV3')]
        return json.dumps(dict(checked=len(modules),blocking=[a.get_actor_label() for a in modules if a.static_mesh_component.get_collision_enabled()!=unreal.CollisionEnabled.NO_COLLISION]))
    if stage == 'weapon_watch_surface':
        from pathlib import Path
        c=unreal._moon_surface_probe
        planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        class Watch:
            def __init__(self):
                self.started=unreal.GameplayStatics.get_time_seconds(world);self.rows=[];self.last=None;self.sample=-10
                self.handle=unreal.register_slate_post_tick_callback(self.tick)
            def tick(self,delta):
                elapsed=unreal.GameplayStatics.get_time_seconds(world)-self.started
                state=str(c.get_editor_property('state'))
                if state!=self.last or elapsed-self.sample>8:
                    self.last=state;self.sample=elapsed
                    self.rows.append(dict(seconds=elapsed,state=state,location=xyz(c.get_actor_location()),refuge_distance_cm=planet.approximate_surface_arc_distance(c.get_actor_location(),c.get_editor_property('refuge'))))
                    (Path(unreal.Paths.project_dir())/'Saved/MoonSurfaceV3/entrance_clearance.json').write_text(json.dumps(self.rows,indent=2),encoding='utf-8')
                if elapsed>90:unreal.unregister_slate_post_tick_callback(self.handle)
        unreal._weapon_entrance_watch=Watch()
        return json.dumps(dict(probe=c.get_name(),seconds=90))
    if stage == 'weapon_probe_reset':
        c=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceCreature) if a.get_name()=='BP_MirrorSkimmer_C_26')
        unreal._moon_surface_probe=c
        unreal.GameplayStatics.apply_damage(c,1,pc,pawn,None)
        return json.dumps(dict(probe=c.get_name(),location=xyz(c.get_actor_location())))
    if stage == 'weapon_open_symbolic':
        fixed=[]
        for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.StaticMeshActor):
            if a.actor_has_tag('MoonEcologyV3'):
                a.static_mesh_component.set_collision_profile_name('NoCollision');fixed.append(a.get_actor_label())
        return json.dumps(dict(runtime_profiles=len(fixed)))
    if stage == 'weapon_probe_traversal':
        import math
        c=unreal._moon_surface_probe
        planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        loc=c.get_actor_location();ref=c.get_editor_property('refuge');hab=c.get_editor_property('habitat')
        steering=c.get_component_by_class(unreal.JTSPlanetSurfaceSteeringComponent)
        up=planet.get_radial_up_vector(loc);heading=(ref-loc);heading=heading-up*heading.dot(up);heading.normalize()
        samples=[]
        for distance in (0,1,5,14,28,60,118):
            hit=planet.project_point_to_surface(loc+heading*distance)
            samples.append(dict(distance=distance,point=xyz(hit.impact_point),normal=xyz(hit.impact_normal),slope=math.degrees(math.acos(max(-1,min(1,hit.impact_normal.dot(planet.get_radial_up_vector(hit.impact_point))))))))
        end=planet.project_point_to_surface(loc+heading*28).impact_point
        surface=planet.get_editor_property('gameplay_surface_actor')
        hits=unreal.SystemLibrary.sphere_trace_multi_for_objects(world,loc+up*12,end+planet.get_radial_up_vector(end)*12,9,[unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1],True,[c,surface],unreal.DrawDebugTrace.NONE,True)
        obstacles=[]
        for h in (hits or []):
            v=h.to_tuple();actor=v[9];component=v[10]
            obstacles.append(dict(actor=actor.get_actor_label(),mesh=path(component.get_editor_property('static_mesh')),collision=str(component.get_collision_enabled()),profile=str(component.get_collision_profile_name()),tags=[str(t) for t in actor.tags]))
        return json.dumps(dict(creature=path(c),state=str(c.get_editor_property('state')),location=xyz(loc),refuge=xyz(ref),samples=samples,
            surface=path(surface),obstacles=obstacles,
            steering={k:steering.get_editor_property(k) for k in ('max_slope_degrees','probe_distance','body_radius')},
            habitat=dict(path=path(hab),location=xyz(hab.get_actor_location()),rotation=str(hab.get_actor_rotation()),props={k:str(hab.get_editor_property(k)) for k in ('spawn_radius','core_radius','minor_axis_ratio','boundary_phase','light_preference','terminator_margin')})))
    if stage == 'weapon_target':
        location=pc.player_camera_manager.get_camera_location()+pc.player_camera_manager.get_camera_rotation().get_forward_vector()*1500
        target=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat) if a.get_actor_label()=='Moon_Nest_06')
        # Move an existing damageable actor only inside disposable PIE, leaving the authored level untouched.
        target.get_component_by_class(unreal.StaticMeshComponent).set_mobility(unreal.ComponentMobility.MOVABLE)
        target.set_actor_location(location-(target.get_component_by_class(unreal.BoxComponent).get_world_location()-target.get_actor_location()),False,True)
        unreal._weapon_test_target=target
        return json.dumps(dict(target=path(target),health=target.get_component_by_class(unreal.JTSHealthComponent).get_health()))
    if stage == 'weapon_target_health':
        return json.dumps(dict(health=unreal._weapon_test_target.get_component_by_class(unreal.JTSHealthComponent).get_health()))
    if stage == 'weapon_entrances':
        fixed=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat) if a.get_actor_label().startswith('Moon_Nest_')]
        return json.dumps([dict(name=a.get_actor_label(),destructible=a.get_editor_property('destructible_entrance'),
            replicated=a.get_editor_property('replicates'),health=a.get_component_by_class(unreal.JTSHealthComponent).get_health(),
            mesh_collision=str(a.get_editor_property('refuge_mesh').get_collision_enabled()),
            target_collision=[str(c.get_collision_enabled()) for c in a.get_components_by_class(unreal.BoxComponent)]) for a in fixed])
    if stage == 'weapon_destroy_entrance':
        a=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat) if a.get_actor_label()=='Moon_Nest_05')
        unreal.GameplayStatics.apply_damage(a,1000,pc,pawn,None)
        return json.dumps(dict(health=a.get_component_by_class(unreal.JTSHealthComponent).get_health(),visible=a.get_editor_property('refuge_mesh').is_visible(),
            target_collision=[str(c.get_collision_enabled()) for c in a.get_components_by_class(unreal.BoxComponent)]))
    if stage == 'weapon_field':
        planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        entrance=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat) if a.get_actor_label()=='Moon_Nest_05')
        hit=planet.project_point_to_surface(entrance.get_actor_location()+entrance.get_actor_forward_vector()*650)
        up=planet.get_radial_up_vector(hit.impact_point)
        pawn.get_movement_component().stop_movement_immediately()
        pawn.set_actor_location(hit.impact_point+up*101,False,True)
        return 'Player moved onto open Moon surface for weapon validation'
    if stage == 'weapon_aim':ranged.start_aim()
    if stage == 'weapon_hip':ranged.stop_aim()
    if stage == 'weapon_fire':
        ranged.start_fire();ranged.stop_fire()
    if stage == 'weapon_shot':
        unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI',pc)
        return 'Requested PIE screenshot including HUD'
    if stage in ('weapon_summary','weapon_aim','weapon_hip','weapon_fire'):
        components={c.get_name():c for c in pawn.get_components_by_class(unreal.SceneComponent)}
        anchor=components['WeaponHandAttachmentAnchor'];muzzle=components['WeaponMuzzle']
        target=ranged.get_camera_aim_point()
        wrist=mesh.get_socket_location('Wrist_R');finger=mesh.get_socket_location('Index1_R')
        direction=muzzle.get_forward_vector();error=(target-muzzle.get_world_location()).cross(direction).length() if target else None
        widgets=[o for o in unreal.ObjectIterator() if isinstance(o,unreal.TextBlock) and o.get_name()=='CrosshairText']
        reticles=[]
        for crosshair in widgets:
            reticles.append(dict(visibility=str(crosshair.get_visibility()),text=str(crosshair.get_text()),parent_visibility=str(crosshair.get_parent().get_visibility())))
        return json.dumps(dict(aiming=ranged.get_editor_property('is_aiming'),ammo=ranged.get_ammo(),
            weapon_parent=path(anchor.get_attach_parent()),palm_error_cm=(anchor.get_world_location()-(wrist+(finger-wrist)*1.35)).length(),
            barrel_target_error_cm=error,target=xyz(target) if target else None,
            camera=xyz(pc.player_camera_manager.get_camera_location()),camera_forward=xyz(pc.player_camera_manager.get_camera_rotation().get_forward_vector()),
            muzzle=xyz(muzzle.get_world_location()),muzzle_forward=xyz(direction),reticles=reticles))
    data=dict(pawn=path(pawn), mesh=inspect_mesh(mesh),active_item=str(inv.get_active_item_id()),
        ranged=ranged.has_active_ranged_weapon(),aiming=ranged.get_editor_property('is_aiming'),
        control_rotation=str(pc.get_control_rotation()),pawn_rotation=str(pawn.get_actor_rotation()),
        location=xyz(pawn.get_actor_location()))
    data['bones']={n:xyz(mesh.get_socket_location(n)) for n in ('Head','UpperArm_R','LowerArm_R','Wrist_R','Index1_R')}
    data['weapon']=[dict(name=c.get_name(),parent=path(c.get_attach_parent()),socket=str(c.get_attach_socket_name()),
        location=xyz(c.get_world_location()),rotation=str(c.get_world_rotation()),visible=c.is_visible())
        for c in pawn.get_components_by_class(unreal.SceneComponent) if 'Weapon' in c.get_name() or 'HandAttachment' in c.get_name()]
    return json.dumps(data)
