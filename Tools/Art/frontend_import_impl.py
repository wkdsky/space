"""Import prepared front-end source assets into an isolated art directory.

Called only through FrontEndArtTools.run_frontend_step('import'). Does not touch live maps/UI.
"""
import unreal
import json
import hashlib
from pathlib import Path
REPORT_DIR=Path('D:/projects/space/Saved/Art/FrontEnd')
REPORT_DIR.mkdir(parents=True,exist_ok=True)

ROOT=Path(unreal.Paths.project_dir()).resolve()
SOURCE=ROOT/'SourceArt/FrontEnd'
SOURCE_FILES={entry['file'] for entry in json.loads((SOURCE/'asset_manifest.json').read_text(encoding='utf-8'))['assets'] if not entry.get('authoring_only')}
DEST='/Game/Space/FrontEnd'
EA=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
REPORT={'imported':[],'errors':[],'materials':[]}


def save(obj):
    assert EA.save_loaded_asset(obj,only_if_is_dirty=False),obj.get_path_name()


def import_file(path,folder,options=None,factory=None,name=None):
    assert path.relative_to(SOURCE).as_posix() in SOURCE_FILES, path
    destination=DEST+'/'+folder+'/'+(name or path.stem)
    fingerprint=hashlib.sha256(path.read_bytes()).hexdigest()
    if folder=='Animations':fingerprint+=':fbx-centimetres-100'
    if EA.does_asset_exist(destination):
        existing=unreal.load_asset(destination)
        if EA.get_metadata_tag(existing,'JTS.SourceSHA256')==fingerprint:return existing
    task=unreal.AssetImportTask()
    task.set_editor_property('filename',str(path))
    task.set_editor_property('destination_path',DEST+'/'+folder)
    task.set_editor_property('destination_name',name or path.stem)
    task.set_editor_property('automated',True)
    task.set_editor_property('replace_existing',True)
    task.set_editor_property('save',True)
    if options:task.set_editor_property('options',options)
    if factory:task.set_editor_property('factory',factory)
    TOOLS.import_asset_tasks([task])
    assets=list(task.get_editor_property('imported_object_paths'))
    if not assets:raise RuntimeError('No imported asset for '+str(path))
    asset=unreal.load_asset(destination) or unreal.load_asset(assets[0])
    EA.set_metadata_tag(asset,'JTS.SourceSHA256',fingerprint)
    save(asset)
    REPORT['imported'].append(asset.get_path_name());return asset


def expression(material,cls,**properties):
    node=unreal.MaterialEditingLibrary.create_material_expression(material,cls)
    for key,value in properties.items():node.set_editor_property(key,value)
    return node


def parameter(material,name,color):
    return expression(material,unreal.MaterialExpressionVectorParameter,parameter_name=name,default_value=unreal.LinearColor(*color))


def scalar(material,name,value):
    return expression(material,unreal.MaterialExpressionScalarParameter,parameter_name=name,default_value=value)


def connect_prop(node,prop,output=''):
    unreal.MaterialEditingLibrary.connect_material_property(node,output,prop)


def connect(a,b,port,output=''):
    assert unreal.MaterialEditingLibrary.connect_material_expressions(a,output,b,port)


for folder in ['Textures','UI']:
    for path in sorted((SOURCE/folder).glob('*.png')):
        if path.relative_to(SOURCE).as_posix() not in SOURCE_FILES:continue
        texture=import_file(path,folder)
        normal=folder=='Textures' and path.stem.endswith('_Normal')
        if normal:
            texture.set_editor_property('srgb',False)
            texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
        elif path.stem.endswith('_ORM'):
            texture.set_editor_property('srgb',False)
            texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
        elif folder=='UI':
            texture.set_editor_property('srgb',True)
            texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
            texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        elif path.stem=='T_FE_SkyGradient':texture.set_editor_property('srgb',False)
        texture.set_editor_property('max_texture_size',2048)
        save(texture)

