"""Scoped authoring and validation of the corpse crater encounter."""
import unreal, json, math, hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
ART=ROOT/'SourceArt/MirrorBoss/MirrorBoss'
FOLDER='/Game/Space/Creatures/MirrorBoss'
BPS='/Game/Space/Blueprints/Planets/Moon/EcologyV3'
QA=ROOT/'Saved/MirrorBoss'
def actors(): return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
def xyz(v): return [v.x,v.y,v.z]
def save(obj):
    assert unreal.EditorAssetLibrary.save_loaded_asset(obj),obj
def world():return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
def planet():return next(a for a in actors() if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='Moon')
def material(name,color,roughness=.6,metallic=0,vertex=False,folder=None):
    folder=folder or FOLDER+'/Materials';mat=unreal.load_asset(folder+'/'+name)
    if not mat:mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
    lib=unreal.MaterialEditingLibrary;lib.delete_all_material_expressions(mat)
    c=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector);c.set_editor_property('constant',unreal.LinearColor(*color,1))
    if vertex:
        v=lib.create_material_expression(mat,unreal.MaterialExpressionVertexColor)
        mult=lib.create_material_expression(mat,unreal.MaterialExpressionMultiply)
        lib.connect_material_expressions(c,'',mult,'A');lib.connect_material_expressions(v,'RGB',mult,'B');c=mult
        mat.set_editor_property('two_sided',False)
        grain=lib.create_material_expression(mat,unreal.MaterialExpressionTextureSample)
        grain.set_editor_property('texture',unreal.load_asset('/Game/Space/Environment/MoonV3/Textures/T_MoonMineral_Detail'))
        grain.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        contrast=lib.create_material_expression(mat,unreal.MaterialExpressionLinearInterpolate)
        contrast.set_editor_property('const_a',.65);contrast.set_editor_property('const_b',1.0)
        lib.connect_material_expressions(grain,'R',contrast,'Alpha')
        detail=lib.create_material_expression(mat,unreal.MaterialExpressionMultiply)
        lib.connect_material_expressions(c,'',detail,'A');lib.connect_material_expressions(contrast,'',detail,'B');c=detail
    lib.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
    for value,prop in [(roughness,unreal.MaterialProperty.MP_ROUGHNESS),(metallic,unreal.MaterialProperty.MP_METALLIC)]:
        node=lib.create_material_expression(mat,unreal.MaterialExpressionConstant);node.set_editor_property('r',value);lib.connect_material_property(node,'',prop)
    lib.recompile_material(mat);save(mat);return mat
def import_mesh():
    from moon_surface_unreal import fbx_task,import_tasks
    import_tasks([fbx_task(ART/'SK_MirrorBoss.fbx',FOLDER,'SK_MirrorBoss',True)])
    mesh=unreal.load_asset(FOLDER+'/SK_MirrorBoss');assert mesh
    settings=[('MirrorShell',(.54,.61,.65),.23,.78),('WornRim',(.28,.34,.36),.55,.15),('Joint',(.035,.052,.057),.8,0),('Crown',(.78,.83,.82),.2,.78),('SoftTissue',(.28,.13,.075),.7,0),('Eyes',(.20,.72,.84),.17,.3)]
    mats={'M_MB_'+n:material('M_MB_'+n,c,r,m) for n,c,r,m in settings}
    slots=list(mesh.materials)
    for slot in slots:
        key=str(slot.material_slot_name);assert key in mats,key;slot.material_interface=mats[key]
    mesh.set_editor_property('materials',slots);save(mesh);save(mesh.skeleton)
    return dict(mesh=mesh.get_path_name(),materials=list(mats),bounds=str(mesh.get_bounds()))
