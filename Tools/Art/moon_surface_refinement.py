"""Scoped Moon resource art/configuration and protected-landmark validation, via MCP."""
import hashlib
import json
from pathlib import Path
import unreal
import moon_surface_unreal as s

ART = s.ROOT / 'SourceArt/MoonResources'
QA = s.ROOT / 'Saved/MoonResources'
MESHES = '/Game/Space/Environment/MoonResources/Meshes'
MATERIALS = '/Game/Space/Environment/MoonResources/Materials'

def manifest(): return json.loads((ART/'manifest.json').read_text(encoding='utf-8'))

def check_protected(state=None):
    before=json.loads((QA/'protected_before.json').read_text())
    file=s.ROOT/'Content/ThirdParty/Environment/UltimateSpace/Meshes/Planet_3.uasset'
    transforms={n:v.split('{rotation:',1)[-1] for n,v in before['transforms'].items()}
    assert protected(state)==transforms, 'Protected landmark transforms changed'
    assert hashlib.sha256(file.read_bytes()).hexdigest()==before['moon_sha256'], 'Source Moon mesh changed'
    return True

def import_assets(batch):
    s.editor_world();lib=unreal.MaterialEditingLibrary
    from moon_surface_finish import linear
    mats={}
    for role,(h,roughness,metallic) in manifest()['palette'].items():
        name='M_MR_'+role
        mat=unreal.load_asset(MATERIALS+'/'+name)
        if not mat:mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,MATERIALS,unreal.Material,unreal.MaterialFactoryNew())
        lib.delete_all_material_expressions(mat)
        mat.set_editor_property('two_sided',False)
        mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED if role=='CarbonMargin' else unreal.BlendMode.BLEND_OPAQUE)
        tex=lib.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-650,0)
        tex.set_editor_property('texture',unreal.load_asset('/Game/Space/Environment/MoonV3/Textures/T_MoonMineral_Detail'))
        tex.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-850,0)
        uv.set_editor_property('u_tiling',4);uv.set_editor_property('v_tiling',4)
        lib.connect_material_expressions(uv,'',tex,'UVs')
        blend=lib.create_material_expression(mat,unreal.MaterialExpressionLinearInterpolate,-120,0)
        for pin,factor,y in (('A',.89,0),('B',1.04,150)):
            c=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-410,y)
            c.set_editor_property('constant',unreal.LinearColor(*(v*factor for v in linear(h)),1))
            lib.connect_material_expressions(c,'',blend,pin)
        lib.connect_material_expressions(tex,'R',blend,'Alpha')
        output=blend
        if role=='CarbonMargin':
            # UV0.Y is authored mineral coverage, preserved by the conforming mesh path.
            coverage=lib.create_material_expression(mat,unreal.MaterialExpressionComponentMask,-430,-300)
            coverage.set_editor_property('r',False)
            coverage.set_editor_property('g',True)
            coverage.set_editor_property('b',False)
            coverage.set_editor_property('a',False)
            coordinates=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-650,-300)
            assert lib.connect_material_expressions(coordinates,'',coverage,'')
            invert=lib.create_material_expression(mat,unreal.MaterialExpressionOneMinus,-250,-300)
            assert lib.connect_material_expressions(coverage,'',invert,'')
            substrate=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-410,-500)
            substrate.set_editor_property('constant',unreal.LinearColor(*linear('#BFC0C4'),1))
            output=lib.create_material_expression(mat,unreal.MaterialExpressionLinearInterpolate,120,0)
            lib.connect_material_expressions(substrate,'',output,'A')
            lib.connect_material_expressions(blend,'',output,'B')
            # FBX flips V on import: Blender coverage 1 becomes Unreal V 0.
            lib.connect_material_expressions(invert,'',output,'Alpha')
            feather=lib.create_material_expression(mat,unreal.MaterialExpressionMaterialFunctionCall,120,-300)
            feather.set_material_function(unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA'))
            assert lib.connect_material_expressions(invert,'',feather,'')
            assert lib.connect_material_property(feather,'',unreal.MaterialProperty.MP_OPACITY_MASK)
        lib.connect_material_property(output,'',unreal.MaterialProperty.MP_BASE_COLOR)
        for property,value,y in ((unreal.MaterialProperty.MP_ROUGHNESS,roughness,300),(unreal.MaterialProperty.MP_METALLIC,metallic,450)):
            node=lib.create_material_expression(mat,unreal.MaterialExpressionConstant,-120,y);node.set_editor_property('r',value)
            lib.connect_material_property(node,'',property)
        lib.recompile_material(mat);s.save(mat);mats[name]=mat
    rows=manifest()['assets'][batch*21:(batch+1)*21]
    s.import_tasks([s.fbx_task(ART/'FBX'/(r['name']+'.fbx'),MESHES,r['name']) for r in rows])
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for r in rows:
        mesh=unreal.load_asset(MESHES+'/'+r['name']);assert mesh
        mesh.set_editor_property('allow_cpu_access',True)
        slots=list(mesh.static_materials)
        for slot in slots:
            assert str(slot.material_slot_name) in mats,str(slot.material_slot_name)
            slot.material_interface=mats[str(slot.material_slot_name)]
        mesh.set_editor_property('static_materials',slots)
        sm.remove_collisions(mesh)
        if '_Carbon_' not in r['name']:assert sm.set_convex_decomposition_collisions(mesh,1,16,100000)
        s.save(mesh)
    check_protected()
    return dict(imported=len(rows),batch=batch)

def protected(state=None):
    if state is None:state = s.actors()
    names = ['MoonPlanet', 'Moon_Corpse_SkeletonAstronaut'] + [f'Moon_Nest_{i:02d}' for i in range(1, 10)]
    return {n: str(state[n].get_actor_transform()).split('{rotation:',1)[1] for n in names}

def mesh(name):
    value=unreal.load_asset(MESHES+'/'+name);assert value,name
    return value

def variants(kind,size):return [mesh(f'SM_MR_{kind}_{size}_{i:02d}') for i in range(1,7)]

def carbon_entry(count, distance=(140,460)):
    e=unreal.JTSPlanetSurfaceScatterEntry()
    values=dict(meshes=[mesh(r['name']) for r in manifest()['assets'] if '_Carbon_' in r['name']],count=count,
        minimum_spacing=100,scale_range=unreal.Vector2D(.65,1.18),conform_to_surface=True,cast_shadow=False,
        resource_item=unreal.load_asset('/Game/Space/Data/Items/DA_Item_InorganicCarbon'),resource_units=1,
        near_refuge=True,refuge_distance=unreal.Vector2D(*distance))
    for k,v in values.items():e.set_editor_property(k,v)
    return e

def configure_rock_art():
    # Only medium/large nodes carry inclusions. Loose pickups are separate pure fragments.
    paths=['/Game/Space/Blueprints/Modes/BP_MoonResourceActor','/Game/Space/Blueprints/Modes/Moon/BP_MoonResourceActor']
    for path in paths:
        bp=unreal.load_asset(path)
        if not bp:continue
        defaults=unreal.get_default_object(bp.generated_class())
        for prop,kind,size in [('medium_rock_meshes','Rock','Medium'),('large_rock_meshes','Rock','Large'),
            ('medium_metal_rock_meshes','Metal','Medium'),('large_metal_rock_meshes','Metal','Large'),
            ('medium_crystal_rock_meshes','Crystal','Medium'),('large_crystal_rock_meshes','Crystal','Large')]:
            defaults.set_editor_property(prop,variants(kind,size))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp);s.save(bp)
    for item,kind in [('Rock','Rock'),('Ore','Metal'),('Crystal','Crystal')]:
        data=unreal.load_asset('/Game/Space/Data/Items/DA_Item_'+item);assert data,item
        loose=variants(kind,'Small' if item=='Rock' else 'Loose')
        data.set_editor_property('world_pickup_mesh',loose[0])
        data.set_editor_property('world_pickup_mesh_variants',loose)
        data.set_editor_property('world_pickup_width',32 if item=='Rock' else 23)
        data.set_editor_property('display_name','石头' if item=='Rock' else '矿物')
        s.save(data)
    return paths


