"""Scoped MirrorSkimmer assets and in-editor visual verification, exposed through Unreal MCP."""
import json
import sys
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/MirrorDefenseQA'
OUT.mkdir(parents=True, exist_ok=True)
BP = '/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_MirrorSkimmer'
MATS = '/Game/Space/Environment/MoonV3/Materials'
EFFECT = '/Game/Space/Effects/Moon'
LIB = unreal.MaterialEditingLibrary
CUSTOM_CONNECTIONS = {}


def material(name, folder):
    mat = unreal.load_asset(folder + '/' + name)
    if not mat:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(mat)
    return mat


def expression(mat, cls, **values):
    node = LIB.create_material_expression(mat, cls)
    if cls == unreal.MaterialExpressionCustom:
        node.set_editor_property('inputs', [])
    for name, value in values.items():
        node.set_editor_property(name, value)
    return node


def parameter(mat, name, value):
    return expression(mat, unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)


def input_node(node, name, value):
    connections=CUSTOM_CONNECTIONS.setdefault(node.get_path_name(),[])
    connections.append((name,value))
    inputs=[]
    for key,_ in connections:
        data=unreal.CustomInput()
        data.set_editor_property('input_name',key)
        inputs.append(data)
    node.set_editor_property('inputs', inputs)
    for key,source in connections:
        assert LIB.connect_material_expressions(source, '', node, key)


