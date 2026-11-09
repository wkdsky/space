"""Runtime encounter and local-map checks through Unreal MCP; no runtime scene saves."""
import json, math, time
import unreal
from mirror_boss_validation import worlds, get, server
from mirror_boss_editor import world, QA, xyz

GS=unreal.GameplayStatics


def inspect(w):
    enc=get(w,unreal.JTSNestBossEncounter)[0]; planet=enc.get_editor_property('planet')
    center=enc.get_actor_location(); up=planet.get_radial_up_vector(center)
    forward=enc.get_actor_forward_vector(); forward=(forward-up*forward.dot(up)).normal(); right=up.cross(forward)
    rows=[]
    for nest in enc.get_editor_property('nests'):
        delta=nest.get_actor_location()-center
        mesh=nest.get_editor_property('refuge_mesh')
        rows.append(dict(name=nest.get_actor_label(),location=xyz(nest.get_actor_location()),
            angle=math.degrees(math.atan2(delta.dot(right),delta.dot(forward)))%360,
            radius=math.hypot(delta.dot(right),delta.dot(forward)),visible=mesh.is_visible(),
            mesh_location=xyz(mesh.get_world_location()),blocked=nest.is_entrance_blocked()))
    angles=sorted(r['angle'] for r in rows)
    return dict(world=w.get_path_name(),center=xyz(center),planet=xyz(planet.get_actor_location()),
        nests=rows,gaps=[(angles[(i+1)%len(angles)]-a)%360 for i,a in enumerate(angles)],
        ridge_sections=enc.ridges.get_num_sections(),rotation=str(enc.get_actor_rotation()),
        players=[dict(name=p.get_name(),location=xyz(p.get_actor_location())) for p in get(w,unreal.JTSCharacter)])


