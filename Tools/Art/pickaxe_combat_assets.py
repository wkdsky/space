"""Idempotent content configuration, invoked only through the scoped editor tool."""
import json
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
BPS='/Game/Space/Blueprints/Planets/Moon/EcologyV3'
MESHES='/Game/Space/Environment/MoonV3/Meshes'

def configure():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(),'Stop PIE first'
    import moon_surface_refinement as protected
    assert protected.check_protected()
    ea=unreal.EditorAssetLibrary
    lib=unreal.MaterialEditingLibrary
    path='/Game/Space/Environment/MoonV3/Materials/M_MS3_RiftSeamShadow'
    shadow=unreal.load_asset(path)
    if not shadow:
        shadow=unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(shadow)
    shadow.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    shadow.set_editor_property('two_sided',True)
    node=lib.create_material_expression(shadow,unreal.MaterialExpressionConstant3Vector)
    node.set_editor_property('constant',unreal.LinearColor(.001,.0013,.0015,1))
    lib.connect_material_property(node,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(shadow);assert ea.save_loaded_asset(shadow)
    stone=unreal.load_asset('/Game/Space/Environment/MoonV3/Materials/M_MS3_PropStone');assert stone
    rows=[]
    for i in range(4):
        name='SM_MS3_RiftRim_'+str(i)
        raw=json.loads((ROOT/'SourceArt/MoonSurfaceV3/RiftlipSeams'/(name+'.json')).read_text())
        dm=unreal.DynamicMesh()
        for mid in (0,1):
            selected=[t for t,m in zip(raw['triangles'],raw['material_ids']) if m==mid]
            used=sorted(set(v for tri in selected for v in tri));remap={v:j for j,v in enumerate(used)}
            dm.append_buffers_to_mesh(unreal.GeometryScriptSimpleMeshBuffers(
                vertices=[unreal.Vector(*raw['vertices'][v]) for v in used],
                triangles=[unreal.IntVector(remap[t[0]],remap[t[2]],remap[t[1]]) for t in selected],
                uv0=[unreal.Vector2D(raw['vertices'][v][0]/150,raw['vertices'][v][1]/60) for v in used]),mid)
        mesh=unreal.load_asset(MESHES+'/'+name);assert mesh
        unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dm,mesh,unreal.GeometryScriptCopyMeshToAssetOptions(),unreal.GeometryScriptMeshWriteLOD())
        mesh.set_editor_property('static_materials',[
            unreal.StaticMaterial(material_interface=stone,material_slot_name='RegolithEdge'),
            unreal.StaticMaterial(material_interface=shadow,material_slot_name='SeamShadow')])
        mesh.set_editor_property('allow_cpu_access',True)
        assert ea.save_loaded_asset(mesh)
        rows.append(dict(mesh=name,length_cm=raw['length_cm'],max_gap_cm=raw['max_gap_cm']))
    speeds=[]
    for kind,speed in [('MirrorSkimmer',230.),('Riftlip',255.)]:
        bp=unreal.load_asset(BPS+'/BP_'+kind);cdo=unreal.get_default_object(bp.generated_class())
        old=float(cdo.get_editor_property('flee_speed'))
        cdo.set_editor_property('flee_speed',speed)
        if kind=='Riftlip':cdo.set_editor_property('refuge_entry_distance',8.)
        rates=dict(cdo.get_editor_property('animation_play_rates'))
        rates[unreal.JTSForagerState.FLEE]=1.15
        cdo.set_editor_property('animation_play_rates',rates)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert ea.save_loaded_asset(bp)
        speeds.append(dict(kind=kind,before=old,after=speed))
    item=unreal.load_asset('/Game/Space/Data/Items/DA_Item_Pickaxe')
    item.set_editor_property('mining_reach',145.)
    item.set_editor_property('mining_swing_duration',.40)
    item.set_editor_property('mining_contact_fraction',.52)
    assert ea.save_loaded_asset(item)
    bp=unreal.load_asset('/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2')
    sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);sl=unreal.SubobjectDataBlueprintFunctionLibrary
    components=[sl.get_object_for_blueprint(sl.get_data(h),bp) or sl.get_object(sl.get_data(h)) for h in sub.k2_gather_subobject_data_for_blueprint(bp)]
    strike=next(c for c in components if isinstance(c,unreal.JTSMiningStrikeComponent))
    strike.set_editor_property('upper_arm_reach',18.2)
    strike.set_editor_property('torso_clearance',22.)
    strike.set_editor_property('aim_assist_degrees',7.)
    strike.set_editor_property('close_combat_assist_half_angle',45.)
    strike.set_editor_property('rebound_degrees',30.)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert ea.save_loaded_asset(bp)
    # The seam has an authored dark interior. A grazing sun must not turn its low edge into a metre-wide black wedge.
    def seam_recipe(entry):
        return any(m and m.get_name().startswith('SM_MS3_RiftRim_') for m in entry.get_editor_property('meshes'))
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if not isinstance(actor,unreal.JTSPlanetEcologyHabitat):continue
        scatter=actor.get_editor_property('surface_scatter');entries=list(scatter.get_editor_property('entries'))
        if not any(seam_recipe(e) for e in entries):continue
        actor.modify();scatter.modify()
        for e in entries:
            if seam_recipe(e):e.set_editor_property('cast_shadow',False)
        scatter.set_editor_property('entries',entries)
    distribution=unreal.load_asset('/Game/Space/Data/Planets/Moon/PDA_MoonSurfaceGameplay')
    layouts=list(distribution.get_editor_property('scatter_layouts'))
    for layout in layouts:
        props=list(layout.get_editor_property('props'))
        for prop in props:
            entry=prop.get_editor_property('recipe')
            if seam_recipe(entry):
                entry.set_editor_property('cast_shadow',False);prop.set_editor_property('recipe',entry)
        layout.set_editor_property('props',props)
    distribution.set_editor_property('scatter_layouts',layouts)
    assert ea.save_loaded_asset(distribution)
    assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
    # Keep the existing manifest transforms, generator references and protected landmarks.
    assert protected.check_protected()
    return dict(seams=rows,speeds=speeds,reach_cm=145,swing_seconds=.4,protected=True)