def import_animations():
    from moon_surface_unreal import fbx_task,import_tasks
    skeleton=unreal.load_asset(FOLDER+'/SK_MirrorBoss').skeleton
    clips=['Idle','Walk','Emerge','Slam','Sweep','Charge','Leap','Stagger','Death']
    tasks=[fbx_task(ART/('A_MirrorBoss_'+c+'.fbx'),FOLDER+'/Animations','A_MirrorBoss_'+c,True,True,skeleton) for c in clips]
    import_tasks(tasks)
    for c in clips:save(unreal.load_asset(FOLDER+'/Animations/A_MirrorBoss_'+c))
    return {c:unreal.load_asset(FOLDER+'/Animations/A_MirrorBoss_'+c).get_play_length() for c in clips}
def configure():
    bp=unreal.load_asset(BPS+'/BP_MirrorBoss')
    if not bp:
        factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',unreal.JTSPlanetBoss)
        bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_MirrorBoss',BPS,unreal.Blueprint,factory)
    cdo=unreal.get_default_object(bp.generated_class());mesh=cdo.get_editor_property('mesh')
    mesh.set_skeletal_mesh_asset(unreal.load_asset(FOLDER+'/SK_MirrorBoss'))
    cdo.set_editor_property('display_name',unreal.Text('镜虫首领'))
    cdo.get_editor_property('health').set_editor_property('max_health',620)
    cdo.set_editor_property('extra_player_health',180)
    cdo.set_editor_property('animations',{getattr(unreal.JTSBossAction,c.upper()):unreal.load_asset(FOLDER+'/Animations/A_MirrorBoss_'+c) for c in ['Idle','Walk','Emerge','Slam','Sweep','Charge','Leap','Stagger','Death']})
    attacks={}
    for name,windup,active,recovery,damage,reach,radius,arc,cooldown in [
      ('Slam',1.05,.25,1.2,2.2,480,185,120,3.5),('Sweep',1.1,.6,1.3,1.8,400,200,140,4.5),
      ('Charge',1.05,.95,1.2,2.4,1500,140,60,7),('Leap',1.3,.95,1.45,3,1900,360,180,9)]:
        a=unreal.JTSBossAttack()
        for k,v in dict(windup=windup,active=active,recovery=recovery,damage=damage,reach=reach,radius=radius,arc_degrees=arc,cooldown=cooldown).items():a.set_editor_property(k,v)
        attacks[getattr(unreal.JTSBossAction,name.upper())]=a
    cdo.get_editor_property('combat').set_editor_property('attacks',attacks)
    zones=[]
    rows=[('body',(0,0,142),(118,86,36),.65),('abdomen',(-110,0,110),(45,66,28),.8),('head',(118,0,81),(34,42,23),1.5)]
    for s,sign in [('L',1),('R',-1)]:
        rows += [('claw_'+s,(213,sign*125,80),(63,36,25),.75),('claw_'+s+'_upper',(87,sign*89,107),(34,27,21),.8)]
        for i,x in [(2,12),(3,-88)]:
            rows += [('leg_'+s+str(i)+'_upper',(x-6,sign*102,96),(22,36,20),.9),('leg_'+s+str(i)+'_lower',(x-20,sign*148,47),(18,22,43),.9)]
    for bone,center,extent,mult in rows:
        z=unreal.JTSBossHitZone()
        for k,v in dict(bone=bone,center=unreal.Vector(*center),half_extent=unreal.Vector(*extent),damage_multiplier=mult).items():z.set_editor_property(k,v)
        if bone in ('body','abdomen'):z.set_editor_property('blocks_movement',True)
        zones.append(z)
    cdo.set_editor_property('hit_zones',zones)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
    return dict(blueprint=bp.get_path_name(),zones=len(zones),health=620,attacks=len(attacks))
