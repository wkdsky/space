"""Focused editor operations and runtime inspection, invoked through MoonSurfaceV3Tools MCP."""
import json
import unreal
import moon_surface_unreal as s


def floor_depth(surface, attachment):
    origin=attachment.get_actor_location();up=attachment.get_actor_up_vector()
    result=surface.line_trace_component(origin+up*200,origin-up*300,True,False,False)
    assert result
    return unreal.MathLibrary.dot_vector_vector(origin-result[0],up)


def run(stage):
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if stage=='material_probe':
        rows=[]
        for key in ('M_MS3_Void','M_MS3_PropStone'):
            m=unreal.load_asset(s.MATERIALS+'/'+key)
            node=unreal.MaterialEditingLibrary.get_material_property_input_node(m,unreal.MaterialProperty.MP_BASE_COLOR)
            rows.append(dict(name=key,model=str(m.get_editor_property('shading_model')),base_node=str(node),two_sided=m.get_editor_property('two_sided')))
        return json.dumps(rows)
    if stage in ('stand_mirror','stand_rift'):
        planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        if stage=='stand_mirror':
            landmark=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat) if a.get_actor_label()=='Moon_Nest_01')
            offset=landmark.get_actor_forward_vector()*300+landmark.get_actor_right_vector()*90
        else:
            sun=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DirectionalLight) if a.get_actor_label()=='Sun_Main')
            fissures=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceAttachment) if 'RiftRim' in a.get_component_by_class(unreal.StaticMeshComponent).static_mesh.get_name()]
            landmark=max(fissures,key=lambda a:unreal.MathLibrary.dot_vector_vector((a.get_actor_location()-planet.get_planet_center()).normal(),-sun.get_actor_forward_vector()))
            offset=landmark.get_actor_right_vector()*310+landmark.get_actor_forward_vector()*40
        frame=planet.get_surface_frame_at(landmark.get_actor_location()+offset,-offset)
        pose=frame.transform;pose.translation=frame.location+frame.up*pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
        pawn.get_movement_component().stop_movement_immediately();pawn.get_movement_component().disable_movement();pawn.set_actor_transform(pose,False,True)
        location=pawn.get_actor_location()+frame.up*74-pawn.get_actor_forward_vector()*220+pawn.get_actor_right_vector()*75
        target=landmark.get_actor_location()+landmark.get_actor_up_vector()*15
        forward=(target-location).normal();view_up=(frame.up-forward*unreal.MathLibrary.dot_vector_vector(frame.up,forward)).normal()
        rotation=unreal.MathLibrary.make_rotation_from_axes(forward,unreal.MathLibrary.cross_vector_vector(view_up,forward),view_up)
        lib=unreal.get_default_object(unreal.GameplayStatics.static_class());camera_pose=unreal.Transform(location=location,rotation=rotation)
        camera=lib.call_method('BeginDeferredActorSpawnFromClass',(world,unreal.CameraActor.static_class(),camera_pose,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        camera=lib.call_method('FinishSpawningActor',(camera,camera_pose,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT));camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(65)
        pc.set_view_target_with_blend(camera,0);unreal._moon_opening_player_camera=camera
        return json.dumps(dict(player=pawn.get_name(),opening=landmark.get_name(),camera_height_cm=170))
    if stage=='summary':
        result=json.loads(run('cuts'))
        return json.dumps(dict(fissures=len(result['fissures']),mouths=len(result['mouths']),
            fissure_depth_cm=[min(x['depth_cm'] for x in result['fissures']),max(x['depth_cm'] for x in result['fissures'])],
            mouth_depth_cm=[min(x['depth_cm'] for x in result['mouths']),max(x['depth_cm'] for x in result['mouths'])],triangles=result['triangles']))
    if stage=='mouth_probe':
        planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        cuts=planet.get_editor_property('surface_cuts');rows=[]
        for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat):
            if not a.get_actor_label().startswith('Moon_Nest_'):continue
            outside=planet.project_point_to_surface(a.get_actor_location()+a.get_actor_forward_vector()*220)
            source=planet.get_gameplay_surface_actor().get_component_by_class(unreal.StaticMeshComponent)
            original=source.line_trace_component(a.get_actor_location()+a.get_actor_up_vector()*250,a.get_actor_location()-a.get_actor_up_vector()*300,True,False,False)
            rows.append(dict(label=a.get_actor_label(),
                original_mesh_visible=a.get_editor_property('refuge_mesh').is_visible(),transform=str(a.get_actor_transform()),
                normal_alignment=unreal.MathLibrary.dot_vector_vector(a.get_actor_up_vector(),outside.impact_normal),
                original_direct_trace=str(original)))
        return json.dumps(dict(cuts=cuts.get_cut_count(),runtime=str(cuts.get_editor_property('runtime_surface')),mouths=rows))
    if stage=='opening_grid':
        a=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceAttachment)
            if a.get_component_by_class(unreal.StaticMeshComponent).static_mesh.get_name()=='SM_MS3_FixedNest_01')
        planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        surface=planet.get_editor_property('surface_cuts').get_editor_property('runtime_surface')
        rows=[]
        for x in range(-120,121,30):
            values=[]
            for y in range(-120,121,30):
                p=a.get_actor_location()+a.get_actor_forward_vector()*x+a.get_actor_right_vector()*y;up=a.get_actor_up_vector()
                result=surface.line_trace_component(p+up*250,p-up*300,True,False,False)
                values.append(round(unreal.MathLibrary.dot_vector_vector(p-result[0],up),1) if result else None)
            rows.append(values)
        return json.dumps(dict(depth_grid_cm=rows,mesh_bounds=str(a.get_component_by_class(unreal.StaticMeshComponent).static_mesh.get_bounds()),patch_bounds=str(a.get_actor_bounds(False))))
    if stage=='mouth_destroy':
        planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        cuts=planet.get_editor_property('surface_cuts')
        mouth=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat) if a.get_actor_label()=='Moon_Nest_01')
        point=mouth.get_actor_location();before=cuts.get_cut_count()
        unreal.GameplayStatics.apply_damage(mouth,1000,None,None,None)
        assert cuts.rebuild_now()
        hit=planet.project_point_to_surface(point)
        depth=unreal.MathLibrary.dot_vector_vector(point-hit.impact_point,mouth.get_actor_up_vector())
        assert abs(depth)<2,depth
        assert cuts.get_cut_count()==before-1
        return json.dumps(dict(fixed_mouth_destroyed=True,surface_restored_error_cm=depth,other_cuts_preserved=cuts.get_cut_count()))
    if stage=='mouth_targets':
        rows=[]
        for mouth in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat):
            if not mouth.get_actor_label().startswith('Moon_Nest_'):continue
            box=next(c for c in mouth.get_components_by_class(unreal.BoxComponent) if c.get_name()=='EntranceTarget')
            aim=mouth.get_actor_location()+mouth.get_actor_up_vector()*25
            hit=box.line_trace_component(aim+mouth.get_actor_forward_vector()*220,aim,True,False,False)
            assert hit,mouth.get_actor_label()
            rows.append(mouth.get_actor_label())
        assert len(rows)==9
        return json.dumps(dict(all_nine_cave_openings_targetable=True,mouths=rows))
    if stage=='editor':
        assert not world
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/Space/Maps/L_SpaceWorld')
        return 'Restored SpaceWorld in the editor'
    if stage=='depth_diagnostic':
        planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        surface=planet.get_editor_property('surface_cuts').get_editor_property('runtime_surface')
        return json.dumps([dict(name=a.get_name(),depth_cm=floor_depth(surface,a)) for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceAttachment) if 'RiftRim' in a.get_component_by_class(unreal.StaticMeshComponent).static_mesh.get_name()])
    if stage=='import':
        s.editor_world()
        rows=json.loads((s.ART/'cut_modules.json').read_text(encoding='utf-8'))
        s.import_tasks([s.fbx_task(s.ART/'FBX'/(r['name']+'.fbx'),s.MESHES,r['name']) for r in rows])
        for row in rows:
            mesh=unreal.load_asset(s.MESHES+'/'+row['name']);slots=list(mesh.static_materials)
            for index,slot in enumerate(slots):
                # Real cavities use rock on every wall and floor. Darkness is
                # produced by occlusion, not the old painted-hole material.
                key='M_MS3_PropStone'
                slot.material_slot_name=unreal.Name(key);slot.material_interface=unreal.load_asset(s.MATERIALS+'/'+key)
            mesh.set_editor_property('static_materials',slots);mesh.set_editor_property('allow_cpu_access',True)
            unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(mesh);s.save(mesh)
        source=s.actors()['MoonPlanet'].get_component_by_class(unreal.StaticMeshComponent).static_mesh
        path=s.MESHES+'/SM_MoonCutSource'
        cached=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(),path)
        cached.set_editor_property('allow_cpu_access',True);s.save(cached)
        return json.dumps(dict(imported=len(rows),cut_source=cached.get_path_name(),original_asset=source.get_path_name()))
    if stage=='configure':
        s.editor_world();state=s.actors()
        planet=next(a for a in state.values() if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='Moon')
        planet.get_editor_property('surface_cuts').set_editor_property('cut_source_mesh',unreal.load_asset(s.MESHES+'/SM_MoonCutSource'))
        cores=[a for a in state.values() if isinstance(a,unreal.JTSPlanetEcologyHabitat) and a.get_actor_label().endswith('_Rift_Core')]
        removed=0
        for core in cores:
            scatter=core.get_editor_property('surface_scatter');entries=[]
            for entry in scatter.get_editor_property('entries'):
                meshes=entry.get_editor_property('meshes')
                if any('SM_MS3_Rock_' in m.get_name() for m in meshes):
                    removed+=entry.get_editor_property('count');continue
                if entry.get_editor_property('refuge'):
                    entry.set_editor_property('meshes',[unreal.load_asset(s.MESHES+f'/SM_MS3_RiftRim_{i}') for i in range(4)])
                    entry.set_editor_property('cut_volumes',[unreal.load_asset(s.MESHES+f'/SM_MS3_RiftCutVolume_{i}') for i in range(4)])
                    entry.set_editor_property('conform_to_surface',True);entry.set_editor_property('destructible',True)
                entries.append(entry)
            scatter.set_editor_property('entries',entries)
        bp=unreal.load_asset(s.BPS+'/BP_MoonClosedColony')
        unreal.get_default_object(bp.generated_class()).set_editor_property('height_range',unreal.Vector2D(95,132))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp);s.save(bp)
        for nest in state.values():
            if isinstance(nest,unreal.JTSPlanetColonyNest):nest.set_editor_property('height_range',unreal.Vector2D(95,132))
        for label,a in state.items():
            if label.startswith('Moon_Nest_'):
                suffix=label.rsplit('_',1)[-1]
                a.set_editor_property('refuge_cut_volume',unreal.load_asset(s.MESHES+'/SM_MS3_FixedCutVolume_'+suffix))
                a.get_editor_property('refuge_mesh').set_static_mesh(unreal.load_asset(s.MESHES+'/SM_MS3_FixedNest_'+suffix))
        feeding=unreal.load_asset(s.BPS+'/BP_MoonMirrorFeedingArea')
        scatter=unreal.get_default_object(feeding.generated_class()).get_editor_property('surface_scatter')
        entries=list(scatter.get_editor_property('entries'))
        for e in entries:
            if not e.get_editor_property('refuge'):continue
            e.set_editor_property('meshes',[unreal.load_asset(s.MESHES+f'/SM_MS3_MirrorRefuge_{i}') for i in range(4)])
            e.set_editor_property('cut_volumes',[unreal.load_asset(s.MESHES+f'/SM_MS3_MirrorCutVolume_{i}') for i in range(4)])
            e.set_editor_property('conform_to_surface',True);e.set_editor_property('cast_shadow',True);e.set_editor_property('destructible',False)
        scatter.set_editor_property('entries',entries)
        unreal.BlueprintEditorLibrary.compile_blueprint(feeding);s.save(feeding)
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        return json.dumps(dict(regions=len(cores),fixed_burrow_cuts=9,mirror_scatter_cuts=True,removed_primitive_rocks=removed,tower_height_cm=[95,132]))
    if stage=='cuts':
        assert world
        planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        cuts=planet.get_editor_property('surface_cuts');surface=cuts.get_editor_property('runtime_surface');assert surface
        fissures=[];mouths=[]
        for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceAttachment):
            mesh=a.get_component_by_class(unreal.StaticMeshComponent).static_mesh
            assert not mesh.get_name().startswith('SM_MS3_Rock_'),mesh.get_name()
            is_fissure='RiftRim' in mesh.get_name()
            if not is_fissure and not any(k in mesh.get_name() for k in ('MirrorRefuge','FixedNest')):continue
            assert a.is_refuge_usable(),a.get_name()
            hit=planet.project_point_to_surface(a.get_actor_location());assert hit
            radial_depth=unreal.MathLibrary.dot_vector_vector(a.get_actor_location()-hit.impact_point,a.get_actor_up_vector())
            assert radial_depth>1,(a.get_name(),radial_depth)
            depth=floor_depth(surface,a)
            assert depth>40,(a.get_name(),depth)
            row=dict(actor=a.get_path_name(),mesh=mesh.get_name(),depth_cm=depth,radial_first_hit_depth_cm=radial_depth,collision=hit.hit_component.get_path_name())
            (fissures if is_fissure else mouths).append(row)
        towers=[]
        for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetColonyNest):
            chosen=a.get_editor_property('chosen_height');assert 95<=chosen<=132
            mesh=a.get_component_by_class(unreal.StaticMeshComponent)
            top=mesh.get_local_bounds()[1].z*a.get_actor_scale3d().z
            assert abs(top-chosen)<.1
            towers.append(dict(name=a.get_name(),height_cm=top))
        assert len(fissures)==16
        assert len(fissures)+len(mouths)==cuts.get_cut_count()
        assert sum('FixedNest' in m['mesh'] for m in mouths)==9
        result=dict(fissures=fissures,mouths=mouths,towers=towers,triangles=surface.get_dynamic_mesh().get_triangle_count())
        (s.QA/'true_cut_validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        return json.dumps(result)
    if stage=='state':
        planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        cuts=planet.get_editor_property('surface_cuts')
        animals=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceAttachment)
        a=next(a for a in animals if 'RiftRim' in a.get_component_by_class(unreal.StaticMeshComponent).static_mesh.get_name())
        core=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat) if a.get_actor_label().endswith('_Rift_Core'))
        return json.dumps(dict(source=str(cuts.get_editor_property('cut_source_mesh')),count=cuts.get_cut_count(),planet=str(planet),component=str(planet.get_gameplay_surface_component()),
            core_volumes=[[str(v) for v in e.get_editor_property('cut_volumes')] for e in core.get_editor_property('surface_scatter').get_editor_property('entries')]))
    if stage in ('move','destroy','destroy_check','tower_reference','tower_damage'):
        planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        cuts=planet.get_editor_property('surface_cuts')
        if stage=='move':
            a=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceAttachment)
                if 'RiftRim' in a.get_component_by_class(unreal.StaticMeshComponent).static_mesh.get_name())
            original=a.get_actor_location();up=a.get_actor_up_vector()
            destination=original+a.get_actor_forward_vector()*800
            frame=planet.get_surface_frame_at(destination,a.get_actor_forward_vector());pose=frame.transform;pose.scale3d=a.get_actor_scale3d()
            unreal._moon_cut_probe=a;unreal._moon_cut_destination=frame.location
            unreal._moon_cut_residents=[c for c in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceCreature) if c.get_editor_property('refuge_actor')==a]
            a.set_actor_transform(pose,False,True);assert cuts.rebuild_now()
            restored=planet.project_point_to_surface(original);floor=planet.project_point_to_surface(frame.location)
            assert (restored.impact_point-original).length()<1
            depth=floor_depth(cuts.get_editor_property('runtime_surface'),a);assert depth>40
            return json.dumps(dict(old_position_restored=True,new_depth_cm=depth,cut_count=cuts.get_cut_count()))
        if stage=='destroy':
            a=unreal._moon_cut_probe;point=unreal._moon_cut_destination
            unreal.GameplayStatics.apply_damage(a,1000,None,None,None);assert cuts.rebuild_now()
            hit=planet.project_point_to_surface(point);assert (hit.impact_point-point).length()<1
            return json.dumps(dict(destroyed_position_restored=True,remaining_cuts=cuts.get_cut_count()))
        if stage=='destroy_check':
            residents=unreal._moon_cut_residents
            assert all(c.get_editor_property('refuge_actor') is None and c.get_editor_property('state') not in (unreal.JTSForagerState.HIDDEN,unreal.JTSForagerState.BURROW) for c in residents)
            return json.dumps(dict(residents_stopped_using_destroyed_fissure=len(residents)))
        nests=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetColonyNest)
        if stage=='tower_damage':
            nest=nests[0];height=nest.get_editor_property('chosen_height')
            unreal.GameplayStatics.apply_damage(nest,15,None,None,None)
            top=nest.get_component_by_class(unreal.StaticMeshComponent).get_local_bounds()[1].z*nest.get_actor_scale3d().z
            assert abs(top-height)<.1
            return json.dumps(dict(damaged_tower_height_cm=top,chosen_height_cm=height))
        nest=nests[0];unreal._moon_tower_reference=nest
        pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        frame=planet.get_surface_frame_at(nest.get_actor_location()+nest.get_actor_right_vector()*180,nest.get_actor_forward_vector())
        pose=frame.transform;pose.translation=frame.location+frame.up*pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
        pawn.get_movement_component().stop_movement_immediately();pawn.get_movement_component().disable_movement();pawn.set_actor_transform(pose,False,True)
        return json.dumps(dict(tower=nest.get_name(),height_cm=nest.get_editor_property('chosen_height'),reference_player=pawn.get_name()))
    if stage=='audit':
        assert world
        pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        rows=[]
        for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor):
            distance=(a.get_actor_location()-pawn.get_actor_location()).length()
            if distance>1800:continue
            meshes=a.get_components_by_class(unreal.StaticMeshComponent)
            rows.append(dict(actor=a.get_path_name(),label=a.get_actor_label(),type=a.get_class().get_name(),distance=distance,
                scale=s.xyz(a.get_actor_scale3d()),meshes=[dict(name=c.static_mesh.get_path_name(),bounds=str(c.get_local_bounds())) for c in meshes if c.static_mesh]))
        planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        mesh=planet.get_gameplay_surface_actor().get_component_by_class(unreal.StaticMeshComponent).static_mesh
        skeletal=pawn.get_component_by_class(unreal.SkeletalMeshComponent)
        bones=[str(skeletal.get_bone_name(i)) for i in range(skeletal.get_num_bones())]
        up=pawn.get_actor_up_vector();feet=pawn.get_actor_location()-up*pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
        heights={b:unreal.MathLibrary.dot_vector_vector(skeletal.get_socket_location(b)-feet,up) for b in bones}
        report=dict(nearby=rows,pawn=pawn.get_class().get_name(),pawn_transform=str(pawn.get_actor_transform()),bones_cm=heights,
            capsule_halfheight=pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height(),
            moon_mesh=mesh.get_path_name(),moon_cpu=mesh.get_editor_property('allow_cpu_access'),moon_bounds=str(mesh.get_bounds()),
            towers=[dict(name=a.get_name(),scale=s.xyz(a.get_actor_scale3d()),mesh=a.get_component_by_class(unreal.StaticMeshComponent).static_mesh.get_path_name()) for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetColonyNest)])
        (s.QA/'repair_audit.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        return json.dumps(report)
    if stage=='inventory':
        grouped={}
        for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor):
            if a==unreal.GameplayStatics.get_player_pawn(world,0):continue
            for c in a.get_components_by_class(unreal.StaticMeshComponent):
                if not c.static_mesh:continue
                key=a.get_class().get_name()+' / '+c.static_mesh.get_name()
                grouped[key]=grouped.get(key,0)+1
        return json.dumps(grouped)
    raise ValueError(stage)
