"""Scoped crater layout and mirror feedback authoring through Unreal MCP."""
import json, math
import unreal
from mirror_boss_editor import actors, world, xyz, protected, BPS, QA, save


def encounter():
    return next(a for a in actors() if isinstance(a, unreal.JTSNestBossEncounter))


def inspect():
    if not any(isinstance(a, unreal.JTSNestBossEncounter) for a in actors()):
        return dict(world=world().get_path_name(), actors=[a.get_actor_label() for a in actors()],
                    pie=[w.get_path_name() for w in unreal.EditorLevelLibrary.get_pie_worlds(True)])
    enc = encounter()
    center = enc.get_actor_location()
    planet = enc.get_editor_property('planet')
    up = planet.get_radial_up_vector(center)
    forward = enc.get_actor_forward_vector()
    forward = (forward-up*forward.dot(up)).normal()
    right = up.cross(forward).normal()
    rows = []
    for nest in enc.get_editor_property('nests'):
        delta = nest.get_actor_location() - center
        x, y = delta.dot(forward), delta.dot(right)
        rows.append(dict(label=nest.get_actor_label(), location=xyz(nest.get_actor_location()),
                         angle=math.degrees(math.atan2(y, x)) % 360,
                         radius=math.hypot(x, y), height=delta.dot(up),
                         bounds=str(nest.get_actor_bounds(False))))
    angles = sorted(row['angle'] for row in rows)
    mirrors = []
    for name in ['BP_MirrorSkimmer', 'BP_MirrorBoss']:
        cdo = unreal.get_default_object(unreal.load_asset(BPS+'/'+name).generated_class())
        component = cdo.get_component_by_class(unreal.JTSMirrorDefenseComponent)
        mirrors.append(dict(name=name, leash=cdo.get_editor_property('leash_radius') if name=='BP_MirrorSkimmer' else None,
                            mirror={p:str(component.get_editor_property(p)) for p in ['glare_material','glare_range','glare_half_angle']} if component else None))
    result = dict(world=world().get_path_name(), pie=[w.get_path_name() for w in unreal.EditorLevelLibrary.get_pie_worlds(True)],
                  center=xyz(center), up=xyz(up), forward=xyz(forward), nests=rows,
                  gaps=[(angles[(i+1)%len(angles)]-a)%360 for i,a in enumerate(angles)],
                  mirrors=mirrors, protected=protected())
    QA.mkdir(parents=True, exist_ok=True)
    return result


def fit_surface_offset(planet, center, up, offset):
    # Radial projection changes lateral coordinates over an irregular spherical
    # mesh. Compensate that lateral error until the authored circle is exact.
    target = center + offset
    query = target
    for _ in range(16):
        hit = planet.project_point_to_surface(query)
        assert hit, 'Missing real moon surface'
        error = target - hit.impact_point
        error = error - up*error.dot(up)
        if error.length() < .1: return hit
        query = query + error
    raise AssertionError('Surface circle failed to converge')


