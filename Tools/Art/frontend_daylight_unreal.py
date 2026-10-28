"""Import the asset-first daylight/threat revision via Unreal MCP; no UI or map edits."""
import unreal
import json
from pathlib import Path
REPORT_DIR=Path('D:/projects/space/Saved/Art/FrontEnd')
REPORT_DIR.mkdir(parents=True,exist_ok=True)
ROOT=Path(unreal.Paths.project_dir()).resolve();ART=ROOT/'SourceArt/FrontEnd';DEST='/Game/Space/FrontEnd'
EA=unreal.EditorAssetLibrary;AT=unreal.AssetToolsHelpers.get_asset_tools();ME=unreal.MaterialEditingLibrary
REPORT={'success':True,'imported':[],'materials':[]}
for filename,folder in [('Textures/T_FE_SkyGradient.png','Textures')]+[('Models/SM_FE_'+n+'.fbx','Models') for n in ['ThreatCore','ThreatHalo','ThreatFrontRings']]:
    path=ART/filename;task=unreal.AssetImportTask()
    for k,v in {'filename':str(path),'destination_path':DEST+'/'+folder,'destination_name':path.stem,'automated':True,'replace_existing':True,'save':True}.items():task.set_editor_property(k,v)
    if folder=='Models':
        options=unreal.FbxImportUI()
        for k,v in {'import_as_skeletal':False,'import_materials':False,'import_textures':False,'automated_import_should_detect_type':False,'mesh_type_to_import':unreal.FBXImportType.FBXIT_STATIC_MESH}.items():options.set_editor_property(k,v)
        options.static_mesh_import_data.set_editor_property('combine_meshes',True)
        task.set_editor_property('options',options);task.set_editor_property('factory',unreal.FbxFactory())
    AT.import_asset_tasks([task]);asset=unreal.load_asset(DEST+'/'+folder+'/'+path.stem);assert asset
    if folder=='Textures':asset.set_editor_property('srgb',True)
    EA.save_loaded_asset(asset,only_if_is_dirty=False);REPORT['imported'].append(asset.get_path_name())
for name,color,energy,additive in [('Core',(.5,.78,1),150,False),('Halo',(.12,.18,1),28,True),('FrontRings',(.3,.18,1),200,False)]:
    path=DEST+'/Materials/M_FE_Threat'+name
    mat=unreal.load_asset(path) or AT.create_asset('M_FE_Threat'+name,DEST+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    ME.delete_all_material_expressions(mat)
    mat.set_editor_property('two_sided',True);mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE if additive else unreal.BlendMode.BLEND_OPAQUE)
    tint=ME.create_material_expression(mat,unreal.MaterialExpressionVectorParameter)
    tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',unreal.LinearColor(*color,1))
    intensity=ME.create_material_expression(mat,unreal.MaterialExpressionScalarParameter)
    intensity.set_editor_property('parameter_name','Emission');intensity.set_editor_property('default_value',energy)
    multiply=ME.create_material_expression(mat,unreal.MaterialExpressionMultiply)
    ME.connect_material_expressions(tint,'',multiply,'A');ME.connect_material_expressions(intensity,'',multiply,'B')
    ME.connect_material_property(multiply,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if additive:
        opacity=ME.create_material_expression(mat,unreal.MaterialExpressionConstant);opacity.set_editor_property('r',.16)
        ME.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    ME.recompile_material(mat);EA.save_loaded_asset(mat,only_if_is_dirty=False);REPORT['materials'].append(path)
sky=unreal.load_asset(DEST+'/Materials/M_FE_Sky')
ME.delete_all_material_expressions(sky)
sky.set_editor_property('two_sided',True);sky.set_editor_property('is_sky',True)
sky.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
texture=ME.create_material_expression(sky,unreal.MaterialExpressionTextureSampleParameter2D)
texture.set_editor_property('parameter_name','Sky');texture.set_editor_property('texture',unreal.load_asset(DEST+'/Textures/T_FE_SkyGradient'))
energy=ME.create_material_expression(sky,unreal.MaterialExpressionScalarParameter)
energy.set_editor_property('parameter_name','Emission');energy.set_editor_property('default_value',100)
multiply=ME.create_material_expression(sky,unreal.MaterialExpressionMultiply)
ME.connect_material_expressions(texture,'RGB',multiply,'A');ME.connect_material_expressions(energy,'',multiply,'B')
ME.connect_material_property(multiply,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
ME.recompile_material(sky);EA.save_loaded_asset(sky,only_if_is_dirty=False)
(REPORT_DIR/'daylight_import_report.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