# Twelve reusable material parents, configured by instances; no runtime paths in C++.
parent_kinds=['Suit','Visor','Rubber','PaintedMetal','Concrete','Glass','Light','Decal','Particle','Moon','Sky']
for kind in parent_kinds:
    name='M_FE_'+kind;path=DEST+'/Materials/'+name
    mat=unreal.load_asset(path)
    if not mat:mat=TOOLS.create_asset(name,DEST+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    base=parameter(mat,'Tint',(.75,.74,.68,1))
    rough=scalar(mat,'Roughness',.62);metal=scalar(mat,'Metallic',.05)
    connect_prop(base,unreal.MaterialProperty.MP_BASE_COLOR)
    connect_prop(rough,unreal.MaterialProperty.MP_ROUGHNESS)
    connect_prop(metal,unreal.MaterialProperty.MP_METALLIC)
    if kind in ['Suit','Rubber','PaintedMetal','Concrete','Moon']:
        tex_name='T_FE_Concrete_Normal' if kind=='Concrete' else 'T_FE_Moon_Normal' if kind=='Moon' else 'T_FE_Suit0_Normal'
        node=expression(mat,unreal.MaterialExpressionTextureSampleParameter2D,parameter_name='NormalMap',texture=unreal.load_asset(DEST+'/Textures/'+tex_name),sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        flat=parameter(mat,'FlatNormal',(0,0,1,0))
        strength=scalar(mat,'NormalStrength',.15 if kind=='Suit' else .4)
        normal_mix=expression(mat,unreal.MaterialExpressionLinearInterpolate)
        connect(flat,normal_mix,'A');connect(node,normal_mix,'B','RGB');connect(strength,normal_mix,'Alpha')
        connect_prop(normal_mix,unreal.MaterialProperty.MP_NORMAL)
        if kind!='Moon':
            orm_name={'Suit':'Suit0','Rubber':'Rubber','PaintedMetal':'Metal','Concrete':'Concrete'}[kind]
            orm=expression(mat,unreal.MaterialExpressionTextureSampleParameter2D,parameter_name='ORM',texture=unreal.load_asset(DEST+'/Textures/T_FE_'+orm_name+'_ORM'),sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
            connect_prop(orm,unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,'R')
            connect_prop(orm,unreal.MaterialProperty.MP_ROUGHNESS,'G')
            connect_prop(orm,unreal.MaterialProperty.MP_METALLIC,'B')
        if kind in ['Concrete','Moon']:
            tex_name='T_FE_'+kind+'_BaseColor'
            albedo=expression(mat,unreal.MaterialExpressionTextureSampleParameter2D,parameter_name='BaseMap',texture=unreal.load_asset(DEST+'/Textures/'+tex_name))
            multiply=expression(mat,unreal.MaterialExpressionMultiply);connect(base,multiply,'A');connect(albedo,multiply,'B','RGB');connect_prop(multiply,unreal.MaterialProperty.MP_BASE_COLOR)
    if kind in ['Light','Decal','Particle','Visor']:
        emission=scalar(mat,'Emission',.4 if kind=='Visor' else 1)
        mult=expression(mat,unreal.MaterialExpressionMultiply);connect(base,mult,'A');connect(emission,mult,'B');connect_prop(mult,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if kind in ['Decal','Particle']:
        mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT if kind=='Particle' else unreal.BlendMode.BLEND_MASKED)
        mat.set_editor_property('two_sided',True)
        graphic='/Textures/T_FX_SoftCircle' if kind=='Particle' else '/UI/T_FE_Eye0'
        texture=expression(mat,unreal.MaterialExpressionTextureSampleParameter2D,parameter_name='Graphic',texture=unreal.load_asset(DEST+graphic))
        # Face atlas rows are selected in the material instance; index is independently replicated for eye/mouth.
        uv=expression(mat,unreal.MaterialExpressionTextureCoordinate)
        scale=parameter(mat,'UVScale',(1,1,0,0));offset=parameter(mat,'UVOffset',(0,0,0,0))
        sc=expression(mat,unreal.MaterialExpressionComponentMask,r=True,g=True);oc=expression(mat,unreal.MaterialExpressionComponentMask,r=True,g=True)
        connect(scale,sc,'');connect(offset,oc,'')
        multiply=expression(mat,unreal.MaterialExpressionMultiply);connect(uv,multiply,'A');connect(sc,multiply,'B')
        add=expression(mat,unreal.MaterialExpressionAdd);connect(multiply,add,'A');connect(oc,add,'B');connect(add,texture,'UVs')
        opacity=scalar(mat,'Opacity',.12 if kind=='Particle' else 1)
        alpha=expression(mat,unreal.MaterialExpressionMultiply);connect(texture,alpha,'A','A');connect(opacity,alpha,'B')
        connect_prop(alpha,unreal.MaterialProperty.MP_OPACITY_MASK if kind=='Decal' else unreal.MaterialProperty.MP_OPACITY)
    if kind=='Glass':
        mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
        opacity=scalar(mat,'Opacity',.62);connect_prop(opacity,unreal.MaterialProperty.MP_OPACITY)
    if kind=='Sky':
        mat.set_editor_property('two_sided',True)
        mat.set_editor_property('is_sky',True)
        mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
        texture=expression(mat,unreal.MaterialExpressionTextureSampleParameter2D,parameter_name='Sky',texture=unreal.load_asset(DEST+'/Textures/T_FE_SkyGradient'))
        energy=scalar(mat,'Emission',50)
        multiply=expression(mat,unreal.MaterialExpressionMultiply);connect(texture,multiply,'A','RGB');connect(energy,multiply,'B')
        connect_prop(multiply,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(mat);save(mat);REPORT['materials'].append(path)

PALETTE={
 'Helmet':('Suit',(.81,.79,.69,1),.08,.42), 'Upper':('Suit',(.93,.235,.045,1),.05,.53),
 'Lower':('Suit',(.81,.79,.69,1),.05,.55),'Seal':('Rubber',(.025,.037,.044,1),.02,.78),
 'Metal':('PaintedMetal',(.16,.22,.26,1),.65,.36),'Visor':('Visor',(.006,.018,.026,1),.70,.18),
 'Glow':('Light',(.14,.77,.83,1),.05,.3),'Eye':('Decal',(.94,.70,.25,1),0,.4),
 'Mouth':('Decal',(.94,.70,.25,1),0,.4),'Concrete':('Concrete',(.8,.85,.9,1),0,.9),
 'Marking':('PaintedMetal',(.95,.48,.09,1),.05,.6),'Glass':('Glass',(.025,.12,.17,1),.65,.2),
}
instances={}
for name,(parent,color,metal,rough) in PALETTE.items():
    path=DEST+'/Materials/MI_FE_'+name
    instance=unreal.load_asset(path) or TOOLS.create_asset('MI_FE_'+name,DEST+'/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance,unreal.load_asset(DEST+'/Materials/M_FE_'+parent))
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance,'Tint',unreal.LinearColor(*color))
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(instance,'Metallic',metal)
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(instance,'Roughness',rough)
    if name in ['Eye','Mouth']:
        index=0 if name=='Eye' else 1
        cell=index if name=='Eye' else 5+index
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(instance,'Graphic',unreal.load_asset(DEST+'/Textures/T_FE_FaceAtlas'))
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance,'UVScale',unreal.LinearColor(448/2048,448/2048,0,0))
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance,'UVOffset',unreal.LinearColor((cell%4)*.25+32/2048,(cell//4)*.25+32/2048,0,0))
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(instance,'Emission',3)
    instances['FE_'+name]=instance;save(instance)

skeleton=unreal.load_asset(DEST+'/Characters/SK_FE_Engineer_Skeleton')
for path in sorted((SOURCE/'Characters').glob('SK_*.fbx')):
    if '_LOD' in path.stem:continue
    if path.relative_to(SOURCE).as_posix() not in SOURCE_FILES:continue
    options=unreal.FbxImportUI();options.set_editor_property('import_mesh',True)
    options.set_editor_property('import_as_skeletal',True);options.set_editor_property('skeleton',skeleton)
    options.set_editor_property('create_physics_asset',False)
    options.set_editor_property('import_animations',False);options.set_editor_property('import_materials',False);options.set_editor_property('import_textures',False)
    options.set_editor_property('automated_import_should_detect_type',False)
    options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose',False)
    options.skeletal_mesh_import_data.set_editor_property('use_t0_as_ref_pose',False)
    options.skeletal_mesh_import_data.set_editor_property('import_morph_targets',False)
    mesh=import_file(path,'Characters',options,unreal.FbxFactory())
    if not skeleton:
        skeleton=mesh.get_editor_property('skeleton')
        skeleton.add_compatible_skeleton(unreal.load_asset('/Game/ThirdParty/Player/UltimateModularMen/Casual_2/Mesh/Casual_2_Skeleton'))
        save(skeleton)
    materials=list(mesh.get_editor_property('materials'))
    for slot in materials:
        key=str(slot.get_editor_property('imported_material_slot_name'))
        if key in instances:slot.set_editor_property('material_interface',instances[key])
    mesh.set_editor_property('materials',materials)
    subsystem=unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    if subsystem.get_lod_count(mesh)<3:
        for lod in [1,2]:
            assert subsystem.import_lod(mesh,lod,str(path.with_stem(path.stem+'_LOD'+str(lod))))
    save(mesh)
for path in sorted((SOURCE/'Models').glob('*.fbx')):
    if path.relative_to(SOURCE).as_posix() not in SOURCE_FILES:continue
    options=unreal.FbxImportUI();options.set_editor_property('import_as_skeletal',False)
    options.set_editor_property('import_materials',False);options.set_editor_property('import_textures',False)
    options.set_editor_property('automated_import_should_detect_type',False)
    options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.static_mesh_import_data.set_editor_property('combine_meshes',True)
    mesh=import_file(path,'Models',options,unreal.FbxFactory())
    for i,slot in enumerate(mesh.get_editor_property('static_materials')):
        key=str(slot.get_editor_property('imported_material_slot_name'))
        if key in instances:mesh.set_material(i,instances[key])
    save(mesh)
for path in sorted((SOURCE/'Animations').glob('*.fbx')):
    if path.relative_to(SOURCE).as_posix() not in SOURCE_FILES:continue
    options=unreal.FbxImportUI();options.set_editor_property('import_mesh',False)
    options.set_editor_property('import_as_skeletal',True);options.set_editor_property('skeleton',skeleton)
    options.set_editor_property('import_animations',True)
    options.set_editor_property('automated_import_should_detect_type',False)
    options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_ANIMATION)
    options.anim_sequence_import_data.set_editor_property('import_uniform_scale',100.0)
    animation=import_file(path,'Animations',options,unreal.FbxFactory())
    if path.stem=='A_FE_Idle':
        import runpy
        runpy.run_path(str(ROOT/'Tools/Art/frontend_animation_basis_unreal.py'))['adapt_relaxed_idle'](animation)
for path in sorted((SOURCE/'Audio').glob('*.wav')):
    if path.relative_to(SOURCE).as_posix() not in SOURCE_FILES:continue
    sound=import_file(path,'Audio')
    if 'Music' in path.stem or any(w in path.stem for w in ['Wind','Hangar','ShipCooling']):sound.set_editor_property('looping',True)
    save(sound)
for path in sorted((SOURCE/'Fonts').glob('*.otf')):
    factory=unreal.FontFileImportFactory()
    factory.set_editor_property('batch_create_font_asset',unreal.BatchCreateFontAsset.YES)
    import_file(path,'Fonts',factory=factory,name='F_JTS_UI_'+path.stem.rsplit('-',1)[-1])
# The original presentation materials and lobby pawn are authored after source import.
import runpy
REPORT['presentation_polish']=runpy.run_path(str(ROOT/'Tools/Art/frontend_presentation_polish_unreal.py'),
    init_globals={'AUTHORING_JOB':{'operation':'import'}})['REPORT']
(REPORT_DIR/'unreal_import_report.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