def configure():
    assert not unreal.EditorLevelLibrary.get_pie_worlds(True), 'Stop PIE before authoring'
    before = inspect()
    if not (QA/'FeedbackTuningBefore.json').exists():
        (QA/'FeedbackTuningBefore.json').write_text(json.dumps(before, indent=2), encoding='utf8')
    enc = encounter(); planet = enc.get_editor_property('planet')
    center = enc.get_actor_location(); up = planet.get_radial_up_vector(center)
    forward = enc.get_actor_forward_vector()
    forward = (forward-up*forward.dot(up)).normal(); right = up.cross(forward).normal()
    nests = sorted(enc.get_editor_property('nests'), key=lambda a:a.get_actor_label())
    assert len(nests) == 7
    placements = []
    for i, nest in enumerate(nests):
        angle = 2*math.pi*i/len(nests)
        offset = (forward*math.cos(angle)+right*math.sin(angle))*4250
        hit = fit_surface_offset(planet, center, up, offset)
        placements.append((nest, hit))
    for nest, hit in placements:
        nest.modify()
        nest.set_actor_location(hit.impact_point, False, False)
        nest.set_actor_rotation(planet.make_surface_aligned_rotation(hit.impact_point, center-hit.impact_point), False)
    enc.modify(); enc.rebuild_ridges()
    # Runtime scatter also restores frozen owner poses. Keep both records aligned
    # whenever this authoring step moves the seven entrances.
    from moon_runtime_map_revision import run as map_revision
    map_revision('boss_map_sync_manifest')
    bp = unreal.load_asset(BPS+'/BP_MirrorSkimmer')
    cdo = unreal.get_default_object(bp.generated_class())
    mirror = cdo.get_component_by_class(unreal.JTSMirrorDefenseComponent)
    mirror.set_editor_property('glare_range', 2200)
    mat = mirror.get_editor_property('glare_material')
    lib = unreal.MaterialEditingLibrary
    custom = next(e for e in lib.get_material_expressions(mat) if isinstance(e, unreal.MaterialExpressionCustom))
    code = custom.get_editor_property('code')
    code = code.replace('lerp(0.17,0.32,spots[i].z)', 'lerp(0.22,0.42,spots[i].z)')
    code = code.replace('radius*0.72,radius,d', 'radius*0.80,radius,d')
    code = code.replace('radius*1.55,d))*0.22', 'radius*1.55,d))*0.30')
    assert 'lerp(0.22,0.42,spots[i].z)' in code
    custom.set_editor_property('code', code)
    lib.recompile_material(mat); save(mat)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp); save(bp)
    after = inspect()
    assert after['protected'] == before['protected'], 'Moon mesh, collision or corpse changed'
    assert max(abs(g-360/7) for g in after['gaps']) < .01
    assert max(abs(n['radius']-4250) for n in after['nests']) < .2
    assert enc.ridges.get_num_sections() == 7
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    (QA/'FeedbackTuningAfter.json').write_text(json.dumps(after, indent=2), encoding='utf8')
    return dict(gaps=after['gaps'], radii=[n['radius'] for n in after['nests']], protected=True, glare=mat.get_path_name())


def run(stage):
    if stage.startswith('boss_tuning_qa_'): return qa(stage)
    if stage == 'boss_tuning_inspect': return inspect()
    if stage == 'boss_tuning_configure': return configure()
    if stage == 'boss_tuning_preview':
        from pickaxe_combat_validation import capture
        enc = encounter(); center = enc.get_actor_location()
        up = enc.get_editor_property('planet').get_radial_up_vector(center)
        forward = enc.get_actor_forward_vector()
        forward = (forward-up*forward.dot(up)).normal()
        return dict(image=capture(world(), center+up*7500, center, forward, 'MoonSevenNestCircle'))
    raise ValueError(stage)