def configure():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    # A UI material makes the coverage independent of bloom, exposure and graphics scalability.
    glare = material('M_MirrorRetinalGlare', EFFECT)
    glare.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    glare.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = expression(glare, unreal.MaterialExpressionTextureCoordinate)
    custom = expression(glare, unreal.MaterialExpressionCustom, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property('code', '''
float opacity = 0;
float halo = 0;
float3 spots[3] = {Spot0, Spot1, Spot2};
for (int i=0; i<3; ++i) {
    float2 delta = (UV-spots[i].xy)*float2(Aspect,1);
    float d = length(delta);
    float radius = lerp(0.17,0.32,spots[i].z);
    float core = 1-smoothstep(radius*0.72,radius,d);
    float bloom = (1-smoothstep(radius*0.62,radius*1.55,d))*0.22;
    float ghost = exp(-pow((d-radius*1.22)/0.035,2))*0.035;
    float a = saturate(core+bloom+ghost)*spots[i].z;
    opacity = 1-(1-opacity)*(1-a);
    halo = max(halo,bloom*spots[i].z);
}
opacity = lerp(opacity,1,smoothstep(0.0,1.0,FullWhite));
return float4(1,1,lerp(1,0.98,halo),opacity);
''')
    input_node(custom, 'UV', uv)
    input_node(custom, 'Aspect', parameter(glare, 'Aspect', 1.777778))
    input_node(custom, 'FullWhite', parameter(glare, 'FullWhite', 0))
    for i in range(3):
        node = expression(glare, unreal.MaterialExpressionVectorParameter, parameter_name='Spot'+str(i), default_value=unreal.LinearColor(.5,.5,0,0))
        input_node(custom, 'Spot'+str(i), node)
    color = expression(glare, unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False)
    alpha = expression(glare, unreal.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True)
    LIB.connect_material_expressions(custom, '', color, '')
    LIB.connect_material_expressions(custom, '', alpha, '')
    LIB.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)
    LIB.recompile_material(glare)
    assert unreal.EditorAssetLibrary.save_loaded_asset(glare)

    # Broad, imperfect silver plates, with understated roughness variation and no painted fake reflection.
    for role, tint in [('Shell', (.58,.68,.72)), ('Crown', (.66,.73,.74))]:
        mat = material('M_MC_MirrorSkimmer_'+role, MATS)
        mat.set_editor_property('two_sided', False)
        color = expression(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(*tint,1))
        LIB.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
        LIB.connect_material_property(parameter(mat,'Metallic',1),'',unreal.MaterialProperty.MP_METALLIC)
        rough = expression(mat, unreal.MaterialExpressionCustom, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                           code='return 0.26+0.075*(0.5+0.5*sin(UV.x*37.0+sin(UV.y*29.0)*0.7));')
        input_node(rough,'UV',expression(mat,unreal.MaterialExpressionTextureCoordinate))
        LIB.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
        reflection=expression(mat,unreal.MaterialExpressionTextureSampleParameterCube,
                              parameter_name='NearbyReflection',texture=unreal.load_asset('/Engine/EngineResources/GrayTextureCube'),
                              mip_value_mode=unreal.TextureMipValueMode.TMVM_MIP_LEVEL)
        direction=expression(mat,unreal.MaterialExpressionReflectionVectorWS)
        assert LIB.connect_material_expressions(direction,'',reflection,'UVs')
        assert LIB.connect_material_expressions(parameter(mat,'ReflectionBlurMip',2.3),'',reflection,'Level')
        weight=expression(mat,unreal.MaterialExpressionMultiply,const_b=.6)
        LIB.connect_material_expressions(parameter(mat,'NearbyReflectionWeight',0),'',weight,'A')
        reflected_color=expression(mat,unreal.MaterialExpressionMultiply)
        LIB.connect_material_expressions(reflection,'RGB',reflected_color,'A')
        LIB.connect_material_expressions(weight,'',reflected_color,'B')
        LIB.connect_material_property(reflected_color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        LIB.recompile_material(mat)
        assert unreal.EditorAssetLibrary.save_loaded_asset(mat)

    bp = unreal.load_asset(BP)
    cdo = unreal.get_default_object(bp.generated_class())
    defense = cdo.get_editor_property('mirror_defense')
    for name,value in dict(enabled=True,guard_radius=230,glare_range=2200,body_bone='body',body_lift=2.0,
                           leg_length_scale=1.22,maximum_lean_degrees=78,mirror_height_above_body=8.5,
                           mirror_offset=unreal.Vector(0,0,16),shell_half_extent=unreal.Vector(24,19,4),glare_material=glare).items():
        defense.set_editor_property(name,value)
    legs=[]
    for side in ('L','R'):
        for i in range(1,4):
            prefix='leg_'+side+str(i)
            leg=unreal.JTSMirrorLegChain()
            leg.set_editor_property('upper',prefix+'_upper')
            leg.set_editor_property('lower',prefix+'_lower')
            leg.set_editor_property('foot',prefix+'_toe')
            legs.append(leg)
    defense.set_editor_property('legs',legs)
    cdo.set_editor_property('attack_damage',0)
    cdo.set_editor_property('defends_nest',False)
    cdo.set_editor_property('retaliates',False)
    animations=dict(cdo.get_editor_property('animations'))
    animations[unreal.JTSForagerState.REFLECT]=animations[unreal.JTSForagerState.IDLE]
    animations[unreal.JTSForagerState.ATTACK]=animations[unreal.JTSForagerState.IDLE]
    cdo.set_editor_property('animations',animations)
    mesh=cdo.get_editor_property('mesh')
    mirror_slots=[i for i in range(mesh.get_num_materials()) if any(role in mesh.get_material(i).get_name() for role in ('MirrorSkimmer_Shell','MirrorSkimmer_Crown'))]
    assert len(mirror_slots)==2,mirror_slots
    defense.set_editor_property('reflective_material_slots',mirror_slots)
    mesh.set_editor_property('bounds_scale',1.6)
    mesh.set_editor_property('visibility_based_anim_tick_option',unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    return tune()


def tune():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    bp = unreal.load_asset(BP)
    cdo = unreal.get_default_object(bp.generated_class())
    defense = cdo.get_editor_property('mirror_defense')
    for key, value in dict(glare_range=1600, glare_half_angle=28,
                           retract_seconds=1.5, surface_pause_seconds=2.5, stride_length=40, step_height=5).items():
        defense.set_editor_property(key, value)
    cdo.set_editor_property('walk_speed', 110)
    cdo.set_editor_property('flee_speed', 270)
    cdo.set_editor_property('leash_radius', 1200)
    rates = dict(cdo.get_editor_property('animation_play_rates'))
    rates[unreal.JTSForagerState.WALK] = 1.4
    rates[unreal.JTSForagerState.FLEE] = 1.0
    cdo.set_editor_property('animation_play_rates', rates)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    nests = [a for a in actors if isinstance(a, unreal.JTSPlanetEcologyHabitat) and a.get_actor_label().startswith('Moon_Nest_')]
    assert len(nests) == 9
    for nest in nests:
        before = nest.get_actor_transform()
        nest.modify()
        blockade = nest.get_editor_property('blockade')
        blockade.modify()
        for key, value in dict(finite_population=True, population_capacity=14, population_recovery_delay=90,
                               population_recovery_interval=12, defense_population=10, defense_leash_distance=1800,
                               defender_emergence_interval=.35).items():
            blockade.set_editor_property(key, value)
        assert nest.get_actor_transform().equals(before)
    glare = unreal.load_asset(EFFECT + '/M_MirrorRetinalGlare')
    custom = next(node for node in LIB.get_material_expressions(glare) if isinstance(node, unreal.MaterialExpressionCustom))
    old_code = custom.get_editor_property('code')
    code = old_code.replace('lerp(0.24,0.49,spots[i].z)', 'lerp(0.17,0.32,spots[i].z)')
    if code != old_code:
        custom.set_editor_property('code', code)
        LIB.recompile_material(glare)
        assert unreal.EditorAssetLibrary.save_loaded_asset(glare)
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return inspect()


def inspect():
    bp=unreal.load_asset(BP);cdo=unreal.get_default_object(bp.generated_class())
    defense=cdo.get_editor_property('mirror_defense')
    result=dict(enabled=defense.get_editor_property('enabled'),damage=cdo.get_editor_property('attack_damage'),
                mesh_class=cdo.get_editor_property('mesh').get_class().get_name(),legs=len(defense.get_editor_property('legs')),
                material=defense.get_editor_property('glare_material').get_path_name() if defense.get_editor_property('glare_material') else None,
                mirror_slots=list(defense.get_editor_property('reflective_material_slots')),
                mesh_materials=[cdo.get_editor_property('mesh').get_material(i).get_name() for i in range(cdo.get_editor_property('mesh').get_num_materials())])
    result['motion'] = {key: cdo.get_editor_property(key) for key in ('walk_speed', 'flee_speed')}
    result['individual_threat_radius'] = cdo.get_editor_property('leash_radius')
    result['defense'] = {key: defense.get_editor_property(key) for key in ('glare_range', 'glare_half_angle', 'retract_seconds', 'surface_pause_seconds')}
    result['rates'] = {str(key): value for key, value in dict(cdo.get_editor_property('animation_play_rates')).items()}
    editor_actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    result['nests'] = [dict(label=a.get_actor_label(), count=a.get_editor_property('enemy_count'),
                            enemy=a.get_editor_property('enemy_class').get_name() if a.get_editor_property('enemy_class') else None,
                            recovery=a.get_editor_property('maintain_population'),
                            finite=a.get_editor_property('blockade').get_editor_property('finite_population'),
                            capacity=a.get_editor_property('blockade').get_editor_property('population_capacity'),
                            threat_radius=a.get_editor_property('blockade').get_editor_property('defense_leash_distance'))
                       for a in editor_actors if isinstance(a, unreal.JTSPlanetEcologyHabitat) and a.get_actor_label().startswith('Moon_Nest_')]
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if world:
        for c in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceCreature):
            mat=c.get_editor_property('mesh').get_material(1)
            if isinstance(mat,unreal.MaterialInstanceDynamic):
                texture=mat.call_method('K2_GetTextureParameterValue',(unreal.Name('NearbyReflection'),))
                weight=mat.call_method('K2_GetScalarParameterValue',(unreal.Name('NearbyReflectionWeight'),))
                if texture and weight>0:
                    result['capture']=dict(texture=texture.get_name(),size=texture.get_editor_property('size_x'),weight=weight)
                    cube=unreal.new_object(unreal.TextureCube)
                    unreal.RenderingLibrary.convert_render_target_to_texture_cube_editor_only(world,texture,cube)
                    task=unreal.AssetExportTask();task.object=cube;task.filename=str(OUT/'nearby_reflection.hdr');task.automated=True;task.prompt=False
                    result['capture']['exported']=unreal.Exporter.run_asset_export_task(task)
                    break
    return result


def close():
    assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
    unreal.SystemLibrary.quit_editor()
    return dict(closing=True)


def xyz(v): return [v.x,v.y,v.z]


def balance_start():
    import math
    qa('qa_clear')
    if hasattr(unreal, '_mirror_aim_tick'):
        unreal.unregister_slate_post_tick_callback(unreal._mirror_aim_tick)
        del unreal._mirror_aim_tick
    data = unreal._mirror_defense_qa
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    creature = data['creatures'][0]
    point = data['planet'].get_surface_frame_at(data['base'] + data['forward'] * 900, data['forward'])
    creature.set_actor_location_and_rotation(point.location, point.transform.rotation.rotator(), False, True)
    assert creature.call_method('InitializeForSettlement', (data['planet'], data['base'], point.location))
    pawn.set_actor_location_and_rotation(point.location + point.up * 90, point.transform.rotation.rotator(), False, True)
    creature.defend_entrance(pawn, data['base'], 1800, 1, 3)
    for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.JTSMirrorGlareWidget, False):
        widget.set_visibility(unreal.SlateVisibility.COLLAPSED)
    start = unreal.GameplayStatics.get_time_seconds(world)
    return_location = pawn.get_actor_location()
    probe = dict(rows=[], start=start, previous=creature.get_actor_location(), previous_time=start, shots=set(), events=[])
    unreal._mirror_balance_probe = probe
    def sample(dt):
        if not valid(creature) or not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
            unreal.unregister_slate_post_tick_callback(probe['handle'])
            return
        now = unreal.GameplayStatics.get_time_seconds(world)
        if now <= probe['previous_time']:
            return
        if now-start >= 8 and not probe['events']:
            pawn.set_actor_location(data['base'] + data['forward'] * 2300 + data['up'] * 90, False, True)
            probe['events'].append(dict(time=round(now-start, 3), action='attacker_left_nest_radius'))
        elif now-start >= 9.5 and len(probe['events']) == 1:
            pawn.set_actor_location(return_location, False, True)
            probe['events'].append(dict(time=round(now-start, 3), action='attacker_returned_without_attack'))
        location = creature.get_actor_location()
        up, forward = creature.get_actor_up_vector(), creature.get_actor_forward_vector()
        mirror = creature.get_editor_property('mirror_defense')
        state = creature.get_editor_property('state').name
        angle = math.degrees(math.acos(max(-1, min(1, mirror.get_mirror_normal().dot(up)))))
        feet = [(creature.get_editor_property('mesh').get_socket_location('leg_' + side + str(i) + '_toe') - location).dot(up)
                for side in ('L', 'R') for i in range(1, 4)]
        probe['rows'].append(dict(time=now-start, dt=now-probe['previous_time'], state=state,
                                  step=(location-probe['previous']).length(), angle=angle, feet=feet))
        probe['previous'], probe['previous_time'] = location, now
        if probe.get('state') != state:
            probe['state'], probe['state_start'] = state, now
        camera_location = location + data['right'] * 175 + up * 95
        view = (location + up * 22 - camera_location).normal()
        view_up = (up - view * view.dot(up)).normal()
        data['camera'].set_actor_location_and_rotation(camera_location, unreal.MathLibrary.make_rotation_from_axes(view, view_up.cross(view), view_up), False, True)
        if state not in probe['shots'] and now - probe['state_start'] > .5:
            probe['shots'].add(state)
            unreal.SystemLibrary.execute_console_command(world, 'Shot SHOWUI filename=' + str(OUT / ('balance_' + state + '.png')) + ' -nosuffix')
        if now-start >= 16:
            unreal.unregister_slate_post_tick_callback(probe['handle'])
            probe['finished'] = True
    probe['handle'] = unreal.register_slate_post_tick_callback(sample)
    return dict(started=True)