def protected():
    state={a.get_actor_label():a for a in actors()}
    def transform(a):
        r=a.get_actor_rotation();return dict(location=xyz(a.get_actor_location()),rotation=[r.pitch,r.yaw,r.roll],scale=xyz(a.get_actor_scale3d()))
    return dict(moon_transform=transform(state['MoonPlanet']),moon_mesh=state['MoonPlanet'].static_mesh_component.static_mesh.get_path_name(),
                moon_materials=[m.get_path_name() for m in state['MoonPlanet'].static_mesh_component.get_materials()],
                corpse_transform=transform(state['Moon_Corpse_SkeletonAstronaut']),
                mesh_sha256=hashlib.sha256((ROOT/'Content/ThirdParty/Environment/UltimateSpace/Meshes/Planet_3.uasset').read_bytes()).hexdigest())
def layout():
    QA.mkdir(parents=True,exist_ok=True)
    before=protected();(QA/'protected_before.json').write_text(json.dumps(before,indent=2))
    state={a.get_actor_label():a for a in actors()};p=planet();moon=state['MoonPlanet']
    fit=json.loads((ROOT/'SourceArt/MoonAntNests/moon_crater_foot.json').read_text())
    center=moon.get_actor_transform().transform_location(unreal.Vector(*fit['foot_center_local_cm']))
    hit=p.project_point_to_surface(center);assert hit
    center=hit.impact_point;up=p.get_radial_up_vector(center)
    first=state['Moon_Nest_01'].get_actor_location()-center;first=(first-up*first.dot(up)).normal();right=up.cross(first).normal()
    nests=sorted([a for a in actors() if a.get_actor_label().startswith('Moon_Nest_')],key=lambda a:a.get_actor_label())
    assert len(nests) in (7,9),len(nests)
    retained=nests[:7];retired=nests[7:]
    # Remove explicit references to the retired entrances before deleting their level instances.
    for a in actors():
        for prop in ('placed_ecology_habitats','refuges','refuge_providers','food_points'):
            try:values=list(a.get_editor_property(prop))
            except Exception:continue
            filtered=[v for v in values if v not in retired]
            if len(filtered)!=len(values):a.modify();a.set_editor_property(prop,filtered)
    for a in retired:unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(a)
    rows=[]
    for i,a in enumerate(retained):
        angle=2*math.pi*i/7;direction=first*math.cos(angle)+right*math.sin(angle)
        ground=p.project_point_to_surface(center+direction*4450);assert ground
        a.modify();a.set_actor_location(ground.impact_point,False,False)
        a.set_actor_rotation(p.make_surface_aligned_rotation(ground.impact_point,center-ground.impact_point),False)
        a.set_editor_property('destructible_entrance',True)
        rows.append(dict(label=a.get_actor_label(),angle=angle*180/math.pi,location=xyz(ground.impact_point)))
    encounter=next((a for a in actors() if isinstance(a,unreal.JTSNestBossEncounter)),None)
    if not encounter:encounter=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.JTSNestBossEncounter,center)
    encounter.set_actor_label('Moon_Crater_MirrorBoss_Encounter');encounter.set_actor_location(center,False,False)
    encounter.set_actor_rotation(p.make_surface_aligned_rotation(center,first),False)
    ridge=material('M_MB_ExcavatedRidge',(.10,.107,.112),.96,0,True)
    for k,v in dict(planet=p,nests=retained,boss_class=unreal.load_asset(BPS+'/BP_MirrorBoss').generated_class(),end_ring_radius=650,ridge_half_width=80,ridge_height=3,ridge_material=ridge,wake_delay=1.6).items():encounter.set_editor_property(k,v)
    encounter.rebuild_ridges()
    assert protected()==before,'Protected terrain/corpse changed'
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    result=dict(nests=rows,center=xyz(center),ridge_sections=encounter.ridges.get_num_sections(),protected=True)
    (QA/'layout.json').write_text(json.dumps(result,indent=2));return result