def qa(stage):
    from mirror_boss_validation import worlds, get, server
    from pickaxe_combat_validation import spawn
    gs = unreal.GameplayStatics
    if stage == 'boss_tuning_qa_setup':
        from mirror_boss_feedback_revision import run as feedback
        import time
        result = feedback('boss_feedback_setup')
        unreal.get_default_object(unreal.JTSExpeditionSubsystem).set_editor_property(
            'save_slot_prefix', 'MoonFeedbackTuning_QA_'+str(int(time.time())))
        return result
    if stage == 'boss_tuning_qa_mirrors':
        w = server(); enc = get(w, unreal.JTSNestBossEncounter)[0]
        planet = enc.get_editor_property('planet'); center = enc.get_actor_location()
        up = planet.get_radial_up_vector(center); forward = enc.get_actor_forward_vector()
        forward = (forward-up*forward.dot(up)).normal(); right = up.cross(forward)
        target = gs.get_player_pawn(w, 0)
        cls = unreal.load_asset(BPS+'/BP_MirrorSkimmer').generated_class()
        unreal._moon_tuning_mirrors = []
        for i in range(3):
            hit = planet.project_point_to_surface(center+forward*1150+right*(i-1)*230)
            bug = spawn(w, cls, hit.impact_point, -forward, hit.impact_normal)
            mirror = bug.get_component_by_class(unreal.JTSMirrorDefenseComponent)
            if i == 0: mirror.aim_at(target)
            unreal._moon_tuning_mirrors.append(mirror)
        return dict(spawned=3, active=1)
    if stage == 'boss_tuning_qa_views':
        rows = []
        for pw in worlds():
            enc = get(pw, unreal.JTSNestBossEncounter)[0]
            planet = enc.get_editor_property('planet'); up = planet.get_radial_up_vector(enc.get_actor_location())
            forward = enc.get_actor_forward_vector(); forward = (forward-up*forward.dot(up)).normal()
            for pc in get(pw, unreal.PlayerController):
                if not pc.is_local_controller(): continue
                eye = pc.get_controlled_pawn().get_actor_location()+up*180
                camera = spawn(pw, unreal.CameraActor, eye, -forward, up)
                pc.set_view_target_with_blend(camera, 0)
                rows.append(dict(world=pw.get_path_name(),looking_away=True))
        return rows
    if stage == 'boss_tuning_qa_three':
        for mirror in unreal._moon_tuning_mirrors: mirror.aim_at(gs.get_player_pawn(server(), 0))
        return dict(active=3)
    if stage == 'boss_tuning_qa_sample':
        library = unreal.get_default_object(unreal.load_class(None, '/Script/UMG.WidgetBlueprintLibrary'))
        rows = []
        for pw in worlds():
            for pc in get(pw, unreal.PlayerController):
                if not pc.is_local_controller(): continue
                widgets = library.call_method('GetAllWidgetsOfClass',(pw,unreal.JTSMirrorGlareWidget.static_class(),False))
                mirrors = [b.get_component_by_class(unreal.JTSMirrorDefenseComponent) for b in get(pw,unreal.JTSPlanetSurfaceCreature)]
                rows.append(dict(world=pw.get_path_name(),
                    sources=sum(m.can_glare_at(pc) for m in mirrors if m),
                    widgets=[dict(count=widget.get_illuminating_mirror_count(),white=widget.get_full_white_strength())
                             for widget in widgets if widget.get_owning_player()==pc]))
        return rows
    if stage == 'boss_tuning_qa_key':
        w = server(); enc = get(w,unreal.JTSNestBossEncounter)[0]; planet = enc.get_editor_property('planet')
        pawn = gs.get_player_pawn(w,0); up = planet.get_radial_up_vector(pawn.get_actor_location())
        forward = pawn.get_actor_forward_vector(); right = up.cross(forward)
        hit = planet.project_point_to_surface(pawn.get_actor_location()+forward*110-right*100)
        cls = unreal.load_asset('/Game/Space/Items/Important/BP_MarsEcologyAccessCard').generated_class()
        unreal._moon_tuning_key = spawn(w,cls,hit.impact_point+hit.impact_normal*18,forward,hit.impact_normal)
        return dict(prompt=str(unreal._moon_tuning_key.get_interaction_prompt(pawn)),
                    can_interact=unreal._moon_tuning_key.can_interact(pawn))
    if stage == 'boss_tuning_qa_collect':
        w = server(); start = gs.get_time_seconds(w)
        def tick(dt):
            if gs.get_time_seconds(w)-start < .7: return
            unreal.unregister_slate_post_tick_callback(unreal._moon_tuning_tick)
            library = unreal.get_default_object(unreal.load_class(None,'/Script/UMG.WidgetBlueprintLibrary'))
            rows = []
            for pw in worlds():
                widgets = library.call_method('GetAllWidgetsOfClass',(pw,unreal.JTSExpeditionNoticeWidget.static_class(),False))
                for pc in get(pw,unreal.PlayerController):
                    if not pc.is_local_controller(): continue
                    notices = []
                    for widget in widgets:
                        if widget.get_owning_player()!=pc: continue
                        panel = unreal.find_object(None,widget.get_path_name()+'.WidgetTree.NoticePanel')
                        notices.append(dict(text=str(panel.get_content().get_text()),anchor_y=panel.slot.get_anchors().minimum.y,
                                            visible=widget.is_in_viewport(),opacity=widget.get_render_opacity()))
                    rows.append(dict(world=pw.get_path_name(),notices=notices,
                                     shared=gs.get_game_state(pw).has_important_item('MarsEcologyAccess')))
            (QA/'FeedbackTuningNotice.json').write_text(json.dumps(rows,indent=2),encoding='utf8')
            unreal.SystemLibrary.execute_console_command(w,'Shot SHOWUI',gs.get_player_controller(w,0))
        unreal._moon_tuning_tick = unreal.register_slate_post_tick_callback(tick)
        # Editor Python forces nested actor RPCs to execute locally. Defer the
        # interaction to the normal engine frame so the real client RPC runs.
        command='defer ke '+unreal._moon_tuning_key.get_path_name()+' Interact '+gs.get_player_pawn(w,0).get_path_name()
        unreal.SystemLibrary.execute_console_command(w,command,gs.get_player_controller(w,0))
        return dict(queued=True)
    raise ValueError(stage)