def balance_status():
    probe = unreal._mirror_balance_probe
    rows = probe['rows']
    moving = [row for row in rows if row['state'] in ('WALK', 'FLEE')]
    retract = [row for row in rows if row['state'] == 'MIRROR_RETRACT']
    transitions = []
    for row in rows:
        if not transitions or transitions[-1]['state'] != row['state']:
            transitions.append(dict(time=round(row['time'], 3), state=row['state']))
    return dict(finished=probe.get('finished', False), samples=len(rows), transitions=transitions, events=probe['events'],
                movement_frames=len(moving), advancing_frames=sum(row['step'] > .01 for row in moving),
                retract_angle_range=[min(row['angle'] for row in retract), max(row['angle'] for row in retract)] if retract else [],
                foot_lift_range=[min(row['feet'][0] for row in moving), max(row['feet'][0] for row in moving)] if moving else [])


def valid(obj):
    try:return unreal.SystemLibrary.is_valid(obj)
    except (TypeError,RuntimeError):return False


def spawn(world,cls,location,rotation):
    pose=unreal.Transform(location=location,rotation=rotation)
    lib=unreal.get_default_object(unreal.GameplayStatics.static_class())
    actor=lib.call_method('BeginDeferredActorSpawnFromClass',(world,cls,pose,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    return lib.call_method('FinishSpawningActor',(actor,pose,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))


def qa(step):
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world,'Start PIE first'
    pc=unreal.GameplayStatics.get_player_controller(world,0)
    if step=='qa_setup':
        pc.call_method('ServerRequestDebugPlanet',(unreal.Name('Moon'),False))
        return dict(travelling_to_moon=True)
    pawn=unreal.GameplayStatics.get_player_pawn(world,0)
    if not hasattr(unreal,'_mirror_defense_qa') or not valid(unreal._mirror_defense_qa['camera']):
        planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        nest=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat) if a.get_actor_label()=='Moon_Nest_01')
        frame=planet.get_surface_frame_at(nest.get_actor_location()+nest.get_actor_forward_vector()*700,nest.get_actor_forward_vector())
        base=frame.location;forward=frame.forward;right=frame.right;up=frame.up
        creatures=[]
        for i in range(3):
            point=planet.get_surface_frame_at(base+right*((i-1)*82),forward)
            c=spawn(world,unreal.load_asset(BP).generated_class(),point.location,point.transform.rotation.rotator())
            c.get_editor_property('health').set_max_health(1000,True)
            creatures.append(c)
        camera=spawn(world,unreal.CameraActor,base+forward*340+up*150,unreal.Rotator())
        camera.camera_component.set_field_of_view(65)
        pp=camera.camera_component.get_editor_property('post_process_settings')
        pp.set_editor_property('override_auto_exposure_method',True)
        pp.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
        pp.set_editor_property('override_auto_exposure_bias',True)
        pp.set_editor_property('auto_exposure_bias',9)
        camera.camera_component.set_editor_property('post_process_settings',pp)
        pc.set_view_target_with_blend(camera,0)
        unreal._mirror_defense_qa=dict(camera=camera,creatures=creatures,base=base,forward=forward,right=right,up=up,planet=planet,nest=nest,count=0)
    data=unreal._mirror_defense_qa
    camera=data['camera'];creatures=data['creatures'];base=data['base'];up=data['up'];forward=data['forward'];right=data['right']
    pp=camera.camera_component.get_editor_property('post_process_settings')
    pp.set_editor_property('override_auto_exposure_bias',True)
    pp.set_editor_property('auto_exposure_bias',0)
    pp.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True)
    pp.set_editor_property('auto_exposure_apply_physical_camera_exposure',False)
    camera.camera_component.set_editor_property('post_process_settings',pp)
    if step in ('qa_one','qa_two','qa_three','qa_clear'):
        data['count']={'qa_one':1,'qa_two':2,'qa_three':3,'qa_clear':0}[step]
    if step in ('qa_one','qa_two','qa_three','qa_clear','qa_front','qa_side','qa_back','qa_near'):
        direction=right if step=='qa_side' else -forward if step=='qa_back' else forward
        focus=creatures[0].get_actor_location() if step=='qa_near' else base
        location=focus+direction*(130 if step=='qa_near' else 340)+up*(95 if step=='qa_near' else 150)
        target=focus+up*20
        view=(target-location).normal()
        view_up=(up-view*view.dot(up)).normal()
        rotation=unreal.MathLibrary.make_rotation_from_axes(view,unreal.MathLibrary.cross_vector_vector(view_up,view),view_up)
        camera.set_actor_location_and_rotation(location,rotation,False,True)
        pc.set_view_target_with_blend(camera,0)
        # The camera manager consumes the changed target on its next tick; keep aim refreshed briefly.
        if hasattr(unreal,'_mirror_aim_tick'):
            unreal.unregister_slate_post_tick_callback(unreal._mirror_aim_tick)
        def aim(delta):
            for i,c in enumerate(creatures):
                if valid(c):
                    component=c.get_editor_property('mirror_defense')
                    if i<data['count']:component.aim_at(pawn)
                    else:component.stop_reflection()
        unreal._mirror_aim_tick=unreal.register_slate_post_tick_callback(aim)
        aim(0)
    if step=='qa_lookaway':
        camera.set_actor_rotation(unreal.MathLibrary.make_rotation_from_axes(forward,right,up),True)
    if step=='qa_unblock' and 'blocker' in data:
        data['blocker'].destroy_actor()
    if step=='qa_death':
        unreal.GameplayStatics.apply_damage(creatures[0],10000,pc,pawn,None)
    if step.startswith('qa_nest'):
        nest=data['nest'];planet=data['planet']
        if step=='qa_nest':
            if hasattr(unreal,'_mirror_aim_tick'):
                unreal.unregister_slate_post_tick_callback(unreal._mirror_aim_tick)
                del unreal._mirror_aim_tick
            for c in creatures:
                if valid(c):c.destroy_actor()
            pawn.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
            inv=pawn.get_component_by_class(unreal.JTSInventoryComponent)
            if inv.get_item_count(unreal.JTSItemId.PICKAXE)==0:
                if not inv.try_add_item_by_id(unreal.JTSItemId.PICKAXE,1):
                    item=inv.get_item_at_slot(0)
                    inv.call_method('ServerDropInventorySlot',(0,item.instance_id))
                    assert inv.try_add_item_by_id(unreal.JTSItemId.PICKAXE,1)
            for i in range(inv.get_inventory_capacity()):
                if inv.get_item_at_slot(i).item_id==unreal.JTSItemId.PICKAXE:
                    inv.call_method('ServerEquipInventorySlot',(i,inv.get_item_at_slot(i).instance_id));break
            mouth=nest.get_editor_property('refuge_mesh')
            anchor=nest.get_interaction_anchor_world_location()
            found=False
            for distance in (280,250,220,190,160,130,100,70):
                point=planet.get_surface_frame_at(anchor-mouth.get_forward_vector()*distance,mouth.get_forward_vector())
                pawn.set_actor_location_and_rotation(point.location+point.up*90,point.transform.rotation.rotator(),False,True)
                if nest.call_method('CanExecuteInteraction',(pawn,)):
                    found=True;break
            camera.set_actor_location_and_rotation(pawn.get_actor_location()+pawn.get_actor_up_vector()*55,pawn.get_actor_rotation(),False,True)
            data['work_possible']=found
            data['work_distance']=distance
            if found:
                nest.call_method('SetInteractionHeld',(pawn,True))
                nest.call_method('SetInteractionHeld',(pawn,False))
        if step=='qa_nest_start':
            anchor=nest.get_interaction_anchor_world_location()
            mouth=nest.get_editor_property('refuge_mesh')
            point=planet.get_surface_frame_at(anchor-mouth.get_forward_vector()*600,mouth.get_forward_vector())
            location=point.location+point.up*190
            view=(anchor+point.up*30-location).normal()
            view_up=(point.up-view*view.dot(point.up)).normal()
            camera.set_actor_location_and_rotation(location,unreal.MathLibrary.make_rotation_from_axes(view,view_up.cross(view),view_up),False,True)
            nest.call_method('SetInteractionHeld',(pawn,True))
        if step=='qa_nest_release':nest.call_method('SetInteractionHeld',(pawn,False))
        defenders=[c for c in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceCreature) if c.get_editor_property('refuge_actor')==nest or c.get_editor_property('habitat')==nest]
        widgets=unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.JTSMirrorGlareWidget,False)
        return dict(work_possible=data.get('work_possible'),work_distance=data.get('work_distance'),can_work_now=nest.call_method('CanExecuteInteraction',(pawn,)),active_item=str(pawn.get_component_by_class(unreal.JTSInventoryComponent).get_active_item_id()),progress=nest.get_editor_property('blockade').get_progress(),
                    health=pawn.get_component_by_class(unreal.JTSHealthComponent).get_health(),
                    detected=[w.get_illuminating_mirror_count() for w in widgets],
                    defenders=[dict(state=str(c.get_editor_property('state')),reflecting=c.get_editor_property('mirror_defense').is_reflecting(),
                                    distance=(nest.get_interaction_anchor_world_location()-c.get_actor_location()).length()) for c in defenders])
    if step=='qa_release':
        if hasattr(unreal,'_mirror_aim_tick'):
            unreal.unregister_slate_post_tick_callback(unreal._mirror_aim_tick)
            del unreal._mirror_aim_tick
        for c in creatures:
            c.get_editor_property('mirror_defense').stop_reflection()
        data['count']=0
    if step=='qa_occluded':
        # A disposable blocker tests the same world visibility channel as real Moon rocks.
        wall=spawn(world,unreal.StaticMeshActor,base+forward*160+up*80,camera.get_actor_rotation())
        wall.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        wall.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
        wall.set_actor_scale3d(unreal.Vector(.3,6,4))
        wall.static_mesh_component.set_collision_profile_name('BlockAll')
        data['blocker']=wall
    widgets=unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.JTSMirrorGlareWidget,False)
    if step=='qa_near':
        for w in widgets:w.set_visibility(unreal.SlateVisibility.COLLAPSED)
        if 'reflection_marker' not in data:
            marker=spawn(world,unreal.StaticMeshActor,creatures[0].get_actor_location()+forward*230+up*155,creatures[0].get_actor_rotation())
            marker.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
            marker.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
            marker.set_actor_scale3d(unreal.Vector(.9,.9,.9))
            data['reflection_marker']=marker
    elif step in ('qa_one','qa_two','qa_three','qa_clear','qa_front','qa_side','qa_back'):
        for w in widgets:w.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        if valid(data.get('reflection_marker')):
            data['reflection_marker'].destroy_actor();del data['reflection_marker']
    report=dict(requested=data['count'],detected=[w.get_illuminating_mirror_count() for w in widgets],
                player_health=pawn.get_component_by_class(unreal.JTSHealthComponent).get_health(),
                creatures=[dict(active=c.get_editor_property('mirror_defense').is_reflecting(),
                                visible=c.get_editor_property('mirror_defense').can_glare_at(pc),
                                location=xyz(c.get_actor_location()),normal=xyz(c.get_editor_property('mirror_defense').get_mirror_normal()),
                                feet=[xyz(c.get_editor_property('mesh').get_socket_location('leg_'+side+str(i)+'_toe')) for side in ('L','R') for i in range(1,4)]) for c in creatures])
    report['mirror_materials']=[[c.get_editor_property('mesh').get_material(i).get_name() for i in (1,2)] for c in creatures]
    if step=='qa_status':
        unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename='+str(OUT/('screen_'+str(data['count'])+'.png'))+' -nosuffix',pc)
        report['screenshot']=str(OUT/('screen_'+str(data['count'])+'.png'))
    return report