def run(stage):
    if stage=='boss_map_ready':
        w=world();manager=get(w,unreal.JTSSpaceWorldManager)[0]
        settings=manager.get_editor_property('navigation_map_settings')
        mode=w.get_world_settings().get_editor_property('default_game_mode')
        return dict(world=w.get_path_name(),mode=mode.get_path_name(),
            material=str(settings.get_editor_property('ground_relief_material')),
            require_matching_build=unreal.get_default_object(mode).get_editor_property('require_matching_build_version'),
            pie=bool(worlds()))
    if stage=='boss_map_state':
        return [dict(world=w.get_path_name(),mode=str(GS.get_game_mode(w)),
            phase=str(GS.get_game_state(w).get_gameplay_phase()) if isinstance(GS.get_game_state(w),unreal.JTSGameState) else None,
            pcs=len(get(w,unreal.PlayerController)),pawns=len(get(w,unreal.JTSCharacter)),
            controllers=[a.get_path_name() for a in get(w,unreal.JTSMoonSurfaceController)]) for w in worlds()]
    if stage=='boss_map_material':
        import surface_map_relief, importlib
        return importlib.reload(surface_map_relief).configure()
    if stage=='boss_map_sync_manifest':
        assert not worlds()
        from mirror_boss_editor import save, protected
        before=protected(); enc=get(world(),unreal.JTSNestBossEncounter)[0]
        nests={n.get_name():n for n in enc.get_editor_property('nests')}
        data=next(iter(nests.values())).get_editor_property('surface_scatter').get_editor_property('distribution_data')
        poses=list(data.get_editor_property('actor_poses')); layouts=list(data.get_editor_property('scatter_layouts'))
        updated=[0,0]
        for row in poses:
            nest=nests.get(str(row.get_editor_property('actor_name')))
            if nest:
                row.set_editor_property('pose',nest.get_actor_transform()); updated[0]+=1
        for row in layouts:
            nest=nests.get(str(row.get_editor_property('key')))
            if nest:
                assert len(row.get_editor_property('props'))==0, 'Nested props require explicit surface relocation'
                row.set_editor_property('owner_pose',nest.get_actor_transform()); updated[1]+=1
        assert updated==[7,7],updated
        data.modify();data.set_editor_property('actor_poses',poses);data.set_editor_property('scatter_layouts',layouts);save(data)
        enc.rebuild_ridges()
        assert before==protected()
        return dict(updated=updated,asset=data.get_path_name(),protected=True)
    if stage=='boss_map_manifest':
        enc=get(world(),unreal.JTSNestBossEncounter)[0]
        data=enc.get_editor_property('nests')[0].get_editor_property('surface_scatter').get_editor_property('distribution_data')
        names=[n.get_name() for n in enc.get_editor_property('nests')]
        return dict(asset=data.get_path_name(),radius=enc.get_editor_property('planet').get_approximate_radius(),
            poses=[dict(name=str(p.get_editor_property('actor_name')),location=xyz(p.get_editor_property('pose').translation)) for p in data.get_editor_property('actor_poses') if str(p.get_editor_property('actor_name')) in names],
            layouts=[dict(key=str(p.get_editor_property('key')),props=len(p.get_editor_property('props')),location=xyz(p.get_editor_property('owner_pose').translation)) for p in data.get_editor_property('scatter_layouts') if str(p.get_editor_property('key')) in names])
    if stage=='boss_map_inspect':
        ws=[w for w in worlds() if get(w,unreal.JTSNestBossEncounter)] or [world()]
        rows=[inspect(w) for w in ws]
        QA.mkdir(parents=True,exist_ok=True)
        (QA/('RuntimeRing.json' if worlds() else 'EditorRing.json')).write_text(json.dumps(rows,indent=2),encoding='utf8')
        return rows
    if stage=='boss_map_setup':
        from mirror_boss_feedback_revision import run as feedback
        feedback('boss_feedback_setup')
        unreal.get_default_object(unreal.JTSExpeditionSubsystem).set_editor_property('save_slot_prefix','MoonRuntimeMap_QA_'+str(int(time.time())))
        cdo=unreal.get_default_object(unreal._boss_feedback_mode)
        if not hasattr(unreal,'_moon_map_build'):
            unreal._moon_map_build=cdo.get_editor_property('require_matching_build_version')
        cdo.set_editor_property('require_matching_build_version',False)
        return dict(mode=str(unreal._boss_feedback_mode))
    if stage=='boss_map_travel':
        w=server();pc=GS.get_player_controller(w,0)
        assert len(get(w,unreal.PlayerController))==2
        # Connect in the existing lobby fixture, then travel both clients through
        # the saved map's real GameMode and its normal surface initialization.
        mode=unreal._boss_feedback_mode.get_path_name()
        unreal.SystemLibrary.execute_console_command(w,'net.AllowPIESeamlessTravel 1',pc)
        unreal.SystemLibrary.execute_console_command(w,'defer ServerTravel /Game/Space/Maps/L_SpaceWorld?game='+mode,pc)
        return dict(travelling=True,mode=mode)
    if stage=='boss_map_close':
        if hasattr(unreal,'_moon_map_build'):
            unreal.get_default_object(unreal._boss_feedback_mode).set_editor_property('require_matching_build_version',unreal._moon_map_build)
        from mirror_boss_feedback_revision import run as feedback
        return feedback('boss_feedback_close')
    if stage=='boss_map_configure_style':
        assert not worlds()
        from mirror_boss_editor import save
        mat=unreal.load_asset('/Game/Space/UI/Navigation/M_SurfaceMapRelief')
        manager=get(world(),unreal.JTSSpaceWorldManager)[0]
        settings=manager.get_editor_property('navigation_map_settings')
        settings.set_editor_property('ground_relief_material',mat)
        manager.modify();manager.set_editor_property('navigation_map_settings',settings)
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        return dict(material=mat.get_path_name())
    if stage=='boss_map_assert_runtime':
        expected=json.loads((QA/'EditorRing.json').read_text(encoding='utf8'))[0]
        wanted={n['name']:n['location'] for n in expected['nests']}
        rows=[]
        for w in worlds():
            if not get(w,unreal.JTSNestBossEncounter):continue
            row=inspect(w)
            assert len(row['nests'])==7 and row['ridge_sections']==7,row
            assert max(abs(g-360/7) for g in row['gaps'])<.01,row['gaps']
            for nest in row['nests']:
                assert unreal.Vector(*nest['location']).distance(unreal.Vector(*wanted[nest['name']]))<.2,nest
            rows.append(dict(world=w.get_path_name(),gaps=row['gaps'],ridges=row['ridge_sections'],players=len(row['players'])))
        assert len(rows)==2,rows
        (QA/'RuntimeRingVerified.json').write_text(json.dumps(rows,indent=2))
        return rows
    if stage=='boss_map_ui':
        rows=[]
        library=unreal.get_default_object(unreal.load_class(None,'/Script/UMG.WidgetBlueprintLibrary'))
        for w in worlds():
            huds=library.call_method('GetAllWidgetsOfClass',(w,unreal.JTSPrototypeHUDWidget.static_class(),False))
            for hud in huds:
                pc=hud.get_owning_player()
                if not pc or not pc.is_local_controller():continue
                nav=unreal.find_object(None,hud.get_path_name()+'.WidgetTree.CruiseNavigation')
                resource=next((r for r in unreal.ObjectIterator(unreal.Object) if r.get_outer()==nav and r.get_name().startswith('JTSSurfaceMapCapture')),None)
                if not resource:continue
                captures=[c for c in unreal.ObjectIterator(unreal.SceneCaptureComponent2D) if c.get_outer()==resource]
                capture=next((c for c in captures if c.get_editor_property('capture_source')==unreal.SceneCaptureSource.SCS_BASE_COLOR),None)
                depth=next((c for c in captures if c.get_editor_property('capture_source')==unreal.SceneCaptureSource.SCS_SCENE_DEPTH),None)
                if not capture:continue
                up=get(w,unreal.JTSNestBossEncounter)[0].get_editor_property('planet').get_radial_up_vector(pc.get_controlled_pawn().get_actor_location())
                view=unreal.MathLibrary.get_forward_vector(pc.player_camera_manager.get_camera_rotation())
                forward=(view-up*view.dot(up)).normal(); right=up.cross(forward)
                rows.append(dict(world=w.get_path_name(),stage=nav.get_navigation_stage(),
                    right_dot=capture.get_right_vector().dot(right),forward_dot=capture.get_up_vector().dot(forward),
                    depth=bool(depth),materials=[r.get_name() for r in unreal.ObjectIterator(unreal.MaterialInstanceDynamic) if r.get_outer()==resource],
                    player_map=str(nav.get_player_map_position())))
        (QA/'RuntimeMapUI.json').write_text(json.dumps(rows,indent=2))
        assert len(rows)==2,rows
        assert all(r['right_dot']>.999 and r['forward_dot']>.999 and r['depth'] and r['materials'] for r in rows),rows
        return rows
    if stage=='boss_map_client_shot':
        w=next(w for w in worlds() if not GS.get_game_mode(w))
        pc=GS.get_player_controller(w,0)
        unreal.SystemLibrary.execute_console_command(w,'Shot SHOWUI',pc)
        return dict(client_screenshot=True)
    if stage=='boss_map_moon':
        w=server(); pc=GS.get_player_controller(w,0)
        unreal.SystemLibrary.execute_console_command(w,'defer ke '+pc.get_path_name()+' ServerRequestDebugPlanet Moon false',pc)
        return dict(queued=True)
    if stage=='boss_map_capture':
        from pickaxe_combat_validation import capture
        w=server(); enc=get(w,unreal.JTSNestBossEncounter)[0]; planet=enc.get_editor_property('planet')
        center=enc.get_actor_location(); up=planet.get_radial_up_vector(center)
        return dict(image=capture(w,center+up*7500,center,enc.get_actor_forward_vector(),'MoonRuntimeRing'))
    raise ValueError(stage)

