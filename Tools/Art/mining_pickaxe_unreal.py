"""Configure the authored tool mesh and the Casual 2 rig without runtime asset paths."""
from pathlib import Path
import unreal


def configure():
    root=Path(unreal.Paths.project_dir()).resolve()
    folder='/Game/Space/Items/Tools/Pickaxe'
    task=unreal.AssetImportTask();task.set_editor_property('filename',str(root/'SourceArt/MiningPickaxe/SM_MiningPickaxe.fbx'))
    task.set_editor_property('destination_path',folder);task.set_editor_property('destination_name','SM_MiningPickaxe')
    task.set_editor_property('automated',True);task.set_editor_property('replace_existing',True);task.set_editor_property('save',True)
    options=unreal.FbxImportUI();options.set_editor_property('import_mesh',True);options.set_editor_property('import_materials',False)
    options.set_editor_property('import_textures',False);options.set_editor_property('import_as_skeletal',False)
    options.static_mesh_import_data.set_editor_property('combine_meshes',True)
    options.static_mesh_import_data.set_editor_property('auto_generate_collision',False)
    task.set_editor_property('options',options);unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh=unreal.load_asset(folder+'/SM_MiningPickaxe');assert mesh
    lib=unreal.MaterialEditingLibrary
    roles={'Frame':((.035,.065,.085),.72,.40),'Shell':((.52,.65,.70),.48,.34),
        'Grip':((.018,.028,.036),.05,.83),'Steel':((.13,.21,.27),.8,.35),
        'Edge':((.55,.72,.78),.9,.24),'Accent':((.95,.23,.035),.35,.42),
        'Energy':((.015,.75,1),.1,.25)}
    slots=list(mesh.get_editor_property('static_materials'))
    for slot in slots:
        role=next(r for r in roles if r.lower() in str(slot.get_editor_property('material_slot_name')).lower())
        name='M_Pickaxe_'+role;mat=unreal.load_asset(folder+'/'+name)
        if not mat:
            mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
            color,metal,rough=roles[role]
            c=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector);c.set_editor_property('constant',unreal.LinearColor(*color))
            lib.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
            for value,prop in ((metal,unreal.MaterialProperty.MP_METALLIC),(rough,unreal.MaterialProperty.MP_ROUGHNESS)):
                e=lib.create_material_expression(mat,unreal.MaterialExpressionConstant);e.set_editor_property('r',value);lib.connect_material_property(e,'',prop)
            if role=='Energy':
                e=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector)
                e.set_editor_property('constant',unreal.LinearColor(.015,2.4,3.))
                lib.connect_material_property(e,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            lib.recompile_material(mat)
        slot.set_editor_property('material_interface',mat);assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
    mesh.set_editor_property('static_materials',slots)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    item=unreal.load_asset('/Game/Space/Data/Items/DA_Item_Pickaxe');profile=item.get_editor_property('held_presentation')
    profile.set_editor_property('mesh',mesh);profile.set_editor_property('grip_transform',unreal.Transform())
    profile.set_editor_property('mesh_scale',.65)
    profile.set_editor_property('muzzle_transform',unreal.Transform(location=unreal.Vector(61.75,0,-23.4)))
    item.set_editor_property('held_presentation',profile);item.set_editor_property('world_pickup_mesh',mesh)
    item.set_editor_property('world_pickup_width',60.)
    assert unreal.EditorAssetLibrary.save_loaded_asset(item)
    paths=[p for p in unreal.EditorAssetLibrary.list_assets('/Game/Space',True)
        if p.rsplit('/',1)[-1].split('.')[0]=='BP_JTSPlayer_Casual_2']
    assert paths
    for path in paths:
        bp=unreal.load_asset(path);bp.modify()
        sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        lib=unreal.SubobjectDataBlueprintFunctionLibrary
        components=[lib.get_object_for_blueprint(lib.get_data(h),bp) or lib.get_object(lib.get_data(h))
            for h in sub.k2_gather_subobject_data_for_blueprint(bp)]
        strike=next(c for c in components if isinstance(c,unreal.JTSMiningStrikeComponent))
        strike.modify();strike.set_editor_property('elbow_reference_offset',unreal.Vector(3.2,16.1,27.6))
        strike.set_editor_property('forearm_reach',27.3);strike.set_editor_property('upper_arm_reach',18.2)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    for path in unreal.EditorAssetLibrary.list_assets(folder,False):
        name=path.rsplit('/',1)[-1].split('.')[0]
        if (name.startswith('Pickaxe_') or name=='M_Pickaxe_Wood') and not unreal.EditorAssetLibrary.find_package_referencers_for_asset(path):
            unreal.EditorAssetLibrary.delete_asset(path)
    return dict(mesh=mesh.get_path_name(),rigs=paths)