def revision_configure():
    from moon_surface_unreal import fbx_task,import_tasks
    skeleton=unreal.load_asset(FOLDER+'/SK_MirrorBoss').skeleton
    import_tasks([fbx_task(ART/('A_MirrorBoss_'+c+'.fbx'),FOLDER+'/Animations','A_MirrorBoss_'+c,True,True,skeleton) for c in ['TurnLeft','TurnRight']])
    bp=unreal.load_asset(BPS+'/BP_MirrorBoss');cdo=unreal.get_default_object(bp.generated_class())
    clips=dict(cdo.get_editor_property('animations'))
    for name,enum in [('TurnLeft','TURN_LEFT'),('TurnRight','TURN_RIGHT')]:
        clip=unreal.load_asset(FOLDER+'/Animations/A_MirrorBoss_'+name);save(clip)
        clips[getattr(unreal.JTSBossAction,enum)]=clip
    cdo.set_editor_property('animations',clips)
    cdo.get_editor_property('combat').set_editor_property('turn_speed_degrees',72)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp);save(skeleton)
    item_folder='/Game/Space/Items/Important'
    import_tasks([fbx_task(ROOT/'SourceArt/MirrorBoss/SM_MarsAccessCard.fbx',item_folder,'SM_MarsAccessCard')])
    mesh=unreal.load_asset(item_folder+'/SM_MarsAccessCard')
    configs={'M_CardAlloy':((.6,.66,.69),.35,.65),'M_CardInset':((.016,.036,.049),.65,.15),
             'M_CardSignal':((.02,.65,.9),.3,.2),'M_CardMars':((.7,.12,.045),.4,.3)}
    slots=list(mesh.static_materials)
    for slot in slots:
        key=str(slot.material_slot_name);color,rough,metal=configs[key]
        mat=material(key,color,rough,metal,folder=item_folder+'/Materials')
        if key=='M_CardSignal':
            lib=unreal.MaterialEditingLibrary
            node=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector)
            node.set_editor_property('constant',unreal.LinearColor(.04,1.3,1.8,1))
            lib.connect_material_property(node,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            lib.recompile_material(mat);save(mat)
        slot.material_interface=mat
    mesh.set_editor_property('static_materials',slots);save(mesh)
    key_bp=unreal.load_asset(item_folder+'/BP_MarsEcologyAccessCard')
    if not key_bp:
        factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',unreal.JTSExpeditionKeyPickup)
        key_bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_MarsEcologyAccessCard',item_folder,unreal.Blueprint,factory)
    config=unreal.get_default_object(key_bp.generated_class())
    config.set_editor_property('important_item_id','MarsEcologyAccess')
    config.set_editor_property('display_name',unreal.Text('火星生态基地权限卡'))
    config.set_editor_property('pickup_notice',unreal.Text('火星生态基地 已标记在导航系统中'))
    config.get_editor_property('mesh').set_static_mesh(mesh)
    unreal.BlueprintEditorLibrary.compile_blueprint(key_bp);save(key_bp)
    enc=next(a for a in actors() if isinstance(a,unreal.JTSNestBossEncounter))
    enc.set_editor_property('encounter_id','MoonCorpseCraterLeader')
    enc.set_editor_property('reward_class',key_bp.generated_class())
    mars=[a for a in actors() if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='Mars']
    assert len(mars)==1,[a.get_actor_label() for a in mars]
    mars[0].set_editor_property('navigation_required_important_item','MarsEcologyAccess')
    mars[0].set_editor_property('show_on_star_chart',True)
    mars[0].set_editor_property('show_screen_marker',True)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return dict(turn_clips=2,card=key_bp.get_path_name(),mars=mars[0].get_actor_label(),reward=enc.get_actor_label())

def run(stage):
    if stage.startswith('boss_map_'):
        import moon_runtime_map_revision, importlib
        return importlib.reload(moon_runtime_map_revision).run(stage)
    if stage.startswith('boss_tuning_'):
        import moon_feedback_tuning,importlib
        return importlib.reload(moon_feedback_tuning).run(stage)
    if stage.startswith('boss_feedback_'):
        import mirror_boss_feedback_revision,importlib
        return importlib.reload(mirror_boss_feedback_revision).run(stage)
    if stage=='boss_revision_configure':return revision_configure()
    if stage=='boss_revision_network':
        result=run('boss_network_settings')
        settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
        settings.set_editor_property('AdditionalServerGameOptions','?game=/Script/space.JTSGameplayGameModeBase?JTSBuild=JTS-1')
        world().get_world_settings().set_editor_property('default_game_mode',unreal.JTSGameplayGameModeBase)
        gm=unreal.get_default_object(unreal.JTSGameplayGameModeBase)
        unreal._boss_qa_build_rule=gm.get_editor_property('require_matching_build_version')
        gm.set_editor_property('require_matching_build_version',False)
        subsystem=unreal.get_default_object(unreal.JTSExpeditionSubsystem)
        unreal._boss_qa_save_prefix=subsystem.get_editor_property('save_slot_prefix')
        subsystem.set_editor_property('save_slot_prefix','MirrorBoss_Disposable_QA')
        return result
    if stage=='boss_final_check':
        assert not unreal.EditorLevelLibrary.get_pie_worlds(True),'Stop PIE first'
        enc=next(a for a in actors() if isinstance(a,unreal.JTSNestBossEncounter))
        nests=list(enc.get_editor_property('nests'))
        assert len(nests)==7 and enc.ridges.get_num_sections()==7
        assert all(n.get_editor_property('destructible_entrance') and not n.is_entrance_blocked() for n in nests)
        assert len([a for a in actors() if a.get_actor_label().startswith('Moon_Nest_')])==7
        bp=unreal.load_asset(BPS+'/BP_MirrorBoss');cdo=unreal.get_default_object(bp.generated_class())
        unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
        assert len(cdo.get_editor_property('animations'))==11 and len(cdo.get_editor_property('hit_zones'))==15
        reward=unreal.get_default_object(enc.get_editor_property('reward_class'))
        assert str(reward.get_editor_property('important_item_id'))=='MarsEcologyAccess'
        assert str(reward.get_editor_property('display_name'))=='火星生态基地权限卡'
        assert str(reward.get_editor_property('pickup_notice'))=='火星生态基地 已标记在导航系统中'
        mars=next(a for a in actors() if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='Mars')
        assert str(mars.get_editor_property('navigation_required_important_item'))=='MarsEcologyAccess'
        before=json.loads((QA/'protected_before.json').read_text());after=protected()
        before['moon_materials']=[m.split("'")[1] if m.startswith('<Object') else m for m in before['moon_materials']]
        assert before==after,'Protected moon/corpse state differs'
        mode=world().get_world_settings().get_editor_property('default_game_mode').get_path_name()
        assert 'BP_SpaceWorldGameMode' in mode,mode
        up=planet().get_radial_up_vector(enc.get_actor_location())
        f=nests[0].get_actor_location()-enc.get_actor_location();f=(f-up*f.dot(up)).normal();r=up.cross(f).normal()
        angles=sorted(math.degrees(math.atan2((n.get_actor_location()-enc.get_actor_location()).dot(r),(n.get_actor_location()-enc.get_actor_location()).dot(f)))%360 for n in nests)
        gaps=[(angles[(i+1)%7]-angles[i])%360 for i in range(7)]
        assert max(abs(g-360/7) for g in gaps)<.5,gaps
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        return dict(nests=7,ridges=7,angular_gaps=gaps,clips=11,hit_zones=15,protected=True,game_mode=mode)
    if stage=='boss_import_mesh':return import_mesh()
    if stage=='boss_import_animations':return import_animations()
    if stage=='boss_configure':return configure()
    if stage=='boss_layout':return layout()
    if stage=='boss_rebuild':
        encounter=next(a for a in actors() if isinstance(a,unreal.JTSNestBossEncounter));encounter.modify()
        encounter.set_editor_property('ridge_material',material('M_MB_ExcavatedRidge',(.10,.107,.112),.96,0,True))
        encounter.rebuild_ridges();unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        return dict(sections=encounter.ridges.get_num_sections())
    if stage=='boss_gameview':
        unreal.EditorLevelLibrary.editor_set_game_view(True)
        unreal.SystemLibrary.execute_console_command(world(),'viewmode lit')
        return dict(game_view=True)
    if stage=='boss_network_settings':
        settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
        unreal._boss_play_settings={p:settings.get_editor_property(p) for p in ['PlayNumberOfClients','RunUnderOneProcess','AdditionalServerGameOptions']}
        settings.set_editor_property('PlayNumberOfClients',2)
        settings.set_editor_property('RunUnderOneProcess',True)
        # Test boss replication in a disposable neutral game mode; expedition admission rules remain intact.
        settings.set_editor_property('AdditionalServerGameOptions','?game=/Script/Engine.GameModeBase')
        ws=world().get_world_settings()
        unreal._boss_qa_game_mode=ws.get_editor_property('default_game_mode')
        ws.set_editor_property('default_game_mode',unreal.GameModeBase)
        return dict(clients=2,listen_server=True)
    if stage=='boss_restore_settings':
        if hasattr(unreal,'_boss_qa_build_rule'):
            unreal.get_default_object(unreal.JTSGameplayGameModeBase).set_editor_property('require_matching_build_version',unreal._boss_qa_build_rule)
            unreal.get_default_object(unreal.JTSExpeditionSubsystem).set_editor_property('save_slot_prefix',unreal._boss_qa_save_prefix)
            del unreal._boss_qa_build_rule;del unreal._boss_qa_save_prefix
        if hasattr(unreal,'_boss_qa_game_mode'):
            world().get_world_settings().set_editor_property('default_game_mode',unreal._boss_qa_game_mode)
            del unreal._boss_qa_game_mode
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        if hasattr(unreal,'_boss_play_settings'):
            settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
            for p,v in unreal._boss_play_settings.items():settings.set_editor_property(p,v)
            del unreal._boss_play_settings
        return dict(restored=True)
    if stage.startswith('boss_qa_revision_'):
        import mirror_boss_revision_validation,importlib
        return importlib.reload(mirror_boss_revision_validation).run(stage)
    if stage.startswith('boss_qa_'):
        import mirror_boss_validation,importlib
        return importlib.reload(mirror_boss_validation).run(stage)
    if stage=='boss_overview':
        encounter=next(a for a in actors() if isinstance(a,unreal.JTSNestBossEncounter));p=planet();center=encounter.get_actor_location();up=p.get_radial_up_vector(center)
        camera=center+up*9000;rotation=unreal.MathLibrary.find_look_at_rotation(camera,center)
        unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(camera,rotation)
        return dict(location=xyz(camera),rotation=dict(pitch=rotation.pitch,yaw=rotation.yaw,roll=rotation.roll))
    if stage=='boss_asset_check':
        mesh=unreal.load_asset(FOLDER+'/SK_MirrorBoss');cdo=unreal.get_default_object(unreal.load_asset(BPS+'/BP_MirrorBoss').generated_class())
        return dict(bounds=str(mesh.get_bounds()),zones=[dict(bone=str(z.get_editor_property('bone')),center=xyz(z.get_editor_property('center'))) for z in cdo.get_editor_property('hit_zones')],protected=protected())
    if stage=='boss_inspect':
        rows=[]
        for a in actors():
            label=a.get_actor_label()
            if any(s in label for s in ('Nest','Corpse','MoonPlanet','MoonSurface','尸','Crater')):
                rows.append(dict(label=label,path=a.get_path_name(),cls=a.get_class().get_name(),location=xyz(a.get_actor_location()),rotation=str(a.get_actor_rotation()),scale=xyz(a.get_actor_scale3d()),meshes=[dict(name=c.get_name(),mesh=str(c.get_editor_property('static_mesh'))) for c in a.get_components_by_class(unreal.StaticMeshComponent)]))
        return rows
    raise ValueError(stage)