def configure():
    s.editor_world();check_protected();state=s.actors()
    moon=state['MoonPlanet'];corpse=state['Moon_Corpse_SkeletonAstronaut']
    planet=next(a for a in state.values() if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='Moon')
    fit=json.loads((s.ROOT/'SourceArt/SkeletonAstronaut/moon_crater_fit.json').read_text())
    source=json.loads((s.ROOT/'SourceArt/SkeletonAstronaut/moon_reference_geometry.json').read_text())
    center=moon.get_actor_transform().transform_location(unreal.Vector(*(v*100 for v in fit['center_surface_local_m'])))
    label='Moon_MainCrater_Clearance'
    marker=state.get(label) or s.editor().spawn_actor_from_class(unreal.TargetPoint,center,unreal.Rotator())
    marker.set_actor_label(label);marker.set_actor_location(center,False,False)
    marker.set_folder_path('Planets/Moon/Protected');marker.set_editor_property('tags',['MoonMainCraterClearance'])
    # Rim indices belong to the corpse fitting export, not the differently ordered UE extraction.
    rim=[moon.get_actor_transform().transform_location(unreal.Vector(*(v*100 for v in source['vertices'][i]))) for i in fit['rim_vertices']]
    clearance=max(planet.approximate_surface_arc_distance(center,p) for p in rim)+400
    assert 6000<clearance<14000,clearance
    paths=configure_rock_art()
    path='/Game/Space/Data/Items/DA_Item_InorganicCarbon'
    carbon=unreal.load_asset(path)
    if not carbon:carbon=s.EA.duplicate_asset('/Game/Space/Data/Items/DA_Item_Rock',path)
    carbon.set_editor_property('item_id',unreal.JTSItemId.INORGANIC_CARBON)
    carbon.set_editor_property('display_name','无机碳')
    carbon.set_editor_property('description','碳酸盐胶结沉积。月球生物活动后留下的矿化中间物，常附着在裂缝、巢塔和洞口外围。')
    carbon.set_editor_property('world_pickup_mesh',mesh('SM_MR_Carbon_Accretion_02'))
    carbon.set_editor_property('world_pickup_mesh_variants',[mesh(f'SM_MR_Carbon_Accretion_{i:02d}') for i in range(1,4)])
    carbon.set_editor_property('world_pickup_width',30)
    carbon.set_editor_property('accent_color',unreal.LinearColor(.62,.67,.55,1));s.save(carbon)
    habitats=[a for a in state.values() if isinstance(a,unreal.JTSPlanetEcologyHabitat)]
    configured=0
    for a in habitats:
        a.modify();sc=a.get_editor_property('surface_scatter')
        sc.set_editor_property('exclusion_actors',[marker]);sc.set_editor_property('exclusion_radius',clearance)
        entries=[e for e in sc.get_editor_property('entries') if not e.get_editor_property('resource_item')]
        # Only the original nine mouths are destructible. Peripheral fissures remain independent props.
        for e in entries:
            e.set_editor_property('destructible',False)
            if e.get_editor_property('refuge'):e.set_editor_property('minimum_spacing',200)
        if a.get_actor_label().endswith('_Rift_Core'):
            entries.append(carbon_entry(4,(240,600)));configured+=1
        if a.get_actor_label().endswith('_Workers'):
            entries.append(carbon_entry(5,(120,330)));configured+=1
        sc.set_editor_property('entries',entries)
    feeding=unreal.load_asset(s.BPS+'/BP_MoonMirrorFeedingArea')
    defaults=unreal.get_default_object(feeding.generated_class());sc=defaults.get_editor_property('surface_scatter')
    entries=[e for e in sc.get_editor_property('entries') if not e.get_editor_property('resource_item')]
    entries.append(carbon_entry(3,(180,420)));sc.set_editor_property('entries',entries)
    unreal.BlueprintEditorLibrary.compile_blueprint(feeding);s.save(feeding)
    # Reuse the project's area-resource generator, with a zero-density winning crater region.
    spawner_bp=unreal.load_asset('/Game/Space/Blueprints/Planets/Moon/BP_MoonResourceSpawner');assert spawner_bp
    defaults=unreal.get_default_object(spawner_bp.generated_class())
    defaults.set_editor_property('resource_actor_class',unreal.load_asset(paths[0]).generated_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(spawner_bp);s.save(spawner_bp)
    resource_regions=[]
    for row in s.recipe()['regions']:
        core=state['MoonV3_'+row['id']+'_Rift_Core'];name='MoonResources_'+row['id']
        a=state.get(name) or s.editor().spawn_actor_from_class(spawner_bp.generated_class(),core.get_actor_location(),core.get_actor_rotation())
        a.set_actor_label(name);a.set_folder_path('Planets/Moon/Resources');a.modify()
        for k,v in dict(resource_density=.32,override_coverage_radius=True,radius=row['core_radius_m']*100,
            coverage_planet=planet,exclusion_actors=[marker],exclusion_radius=clearance,overlap_priority=0).items():a.set_editor_property(k,v)
        resource_regions.append(a)
    crater=state.get('MoonResources_MainCrater_Empty') or s.editor().spawn_actor_from_class(spawner_bp.generated_class(),center,unreal.Rotator())
    crater.set_actor_label('MoonResources_MainCrater_Empty');crater.set_folder_path('Planets/Moon/Protected')
    for k,v in dict(resource_density=0,override_coverage_radius=True,radius=clearance,coverage_planet=planet,overlap_priority=-100).items():crater.set_editor_property(k,v)
    check_protected()
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    result=dict(protected=True,crater_clearance_cm=clearance,resource_regions=len(resource_regions),carbon_habitats=configured,
        fixed_nests=9,models=len(manifest()['assets']))
    (QA/'configuration.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return result

def inspect():
    s.editor_world(); state = s.actors()
    corpse = state['Moon_Corpse_SkeletonAstronaut']
    planet = next(a for a in state.values() if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'Moon')
    rows = []
    for a in state.values():
        d = planet.approximate_surface_arc_distance(corpse.get_actor_location(), a.get_actor_location())
        if isinstance(a, (unreal.JTSPlanetEcologyHabitat, unreal.JTSPlanetColonyNest, unreal.JTSMoonResourceSpawner)):
            row = dict(label=a.get_actor_label(), distance=round(d), cls=a.get_class().get_name(), position=s.xyz(a.get_actor_location()))
            if isinstance(a, unreal.JTSPlanetEcologyHabitat):
                sc = a.get_editor_property('surface_scatter')
                row.update(core=a.get_editor_property('core_radius'), exclusion=sc.get_editor_property('exclusion_radius'),
                    entries=[dict(meshes=[m.get_name() for m in e.get_editor_property('meshes')], count=e.get_editor_property('count')) for e in sc.get_editor_property('entries')])
            rows.append(row)
    QA.mkdir(parents=True, exist_ok=True)
    file = s.ROOT/'Content/ThirdParty/Environment/UltimateSpace/Meshes/Planet_3.uasset'
    baseline = dict(transforms=protected(), moon_sha256=hashlib.sha256(file.read_bytes()).hexdigest())
    if not (QA/'protected_before.json').exists():
        (QA/'protected_before.json').write_text(json.dumps(baseline, indent=2), encoding='utf-8')
    return dict(corpse=s.xyz(corpse.get_actor_location()), center=s.xyz(planet.get_planet_center()),
        moon_materials=[str(m) for m in state['MoonPlanet'].static_mesh_component.get_materials()], habitats=sorted(rows, key=lambda r:r['distance']))

def validate_runtime():
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert world
    state={a.get_actor_label():a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)}
    planet=next(a for a in state.values() if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='Moon')
    marker=state['Moon_MainCrater_Clearance'];radius=json.loads((QA/'configuration.json').read_text())['crater_clearance_cm']
    attachments=[a for a in state.values() if isinstance(a,unreal.JTSPlanetSurfaceAttachment)]
    banned=[];carbon=[];mouths=[];types={}
    for a in attachments:
        name=a.get_component_by_class(unreal.StaticMeshComponent).get_editor_property('static_mesh').get_name()
        if name.startswith('SM_MR_Carbon_'):carbon.append(a)
        if 'FixedNest' in name:mouths.append(a)
        elif planet.approximate_surface_arc_distance(marker.get_actor_location(),a.get_actor_location())<radius-1:banned.append(name)
        types[name.split('_')[2]]=types.get(name.split('_')[2],0)+1
    rocks=[a for a in state.values() if isinstance(a,unreal.JTSMoonResourceActor)]
    pickups=[a for a in state.values() if isinstance(a,unreal.JTSWorldPickupActor)
        and (a.get_actor_location()-planet.get_planet_center()).length()<50000]
    for a in rocks+pickups:
        if planet.approximate_surface_arc_distance(marker.get_actor_location(),a.get_actor_location())<radius-1:banned.append(a.get_name())
    assert not banned,banned
    assert len(mouths)==9,len(mouths)
    assert len(carbon)>15,len(carbon)
    assert len(rocks)>20,len(rocks)
    assert all(a.get_editor_property('replicates') for a in carbon+rocks)
    references=[a for a in attachments if a not in carbon]+[a for a in state.values() if isinstance(a,unreal.JTSPlanetColonyNest)]
    distances=[min(planet.approximate_surface_arc_distance(a.get_actor_location(),r.get_actor_location()) for r in references) for a in carbon]
    kinds={}
    for a in rocks:
        n=str(a.get_node_size());kinds[n]=kinds.get(n,0)+1
    result=dict(fixed_mouths=len(mouths),extra_content_in_crater=banned,carbon_deposits=len(carbon),mining_nodes=len(rocks),loose_pickups=len(pickups),
        carbon_nearest_refuge_cm=[round(min(distances)),round(max(distances))],rock_kinds=kinds,replicated=True,protected_original_assets=check_protected(state))
    (QA/'runtime_validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return result

def collect_probe():
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert world
    deposits=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceAttachment)
        if a.get_component_by_class(unreal.StaticMeshComponent).get_editor_property('static_mesh').get_name().startswith('SM_MR_Carbon_')]
    assert deposits
    player=unreal.GameplayStatics.get_player_pawn(world,0);assert player
    inventory=player.get_component_by_class(unreal.JTSInventoryComponent)
    if not inventory.can_add_item(unreal.JTSItemId.INORGANIC_CARBON,1):
        assert inventory.drop_item_at_slot(0), 'Could not free a temporary PIE inventory slot'
    before=inventory.get_item_count(unreal.JTSItemId.INORGANIC_CARBON)
    a=deposits[0];planet=a.get_owner();up=planet.get_radial_up_vector(a.get_actor_location())
    forward=a.get_actor_forward_vector()
    player.set_actor_location_and_rotation(a.get_actor_location()+up*100-forward*120,s.rotation(forward,up),False,True)
    a.call_method('Interact',(player,))
    after=inventory.get_item_count(unreal.JTSItemId.INORGANIC_CARBON)
    assert after==before+1,(before,after)
    ship=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSSpacecraftActor))
    stored=ship.get_resource_amount(unreal.JTSResourceType.INORGANIC_CARBON)
    player.set_actor_location(ship.get_boarding_interaction_center(),False,True)
    ship.try_deposit_resources_from_pawn(player)
    assert inventory.get_item_count(unreal.JTSItemId.INORGANIC_CARBON)==0
    assert ship.get_resource_amount(unreal.JTSResourceType.INORGANIC_CARBON)==stored+after
    result=dict(collected=after-before,shared_ship_deposit=after,inventory_cleared=True)
    (QA/'collection_validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return result

def interaction_probe(prepare):
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert world
    player=unreal.GameplayStatics.get_player_pawn(world,0);inventory=player.get_component_by_class(unreal.JTSInventoryComponent)
    pc=unreal.GameplayStatics.get_player_controller(world,0)
    if prepare:
        if hasattr(unreal,'_moon_carbon_interaction'):
            old=unreal._moon_carbon_interaction[1]
            if old in list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor)):old.destroy_actor()
        if not inventory.can_add_item(unreal.JTSItemId.INORGANIC_CARBON,1):assert inventory.drop_item_at_slot(0)
        pickups=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSWorldPickupActor)
        deposits=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceAttachment)
            if a.get_component_by_class(unreal.StaticMeshComponent).get_editor_property('static_mesh').get_name().startswith('SM_MR_Carbon_')
            and all((a.get_actor_location()-p.get_actor_location()).length()>500 for p in pickups)]
        # Pick the lower hemisphere to exercise radial eye-position validation.
        a=min(deposits,key=lambda a:a.get_actor_location().z-a.get_owner().get_planet_center().z)
        up=a.get_owner().get_radial_up_vector(a.get_actor_location());forward=a.get_actor_forward_vector()
        player.set_actor_location_and_rotation(a.get_actor_location()+up*100-forward*150,s.rotation(forward,up),False,True)
        eye=player.get_actor_location()+up*60;rot=s.rotation((a.get_actor_location()-eye).normal(),up)
        pose=unreal.Transform(location=eye,rotation=rot)
        lib=unreal.get_default_object(unreal.GameplayStatics.static_class())
        cam=lib.call_method('BeginDeferredActorSpawnFromClass',(world,unreal.CameraActor.static_class(),pose,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        cam=lib.call_method('FinishSpawningActor',(cam,pose,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        pc.set_view_target_with_blend(cam,0)
        unreal._moon_carbon_interaction=(a,cam,inventory.get_item_count(unreal.JTSItemId.INORGANIC_CARBON))
        return dict(prepared=True,prompt=str(a.call_method('GetInteractionPrompt',(player,))),resource_name=str(a.get_interaction_display_name()))
    a,cam,before=unreal._moon_carbon_interaction
    component=player.get_component_by_class(unreal.InteractionComponent);component.refresh_interactable()
    selected=component.get_current_interactable();assert selected==a,str(selected)
    prompt=str(component.get_current_interaction_prompt());assert prompt
    assert component.try_interact()
    after=inventory.get_item_count(unreal.JTSItemId.INORGANIC_CARBON);assert after==before+1,(before,after)
    pc.set_view_target_with_blend(player,0);cam.destroy_actor()
    result=dict(normal_interaction_selected=True,prompt=prompt,collected=after-before,lower_hemisphere=True)
    (QA/'interaction_validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return result

def legacy_candidates():
    paths=['/Game/'+p.relative_to(s.ROOT/'Content').as_posix().removesuffix('.uasset')
        for p in (s.ROOT/'Content/Space/Meshes/Resources/Moon').glob('*.uasset')]
    paths += ['/Game/'+p.relative_to(s.ROOT/'Content').as_posix().removesuffix('.uasset')
        for p in (s.ROOT/'Content/ThirdParty/Moon/Resources/Rock(Kenney)').glob('*.uasset')
        if p.name.lower().startswith('rock') and 'rocket' not in p.name.lower()]
    paths += [s.MESHES+f'/SM_MS3_Rock_{i}' for i in range(3)
        if unreal.EditorAssetLibrary.does_asset_exist(s.MESHES+f'/SM_MS3_Rock_{i}')]
    return paths

def legacy_audit():
    s.editor_world()
    return {p:list(unreal.EditorAssetLibrary.find_package_referencers_for_asset(p,True) or []) for p in legacy_candidates()}

def remove_legacy():
    refs=legacy_audit();packages=set(refs)
    external={p:[r for r in rows if r not in packages] for p,rows in refs.items() if any(r not in packages for r in rows)}
    assert not external,external
    assets=[unreal.load_asset(p) for p in refs];assert all(assets)
    files=[s.ROOT/'Content'/(p.removeprefix('/Game/')+'.uasset') for p in refs]
    result=dict(removed=list(refs),bytes_removed=sum(p.stat().st_size for p in files),external_references=external)
    assert unreal.EditorAssetLibrary.delete_loaded_assets(assets)
    assert all(not p.exists() for p in files)
    check_protected()
    (QA/'legacy_cleanup.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return result

def refresh_art():
    s.editor_world();check_protected();configure_rock_art()
    carbon=unreal.load_asset('/Game/Space/Data/Items/DA_Item_InorganicCarbon')
    choices=[mesh(f'SM_MR_Carbon_Accretion_{i:02d}') for i in range(1,4)]
    carbon.set_editor_property('world_pickup_mesh',choices[1])
    carbon.set_editor_property('world_pickup_mesh_variants',choices);s.save(carbon)
    updated=0
    def replace(sc):
        nonlocal updated
        entries=list(sc.get_editor_property('entries'))
        for entry in entries:
            if entry.get_editor_property('resource_item')==carbon:
                entry.set_editor_property('meshes',choices)
                # Millimetre film edges must not project a detached sheet shadow onto the substrate.
                entry.set_editor_property('cast_shadow',False);updated+=1
        sc.set_editor_property('entries',entries)
    for a in s.actors().values():
        if isinstance(a,unreal.JTSPlanetEcologyHabitat):a.modify();replace(a.get_editor_property('surface_scatter'))
    feeding=unreal.load_asset(s.BPS+'/BP_MoonMirrorFeedingArea')
    replace(unreal.get_default_object(feeding.generated_class()).get_editor_property('surface_scatter'))
    unreal.BlueprintEditorLibrary.compile_blueprint(feeding);s.save(feeding)
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return dict(updated_carbon_scatter_entries=updated,carbon_form='Accretion',outline_variants=3,protected=check_protected())


def cleanup_art():
    s.editor_world();current={r['name'] for r in manifest()['assets']}
    obsolete=[MESHES+'/'+p.stem for p in (s.ROOT/'Content/Space/Environment/MoonResources/Meshes').glob('SM_MR_Carbon_*.uasset') if p.stem not in current]
    obsolete+=legacy_candidates();packages=set(obsolete)
    refs={p:list(s.EA.find_package_referencers_for_asset(p,True) or []) for p in obsolete}
    external={p:[r for r in rows if r not in packages] for p,rows in refs.items() if any(r not in packages for r in rows)}
    assert not external,external
    if obsolete:assert s.EA.delete_loaded_assets([unreal.load_asset(p) for p in obsolete])
    return dict(deleted=obsolete,protected=check_protected())


def run(stage):
    if stage == 'materials':return json.dumps(import_assets(-1))
    if stage == 'art_probe':
        asset=mesh('SM_MR_Carbon_Accretion_01');result=[]
        for i in range(asset.get_num_sections(0)):
            vertices,triangles,normals,uvs,tangents=unreal.ProceduralMeshLibrary.get_section_from_static_mesh(asset,0,i)
            result.append(dict(section=i,material=asset.get_material(i).get_name(),vertices=len(vertices),
                normal_z=[min(n.z for n in normals),max(n.z for n in normals)],
                uv_y=[min(uv.y for uv in uvs),max(uv.y for uv in uvs)]))
            if i==0:
                indices=sorted(range(len(vertices)),key=lambda j:vertices[j].x**2+vertices[j].y**2)
                result[-1]['samples']=[dict(pos=s.xyz(vertices[j]),uv=[uvs[j].x,uvs[j].y]) for j in [indices[0],indices[-1]]]
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if world:
            a=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceAttachment) if a.get_component_by_class(unreal.StaticMeshComponent).get_editor_property('static_mesh')==asset)
            p=a.get_component_by_class(unreal.ProceduralMeshComponent)
            v,t,n,uv,tan=unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(p,0)
            result.append(dict(conformed_normal_z=[min(v.z for v in n),max(v.z for v in n)]))
        return json.dumps(result)
    if stage == 'refresh_art':return json.dumps(refresh_art())
    if stage == 'cleanup_art':return json.dumps(cleanup_art())
    if stage == 'inspect': return json.dumps(inspect(), ensure_ascii=False)
    if stage.startswith('import_'):return json.dumps(import_assets(int(stage[-1])))
    if stage == 'configure':return json.dumps(configure())
    if stage == 'runtime':return json.dumps(validate_runtime())
    if stage == 'collect':return json.dumps(collect_probe())
    if stage == 'interaction_prepare':return json.dumps(interaction_probe(True))
    if stage == 'interaction_collect':return json.dumps(interaction_probe(False))
    if stage == 'legacy_audit':return json.dumps(legacy_audit())
    if stage == 'remove_legacy':return json.dumps(remove_legacy())
    if stage == 'final':
        s.editor_world();check_protected();assert not legacy_audit()
        assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
        return json.dumps(dict(protected_original_assets=True,obsolete_resource_assets=0,authored_models=len(manifest()['assets']),saved=True))
    if stage == 'shutdown':
        check_protected()
        assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
        unreal.SystemLibrary.quit_editor()
        return 'Saved; editor closing for a normal build.'
    raise ValueError(stage)

