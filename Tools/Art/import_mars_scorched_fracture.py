"""Import the source-fracture animation and author its complete blackout preview.

Run in an isolated UnrealEditor-Cmd process with -run=pythonscript. Assets and
level-instance configuration live under ScorchedFractureV3; SpaceWorld is untouched.
"""
import json
import sys
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
ART = ROOT / 'SourceArt/MarsDestruction/ScorchedFractureV3'
DATA = json.loads((ART / 'manifest.json').read_text())
FOLDER = '/Game/Space/Environment/MarsDestructionPreview/ScorchedFractureV3'
EA, AT, MEL = unreal.EditorAssetLibrary, unreal.AssetToolsHelpers.get_asset_tools(), unreal.MaterialEditingLibrary


def save(asset):
    assert EA.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def asset(name, cls, factory):
    result = unreal.load_asset(FOLDER + '/' + name) if EA.does_asset_exist(FOLDER + '/' + name) else AT.create_asset(name, FOLDER, cls, factory)
    assert result, name
    return result


def import_texture(filename, name):
    task = unreal.AssetImportTask()
    task.filename = str(ART / filename)
    task.destination_path, task.destination_name = FOLDER, name
    task.automated = task.replace_existing = task.save = True
    AT.import_asset_tasks([task])
    result = unreal.load_asset(FOLDER + '/' + name)
    assert result
    return result


def import_fbx(filename, name, skeletal):
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.import_as_skeletal = skeletal
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH if skeletal else unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_mesh = True
    options.import_materials = options.import_textures = False
    options.import_animations = skeletal
    options.create_physics_asset = False
    mesh_options = options.skeletal_mesh_import_data if skeletal else options.static_mesh_import_data
    mesh_options.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    if skeletal:
        mesh_options.set_editor_property('use_t0_as_ref_pose', False)
        mesh_options.set_editor_property('import_morph_targets', True)
    else:
        mesh_options.combine_meshes = True
        mesh_options.auto_generate_collision = False
    task = unreal.AssetImportTask()
    task.filename = str(ART / filename)
    task.destination_path, task.destination_name = FOLDER, name
    task.automated = task.replace_existing = task.replace_existing_settings = task.save = True
    task.options = options
    AT.import_asset_tasks([task])
    return [unreal.load_asset(p) for p in task.imported_object_paths]


def material(name, texture, mpc, interior=False, cooled=False, flow_texture=None):
    mat = asset(name, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property('used_with_skeletal_mesh', not cooled)
    mat.set_editor_property('used_with_morph_targets', not cooled)

    def node(cls, x=0, y=0):
        return MEL.create_material_expression(mat, cls, x, y)

    def wire(src, dest, pin, output=''):
        names = MEL.get_material_expression_input_names(dest)
        target = pin if pin in names else names[0] if pin in ('Input', 'Position') else pin
        assert MEL.connect_material_expressions(src, output, dest, target), (src, dest, pin, names)

    if texture:
        base = node(unreal.MaterialExpressionTextureSample, -800, -200)
        base.texture = texture
        base_output = 'RGB'
    else:
        base = node(unreal.MaterialExpressionConstant3Vector, -800, -200)
        base.constant = unreal.LinearColor(.07, .026, .014, 1)
        base_output = ''
    uv = node(unreal.MaterialExpressionTextureCoordinate, -800, 60)
    uv.set_editor_property('coordinate_index', 1)
    if cooled:
        world = node(unreal.MaterialExpressionWorldPosition, -1000, 300)
        rest = node(unreal.MaterialExpressionTransformPosition, -780, 300)
        rest.set_editor_property('transform_source_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
        rest.set_editor_property('transform_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
        wire(world, rest, 'Position')
        progress = node(unreal.MaterialExpressionConstant, -650, 500)
        progress.set_editor_property('r', 1.2)
    else:
        pre = node(unreal.MaterialExpressionPreSkinnedPosition, -1000, 300)
        rest = node(unreal.MaterialExpressionVertexInterpolator, -780, 300)
        wire(pre, rest, 'Input')
        progress = node(unreal.MaterialExpressionCollectionParameter, -650, 500)
        progress.set_editor_property('collection', mpc)
        progress.set_editor_property('parameter_name', 'BurnProgress')
    custom = node(unreal.MaterialExpressionCustom, -180, 0)
    custom.set_editor_property('description', 'Solid crust: advancing heat, char, fading rim emission; local erosion is an animation morph.')
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    thermal_output=unreal.CustomOutput()
    thermal_output.set_editor_property('output_name','ThermalEmission')
    thermal_output.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property('additional_outputs',[thermal_output])
    inputs = []
    for name in ('Albedo', 'Field', 'Rest', 'Burn', 'Noise', 'Pressure', 'Edge', 'Flow', 'Rift'):
        item = unreal.CustomInput()
        item.set_editor_property('input_name', name)
        inputs.append(item)
    custom.set_editor_property('inputs', inputs)
    custom.set_editor_property('code', f'''
float3 p = Rest * 0.01;
float n = Noise;
float veins = pow(saturate(1.0 - abs(n-0.5)*9.0), 2.0);
float3 direction=normalize(p+float3(0,0,.000001));
float2 flowUV=float2(atan2(direction.y,direction.x)/6.28318530718+.5,
                    1.-acos(clamp(direction.z,-1.,1.))/3.14159265359);
float3 network=Texture2DSample(Flow,FlowSampler,flowUV).rgb;
float arrival={"Field.x" if cooled else ".03+.21*length(p)+.40*network.r+.25*saturate(max(0.,Field.y)/.18)+.06*network.g"};
float age = Burn - arrival;
float burned = saturate(age/.18);
float band = pow(saturate(1.0-abs(age-.025)*28.0), 2.0);
float onset = Pressure;
float crack = saturate(1.0 - max(0.0,Field.y)*170.0)*Edge;
float3 charred = Albedo*{0.45 if cooled else 0.12} + float3({'.045,.026,.020' if cooled else '.012,.005,.003'})*(0.6+0.8*n);
float3 base = lerp(Albedo, charred, burned);
base = lerp(base, charred*0.6, crack*{0.0 if cooled else 0.8});
float heat = band*{0.0 if cooled else 2.5 if interior else 3.8}
           + burned*({0.025 if cooled else 0.06}+veins*{0.08 if cooled else 1.0 if interior else 2.3})
           + crack*{0.0 if cooled or interior else 10.0}
           + Pressure*{1.2 if interior else 0.0}*pow(saturate(1-length(p)/1.35),2);
float front=pow(saturate(1-abs(age-.035)*10.),2.);
float finger=saturate(1-network.r/.45);
float flicker=.78+.22*sin(length(p)*32.-Burn*65.+network.g*13.);
float live=front*(.5+.5*finger);
float hot=pow(live,3.)*finger*flicker;
float residual=burned*pow(saturate(1-network.b/.50),1.2)*saturate((network.g-.55)*4.);
float groove=Rift*pow(saturate(1-max(0.,Field.y)/(.004+.024*Rift)),.6)*{0. if interior or cooled else 1.};
float root=pow(saturate(1-max(0.,Field.y)/.006),1.5)*Rift*saturate(Pressure*.3)*{0. if interior or cooled else 1.};
base*=1-groove*.85;
ThermalEmission={"float3(1,.12,.006)*heat" if cooled else "float3(1,.68,.25)*hot*5.+float3(1,.14,.008)*live*1.8+float3(1,.075,.006)*residual*2.+float3(1,.025,.001)*burned*.035*(.4+.6*network.g)+float3(1,.14,.008)*root*.7"};
ThermalEmission+=Pressure*{2.5 if interior and not cooled else 0.}*pow(saturate((1.02-length(p))/.32),.7)*float3(1,.42,.10);
return float4(base, heat);
''')
    wire(base, custom, 'Albedo', base_output)
    wire(uv, custom, 'Field')
    wire(rest, custom, 'Rest')
    wire(progress, custom, 'Burn')
    noise = node(unreal.MaterialExpressionNoise, -480, 700)
    noise.set_editor_property('scale', .04)
    noise.set_editor_property('levels', 2)
    noise.set_editor_property('quality', 1)
    noise.set_editor_property('output_min', 0.)
    noise.set_editor_property('output_max', 1.)
    noise.set_editor_property('turbulence', False)
    wire(rest, noise, 'Position')
    wire(noise, custom, 'Noise')
    flow_object=node(unreal.MaterialExpressionTextureObject,-700,1200)
    flow_object.set_editor_property('texture',flow_texture)
    wire(flow_object,custom,'Flow')
    pressure = node(unreal.MaterialExpressionCollectionParameter, -650, 900)
    pressure.set_editor_property('collection', mpc)
    pressure.set_editor_property('parameter_name', 'InternalPressure')
    wire(pressure, custom, 'Pressure')
    edge = node(unreal.MaterialExpressionCollectionParameter, -650, 1050)
    edge.set_editor_property('collection', mpc)
    edge.set_editor_property('parameter_name', 'EdgeGlow')
    wire(edge, custom, 'Edge')
    rift_parameter=node(unreal.MaterialExpressionCollectionParameter,-650,1400)
    rift_parameter.set_editor_property('collection',mpc)
    rift_parameter.set_editor_property('parameter_name','RiftProgress')
    wire(rift_parameter,custom,'Rift')
    rgb = node(unreal.MaterialExpressionComponentMask, 120, -100)
    for channel in ('r', 'g', 'b'):
        rgb.set_editor_property(channel, True)
    rgb.set_editor_property('a', False)
    wire(custom, rgb, 'Input')
    assert MEL.connect_material_property(rgb, '', unreal.MaterialProperty.MP_BASE_COLOR)
    assert MEL.connect_material_property(custom, 'ThermalEmission', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = node(unreal.MaterialExpressionConstant, 350, 420)
    rough.set_editor_property('r', .9 if cooled else .82)
    assert MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(mat)
    save(mat)
    return mat


def volume_energy_material(name,mpc):
    """Depth-clipped emission integration inside a fixed invisible bounds cube.

    Additive radiance has no surface opacity or depth write. The per-pixel
    integration stops at the first opaque rock, so foreground plates remain
    solid while the spaces between them hold soft light rather than a sphere.
    """
    mat=asset(name,unreal.Material,unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided',False)
    def node(cls):return MEL.create_material_expression(mat,cls)
    custom=node(unreal.MaterialExpressionCustom)
    custom.set_editor_property('description','Fixed soft volume radiance, clipped against opaque scene depth; no shrinking surface.')
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs=[]
    for name in ('Entry','Camera','OpaqueDepth','EntryDepth','Pressure'):
        item=unreal.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
    custom.set_editor_property('inputs',inputs)
    custom.set_editor_property('code','''
if (Pressure<.0001) return float3(0,0,0);
float3 entry=Entry*.01;
float3 eye=Camera*.01;
float3 direction=normalize(entry-eye);
float3 safeRay=direction+float3(.000001,.000001,.000001);
float3 exitDistances=(sign(safeRay)*1.12-entry)/safeRay;
float farDistance=max(0.,min(exitDistances.x,min(exitDistances.y,exitDistances.z)));
float opaqueDistance=max(0.,(OpaqueDepth/max(EntryDepth,.01)-1.)*length(entry-eye));
float lengthToMarch=min(farDistance,opaqueDistance);
float stepLength=lengthToMarch/40.;
float integral=0.;
[loop] for (int i=0;i<40;++i) {
    float3 p=entry+direction*((i+.5)*stepLength);
    float envelope=pow(saturate((.94-length(p))/.22),2.);
    float cloud=.72+.15*sin(p.x*11.+sin(p.y*7.))*sin(p.z*9.+p.y*5.);
    integral+=envelope*cloud*stepLength;
}
return float3(1.,.40,.10)*integral*Pressure*(1.+Pressure)*32.;
''')
    world=node(unreal.MaterialExpressionWorldPosition)
    camera=node(unreal.MaterialExpressionCameraPositionWS)
    for source,pin in ((world,'Entry'),(camera,'Camera')):
        transform=node(unreal.MaterialExpressionTransformPosition)
        transform.set_editor_property('transform_source_type',unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
        transform.set_editor_property('transform_type',unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
        pin_names=MEL.get_material_expression_input_names(transform)
        assert MEL.connect_material_expressions(source,'',transform,pin_names[0])
        assert MEL.connect_material_expressions(transform,'',custom,pin)
    for cls,pin in ((unreal.MaterialExpressionSceneDepth,'OpaqueDepth'),(unreal.MaterialExpressionPixelDepth,'EntryDepth')):
        assert MEL.connect_material_expressions(node(cls),'',custom,pin)
    pressure=node(unreal.MaterialExpressionCollectionParameter)
    pressure.set_editor_property('collection',mpc)
    pressure.set_editor_property('parameter_name','InternalPressure')
    assert MEL.connect_material_expressions(pressure,'',custom,'Pressure')
    assert MEL.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity=node(unreal.MaterialExpressionConstant);opacity.set_editor_property('r',1.)
    assert MEL.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(mat);save(mat)
    return mat


def energy_material(name, mpc, rays=False):
    if not rays:
        return volume_energy_material(name,mpc)
    mat = asset(name, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE if rays else unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', rays)
    mat.set_editor_property('used_with_skeletal_mesh', rays)
    uv = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate)
    value = MEL.create_material_expression(mat, unreal.MaterialExpressionCollectionParameter)
    value.set_editor_property('collection', mpc)
    value.set_editor_property('parameter_name', 'RayStrength' if rays else 'InternalPressure')
    custom = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs=[]
    for name in ('UV','Amount','Reach'):
        item=unreal.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
    custom.set_editor_property('inputs',inputs)
    custom.set_editor_property('code',
        'float s=1-UV.y; float a=saturate(UV.x*30)*saturate((1-UV.x)*30)*pow(saturate(1-s/max(Reach,.001)),2)*Amount*.045; return float4(float3(1,.36,.065)*6,a);'
        if rays else 'return float4(float3(1,.40,.10)*Amount*16,1);')
    assert MEL.connect_material_expressions(uv,'',custom,'UV')
    assert MEL.connect_material_expressions(value,'',custom,'Amount')
    reach = MEL.create_material_expression(mat, unreal.MaterialExpressionCollectionParameter)
    reach.set_editor_property('collection', mpc)
    reach.set_editor_property('parameter_name', 'RayReach')
    assert MEL.connect_material_expressions(reach,'',custom,'Reach')
    for alpha, prop in ((False,unreal.MaterialProperty.MP_EMISSIVE_COLOR),(True,unreal.MaterialProperty.MP_OPACITY)):
        if alpha and not rays:continue
        mask=MEL.create_material_expression(mat,unreal.MaterialExpressionComponentMask)
        for channel in ('r','g','b'):mask.set_editor_property(channel,not alpha)
        mask.set_editor_property('a',alpha)
        assert MEL.connect_material_expressions(custom,'',mask,MEL.get_material_expression_input_names(mask)[0])
        assert MEL.connect_material_property(mask,'',prop)
    MEL.recompile_material(mat);save(mat)
    return mat


def keys(section, values):
    channel = section.get_all_channels()[0]
    for frame, value in values:
        if isinstance(value, bool):
            channel.add_key(unreal.FrameNumber(frame), value)
        else:
            channel.add_key(unreal.FrameNumber(frame), value, interpolation=unreal.MovieSceneKeyInterpolation.LINEAR)


def range_section(section, end=1200):
    section.set_range(0, end)
    section.set_completion_mode(unreal.MovieSceneCompletionMode.KEEP_STATE)
    return section


def blackout_material(mpc):
    mat = asset('M_MarsCinematicBlackout', unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
    scene = MEL.create_material_expression(mat, unreal.MaterialExpressionSceneTexture)
    scene.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    value = MEL.create_material_expression(mat, unreal.MaterialExpressionCollectionParameter)
    value.set_editor_property('collection', mpc)
    value.set_editor_property('parameter_name', 'Blackout')
    inverse = MEL.create_material_expression(mat, unreal.MaterialExpressionOneMinus)
    assert MEL.connect_material_expressions(value, '', inverse, MEL.get_material_expression_input_names(inverse)[0])
    multiply = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply)
    assert MEL.connect_material_expressions(scene, 'Color', multiply, 'A')
    assert MEL.connect_material_expressions(inverse, '', multiply, 'B')
    assert MEL.connect_material_property(multiply, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    save(mat)
    return mat


def central_scattering_material(mpc):
    """Camera-space shaft integration from visible internal plasma pixels.

    Opaque scene depth masks the source before integration. A second depth test
    prevents shafts painting over nearer rock. The source projection belongs to
    this fixed, centered cinematic camera, never to individual plate edges.
    """
    mat = asset('M_MarsCentralScattering', unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
    mat.set_editor_property('blendable_location', unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_BLOOM)
    scene = MEL.create_material_expression(mat, unreal.MaterialExpressionSceneTexture)
    scene.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    depth = MEL.create_material_expression(mat, unreal.MaterialExpressionSceneTexture)
    depth.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_SCENE_DEPTH)
    pressure = MEL.create_material_expression(mat, unreal.MaterialExpressionCollectionParameter)
    pressure.set_editor_property('collection', mpc)
    pressure.set_editor_property('parameter_name', 'InternalPressure')
    custom = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name in ('Scene','Depth','Pressure','Leakage'):
        item = unreal.CustomInput()
        item.set_editor_property('input_name', name)
        inputs.append(item)
    custom.set_editor_property('inputs', inputs)
    custom.set_editor_property('code', '''
if (Pressure < .001) return Scene.rgb;
float2 uv = GetDefaultSceneTextureUV(Parameters, 14);
float2 center = ViewportUVToSceneTextureUV(float2(.5,.5),14);
float2 radial=center-uv;
float radialDistance=length(radial*float2(1.7777778,1.));
float earlyReach=min(.78,.18/max(radialDistance,.001));
float2 stepUV=radial*earlyReach/256.;
float2 pixel=floor(uv*View.BufferSizeAndInvSize.xy);
float jitter=frac(52.9829189*frac(dot(pixel,float2(.06711056,.00583715))));
float2 q = uv+stepUV*jitter;
float3 shafts = 0;
[loop] for (int i=0; i<256; ++i) {
    q += stepUV;
    float3 c = SceneTextureLookup(Parameters,q,14,true).rgb;
    // Early fissures are already visibly luminous, but remain below the old
    // 4..8 HDR threshold. Select their warm internal light, excluding the ice
    // cap and ordinary red terrain, before widening the radial scattering.
    float green=c.g/max(c.r,.001), blue=c.b/max(c.r,.001);
    float fissureSource=smoothstep(.55,1.4,c.r)
        *smoothstep(.28,.37,green)*(1.-smoothstep(.62,.80,green))
        *smoothstep(.055,.09,blue)*(1.-smoothstep(.30,.48,blue));
    float source=fissureSource;
    float sourceDepth = SceneTextureLookup(Parameters,q,1,false).r;
    float unblocked = smoothstep(-15.,45.,Depth.r-sourceDepth);
    float travel=(i+.5)/256.;
    float falloff=exp(-2.*travel)*pow(1.-travel,2.);
    shafts += c*source*unblocked*falloff;
}
return Scene.rgb + shafts/256.*5.5*max(0.,Leakage)*saturate(Pressure);
''')
    for node, pin, output in ((scene,'Scene','Color'),(depth,'Depth','Color'),(pressure,'Pressure','')):
        assert MEL.connect_material_expressions(node, output, custom, pin)
    leakage=MEL.create_material_expression(mat,unreal.MaterialExpressionCollectionParameter)
    leakage.set_editor_property('collection',mpc)
    leakage.set_editor_property('parameter_name','CrackLeakage')
    assert MEL.connect_material_expressions(leakage,'',custom,'Leakage')
    assert MEL.connect_material_property(custom, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    save(mat)
    return mat


def main():
    EA.make_directory(FOLDER)
    surface = import_texture('T_MarsSurface_4K.png', 'T_MarsSurface_4K')
    atlas = import_texture('T_MarsDebris_Atlas.png', 'T_MarsDebris_Atlas')
    flow=import_texture(DATA['thermal_flow_texture'],'T_MarsThermalFlow')
    flow.set_editor_property('srgb',False)
    flow.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
    save(flow)
    mpc = asset('MPC_MarsScorch', unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    parameter = unreal.CollectionScalarParameter()
    parameter.set_editor_property('parameter_name', 'BurnProgress')
    parameter.set_editor_property('default_value', -.2)
    blackout_parameter = unreal.CollectionScalarParameter()
    blackout_parameter.set_editor_property('parameter_name', 'Blackout')
    blackout_parameter.set_editor_property('default_value', 0.)
    energy_parameters=[]
    for name in ('InternalPressure','RayStrength','RayReach','EdgeGlow','RiftProgress','CrackLeakage'):
        item=unreal.CollectionScalarParameter()
        item.set_editor_property('parameter_name',name)
        item.set_editor_property('default_value',0.)
        energy_parameters.append(item)
    mpc.set_editor_property('scalar_parameters', [parameter, blackout_parameter]+energy_parameters)
    save(mpc)
    blackout = blackout_material(mpc)
    scattering = central_scattering_material(mpc)
    # Keep the retired asset compilable when reopening an older editor session.
    # Nothing in the revised cinematic uses this plate-attached ray material.
    if EA.does_asset_exist(FOLDER+'/M_MarsInteriorRays'):
        energy_material('M_MarsInteriorRays',mpc,rays=True)
    mats = {'M_MarsScorchedSurface': material('M_MarsScorchedSurface', surface, mpc,flow_texture=flow),
            'M_MarsFractureInterior': material('M_MarsFractureInterior', None, mpc, interior=True,flow_texture=flow),
            'M_MarsDebris_Cooled': material('M_MarsDebris_Cooled', atlas, mpc, cooled=True,flow_texture=flow)}
    core_mat=energy_material('M_MarsInteriorCore',mpc)
    imported = import_fbx('SK_MarsScorchedFracture.fbx', 'SK_MarsScorchedFracture', True)
    mesh = next(a for a in imported if isinstance(a, unreal.SkeletalMesh))
    build = unreal.SkeletalMeshEditorSubsystem.get_lod_build_settings(mesh, 0)
    unreal.log('MARS_NORMAL_BUILD ' + str(build.recompute_normals))
    if build.recompute_normals:
        build.recompute_normals = False
        unreal.SkeletalMeshEditorSubsystem.set_lod_build_settings(mesh, 0, build)
    # Explicit animation import also makes re-running the authoring script reliable.
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.import_mesh = False
    options.import_animations = True
    options.import_materials = options.import_textures = False
    options.skeleton = mesh.get_editor_property('skeleton')
    # Import the target geometry only with the mesh. An animation-only morph
    # reimport uses the FBX factory's computed-normal path and can overwrite
    # source normals on the existing mesh; its curve is authored below instead.
    options.skeletal_mesh_import_data.set_editor_property('import_morph_targets', False)
    options.anim_sequence_import_data.set_editor_property('use_default_sample_rate', True)
    options.anim_sequence_import_data.set_editor_property('import_custom_attribute', True)
    options.anim_sequence_import_data.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    task = unreal.AssetImportTask()
    task.filename = str(ART / 'SK_MarsScorchedFracture.fbx')
    task.destination_path, task.destination_name = FOLDER, 'A_MarsScorchedFracture'
    task.automated = task.replace_existing = task.replace_existing_settings = task.save = True
    task.options = options
    AT.import_asset_tasks([task])
    animation = unreal.load_asset(FOLDER + '/A_MarsScorchedFracture')
    assert abs(animation.get_play_length() - DATA['animation_seconds']) < .05
    morphs = mesh.get_editor_property('morph_targets')
    assert {m.get_name() for m in morphs} == {'ScorchedRim','TectonicRift','RiftOpening'}, [m.get_name() for m in morphs]
    curve_names = unreal.AnimationLibrary.get_animation_curve_names(animation, unreal.RawCurveTrackTypes.RCT_FLOAT)
    # Explicit animation-only reimports can omit FBX blend-shape curves. Author
    # the same exported curve from the shared manifest on every import so this
    # operation is repeatable, including imports over an existing animation.
    skeleton = mesh.get_editor_property('skeleton')
    for morph_name,key_name in [('ScorchedRim','erosion_keys'),('TectonicRift','rift_keys'),('RiftOpening','opening_keys')]:
        if morph_name.casefold() in [str(n).casefold() for n in curve_names]:
            unreal.AnimationLibrary.remove_curve(animation,morph_name)
        unreal.AnimationLibrary.add_curve(animation,morph_name,unreal.RawCurveTrackTypes.RCT_FLOAT,False)
        unreal.AnimationLibrary.add_float_curve_keys(animation,morph_name,
            [f/30 for f,v in DATA[key_name]],[v for f,v in DATA[key_name]])
        if morph_name.casefold() not in [str(n).casefold() for n in unreal.AnimationLibrary.get_curve_meta_data_names(skeleton)]:
            unreal.AnimationLibrary.add_curve_meta_data(skeleton,morph_name)
        unreal.AnimationLibrary.set_curve_meta_data_morph_target(skeleton,morph_name,True)
        for frame,value in DATA[key_name]:
            assert abs(unreal.AnimationLibrary.get_float_value_at_time(animation,morph_name,frame/30)-value)<.001
    save(skeleton)
    # Slow cinematic drift is magnified 40x in this level. Default ACL/frame
    # stripping held the evaluated pose for many frames before jumping. Keep
    # every source frame and full-precision tracks on this asset only.
    compression=asset('ABS_MarsCinematic',unreal.AnimBoneCompressionSettings,
                      unreal.AnimBoneCompressionSettingsFactory())
    codec=unreal.new_object(unreal.load_class(None,'/Script/Engine.AnimCompress_LeastDestructive'),outer=compression)
    compression.set_editor_property('codecs',[codec])
    save(compression)
    animation.set_editor_property('allow_frame_stripping',False)
    animation.set_editor_property('bone_compression_settings',compression)
    animation.set_editor_property('compression_error_threshold_scale',40.)
    animation.set_editor_property('interpolation',unreal.AnimInterpolationType.LINEAR)
    save(animation)
    curve_names = unreal.AnimationLibrary.get_animation_curve_names(animation, unreal.RawCurveTrackTypes.RCT_FLOAT)
    assert morph_name.casefold() in [str(n).casefold() for n in curve_names], (morph_name, [str(n) for n in curve_names])
    assert not unreal.SkeletalMeshEditorSubsystem.get_lod_build_settings(mesh, 0).recompute_normals
    slots = mesh.get_editor_property('materials')
    assert len(slots) >= DATA['materials']
    slots = list(slots)[:DATA['materials']]
    for i, slot in enumerate(slots):
        slot.material_interface = mats[str(slot.material_slot_name)]
        slots[i] = slot
    mesh.set_editor_property('materials', slots)
    save(mesh)
    final = next(a for a in import_fbx('SM_MarsDebris_Exploration.fbx', 'SM_MarsDebris_Exploration', False)
                 if isinstance(a, unreal.StaticMesh))
    final.set_material(0, mats['M_MarsDebris_Cooled'])
    body = final.get_editor_property('body_setup')
    body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    save(final)
    level = FOLDER + '/L_MarsScorchedFracture_Preview'
    if EA.does_asset_exist(level):
        assert unreal.EditorLevelLibrary.load_level(level)
    else:
        assert unreal.EditorLevelLibrary.new_level(level)
    actor_system = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_system.get_all_level_actors()

    def placed(cls, label, location=unreal.Vector(), rotation=unreal.Rotator()):
        obj = next((a for a in actors if a.get_actor_label() == label), None)
        if not obj:
            obj = actor_system.spawn_actor_from_class(cls, location, rotation)
        obj.set_actor_label(label)
        obj.set_actor_location(location, False, False)
        obj.set_actor_rotation(rotation, False)
        return obj

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode', unreal.GameModeBase)
    # Reuse the game's existing space backdrop in this isolated cinematic map.
    stars=placed(unreal.StaticMeshActor,'Mars_CinematicStarfield')
    stars.set_actor_scale3d(unreal.Vector(12000,12000,12000))
    stars.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Sphere.Sphere'))
    stars.static_mesh_component.set_material(0,unreal.load_asset('/Game/Space/Environment/M_SolarSystemStarfield'))
    stars.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    stars.static_mesh_component.set_editor_property('cast_shadow',False)
    fracture = placed(unreal.SkeletalMeshActor, 'Mars_FirstModel_TrueFracture')
    fracture.set_actor_scale3d(unreal.Vector(40, 40, 40))
    component = fracture.skeletal_mesh_component
    component.set_skeletal_mesh_asset(mesh)
    for i, slot in enumerate(slots):
        component.set_material(i, slot.material_interface)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.override_animation_data(animation, False, False, 0, 1)
    component.set_editor_property('visibility_based_anim_tick_option', unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    component.set_editor_property('enable_update_rate_optimizations',False)
    component.set_editor_property('bounds_scale', 2.5)
    core_mesh=next(a for a in import_fbx('SM_MarsInteriorCore.fbx','SM_MarsInteriorCore',False) if isinstance(a,unreal.StaticMesh))
    core_mesh.set_material(0,core_mat);save(core_mesh)
    core=placed(unreal.StaticMeshActor,'Mars_InteriorEnergy')
    core.set_actor_scale3d(unreal.Vector(40,40,40))
    core.static_mesh_component.set_static_mesh(core_mesh)
    core.static_mesh_component.set_material(0,core_mat)
    core.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    core.static_mesh_component.set_cast_shadow(False)
    core.set_actor_enable_collision(False)
    # All illumination remains at the original planet center. Rock blocks the
    # light in both surface and volumetric shadow passes; no light cards follow
    # any plate. Local medium is confined to this cinematic's neighborhood.
    central = placed(unreal.PointLight, 'Mars_CentralPressureLight')
    central.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    central.light_component.set_light_color(unreal.LinearColor(1,.40,.10,1))
    central.light_component.set_use_inverse_squared_falloff(False)
    central.light_component.set_attenuation_radius(15000.)
    central.light_component.set_source_radius(8.)
    central.light_component.set_intensity(0.)
    central.light_component.set_cast_shadows(True)
    central.light_component.set_cast_volumetric_shadow(True)
    central.light_component.set_editor_property('volumetric_scattering_intensity', 1.8)
    central.light_component.set_editor_property('shadow_bias', .03)
    central.light_component.set_editor_property('shadow_resolution_scale', 2.)
    fog = placed(unreal.ExponentialHeightFog, 'Mars_VolumetricFogSettings')
    fog_component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fog_component.set_editor_property('fog_density', .000001)
    fog_component.set_editor_property('fog_cutoff_distance',100000.)
    fog_component.set_editor_property('fog_inscattering_luminance', unreal.LinearColor(0,0,0,1))
    fog_component.set_volumetric_fog(True)
    fog_component.set_volumetric_fog_distance(70000.)
    fog_component.set_volumetric_fog_scattering_distribution(.15)
    fog_component.set_volumetric_fog_emissive(unreal.LinearColor(0,0,0,1))
    medium = placed(unreal.LocalFogVolume, 'Mars_LocalScatteringMedium')
    medium.set_actor_scale3d(unreal.Vector(14.4,14.4,14.4))
    medium_component = medium.get_component_by_class(unreal.LocalFogVolumeComponent)
    medium_component.set_radial_fog_extinction(.35)
    medium_component.set_height_fog_extinction(0.)
    medium_component.set_fog_albedo(unreal.LinearColor(1,.82,.64,1))
    medium_component.set_fog_emissive(unreal.LinearColor(0,0,0,1))
    medium_component.set_fog_phase_g(.15)
    debris = placed(unreal.StaticMeshActor, 'Mars_SecondModel_Exploration')
    debris.set_actor_scale3d(unreal.Vector(40, 40, 40))
    debris.static_mesh_component.set_static_mesh(final)
    debris.static_mesh_component.set_material(0, mats['M_MarsDebris_Cooled'])
    debris.static_mesh_component.set_collision_profile_name('BlockAll')
    debris.set_actor_hidden_in_game(True)
    debris.set_actor_enable_collision(False)
    # Hide the exploration object in the editor as well, so the level initially
    # presents only one planet. Sequencer's visibility system reveals it later.
    debris.set_is_temporarily_hidden_in_editor(True)
    cine_loc = unreal.Vector(0, 40000, 10500)
    ship_loc = unreal.Vector(1400, 49600, 8400)
    cine = placed(unreal.CameraActor, 'Mars_CinematicCamera', cine_loc,
                  unreal.MathLibrary.find_look_at_rotation(cine_loc, unreal.Vector()))
    cine.camera_component.set_projection_mode(unreal.CameraProjectionMode.PERSPECTIVE)
    cine.camera_component.set_field_of_view(41.1)
    ship = placed(unreal.CameraActor, 'Mars_ShipView_Preview', ship_loc,
                  unreal.MathLibrary.find_look_at_rotation(ship_loc, unreal.Vector()))
    ship.camera_component.set_field_of_view(41.1)
    for camera in (cine, ship):
        camera.camera_component.set_aspect_ratio(16 / 9)
        settings = camera.camera_component.get_editor_property('post_process_settings')
        settings.set_editor_property('override_auto_exposure_min_brightness', True)
        settings.set_editor_property('override_auto_exposure_max_brightness', True)
        settings.set_editor_property('auto_exposure_min_brightness', 1.)
        settings.set_editor_property('auto_exposure_max_brightness', 1.)
        settings.set_editor_property('override_bloom_intensity', True)
        settings.set_editor_property('bloom_intensity', .8)
        camera.camera_component.set_editor_property('post_process_settings', settings)
        camera.camera_component.add_or_update_blendable(blackout, 1.)
        camera.camera_component.add_or_update_blendable(scattering, 1.)
    placed(unreal.PlayerStart, 'MarsPreview_PlayerStart', ship_loc, ship.get_actor_rotation())
    for label, rotation, intensity, color in [
        ('Key', unreal.Rotator(-40, -60, 0), 5., unreal.LinearColor(1, .83, .68, 1)),
        ('Fill', unreal.Rotator(-15, 140, 0), 1.8, unreal.LinearColor(.55, .68, 1, 1)),
        ('Rim', unreal.Rotator(-25,-140,0),1.4,unreal.LinearColor(.35,.48,1,1))]:
        light = placed(unreal.DirectionalLight, 'MarsPreview_' + label, rotation=rotation)
        light.light_component.set_intensity(intensity)
        light.light_component.set_light_color(color)
        light.light_component.set_editor_property('volumetric_scattering_intensity', 0.)
    seq = asset('LS_MarsScorchedDestruction', unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    for binding in seq.get_bindings():
        binding.remove()
    for track in seq.get_tracks():
        seq.remove_track(track)
    seq.set_display_rate(unreal.FrameRate(30, 1))
    seq.set_playback_start(0)
    seq.set_playback_end(1200)
    seq.set_view_range_start(0.)
    seq.set_view_range_end(40.)
    first_binding = seq.add_possessable(fracture)
    anim_track = first_binding.add_track(unreal.MovieSceneSkeletalAnimationTrack)
    anim_section = range_section(anim_track.add_section(), round(DATA['animation_seconds']*30))
    params = anim_section.get_editor_property('params')
    params.set_editor_property('animation', animation)
    anim_section.set_editor_property('params', params)

    def visible(binding, values):
        track = binding.add_track(unreal.MovieSceneVisibilityTrack)
        track.set_property_name_and_path('bHidden', 'bHidden')
        keys(range_section(track.add_section()), values)

    visible(first_binding, [(0, True), (1005, False)])
    core_binding=seq.add_possessable(core)
    visible(core_binding, [(0, True), (round(DATA['energy_core_hide_seconds']*30), False)])
    core_transform=core_binding.add_track(unreal.MovieScene3DTransformTrack)
    core_section=range_section(core_transform.add_section())
    channels=core_section.get_all_channels()
    for channel in channels:
        channel_name=str(channel.get_name())
        if channel_name.startswith('Scale.'):
            channel.set_default(40.)
            for frame,value in DATA['energy_core_scale_keys']:
                channel.add_key(unreal.FrameNumber(frame),40.*value,
                    interpolation=unreal.MovieSceneKeyInterpolation.LINEAR)
        elif channel_name.startswith(('Location.','Rotation.')):
            channel.set_default(0.)
    visible(seq.add_possessable(medium), [(0, True), (round(DATA['energy_core_hide_seconds']*30), False)])
    light_binding = seq.add_possessable(central.light_component)
    intensity_track = light_binding.add_track(unreal.MovieSceneFloatTrack)
    intensity_track.set_property_name_and_path('Intensity', 'Intensity')
    keys(range_section(intensity_track.add_section()),
         [(frame,value*DATA['central_light_intensity_scale']) for frame,value in DATA['internal_pressure_keys']])
    final_binding = seq.add_possessable(debris)
    visible(final_binding, [(0, False), (1005, True)])
    collision_track = final_binding.add_track(unreal.MovieSceneBoolTrack)
    collision_track.set_property_name_and_path('bActorEnableCollision', 'bActorEnableCollision')
    keys(range_section(collision_track.add_section()), [(0, False), (1005, True)])
    mpc_track = seq.add_track(unreal.MovieSceneMaterialParameterCollectionTrack)
    mpc_track.set_editor_property('mpc', mpc)
    mpc_section = range_section(mpc_track.add_section())
    # AddScalarParameterKey uses tick-resolution frames, unlike scripting channels.
    tick_rate = seq.get_tick_resolution()
    ticks_per_second = tick_rate.numerator / tick_rate.denominator
    for frame, value in DATA['burn_progress_keys']:
        mpc_section.add_scalar_parameter_key('BurnProgress', unreal.FrameNumber(round(frame / 30 * ticks_per_second)),
                                              value, unreal.MovieSceneKeyInterpolation.LINEAR)
    for frame, value in [(0, 0.), (960, 0.), (990, 1.), (1020, 1.), (1065, 0.), (1200, 0.)]:
        mpc_section.add_scalar_parameter_key('Blackout', unreal.FrameNumber(round(frame/30*ticks_per_second)),
                                              value, unreal.MovieSceneKeyInterpolation.LINEAR)
    for parameter_name, manifest_name in [('InternalPressure','internal_pressure_keys'),('RayStrength','ray_strength_keys'),
                                          ('RayReach','ray_reach_keys'),('EdgeGlow','edge_glow_keys'),('RiftProgress','rift_keys'),
                                          ('CrackLeakage','leakage_keys')]:
        for frame,value in DATA[manifest_name]:
            mpc_section.add_scalar_parameter_key(parameter_name,unreal.FrameNumber(round(frame/30*ticks_per_second)),
                                                value,unreal.MovieSceneKeyInterpolation.LINEAR)
    cuts = seq.add_track(unreal.MovieSceneCameraCutTrack)
    for camera, start, end in ((cine, 0, 1005), (ship, 1005, 1200)):
        binding = seq.add_possessable(camera)
        cut = cuts.add_section()
        cut.set_range(start, end)
        binding_id = unreal.MovieSceneObjectBindingID()
        binding_id.set_editor_property('guid', binding.get_id())
        cut.set_editor_property('camera_binding_id', binding_id)
    seq.delete_marked_frames()
    for frame, label in [(60, 'Gorges deepen in intact crust'), (150, 'Gorges tear while main solids hold position'),
                          (240, 'Main rupture and pressure release'), (420, 'Fire rolls up from within'), (960, 'Fade to black'),
                          (1005, 'Swap + collision behind black'), (1065, 'Ship view / original debris')]:
        mark = unreal.MovieSceneMarkedFrame()
        mark.set_editor_property('frame_number', unreal.FrameNumber(frame))
        mark.set_editor_property('label', label)
        seq.add_marked_frame(mark)
    save(seq)
    sequence_actor = placed(unreal.LevelSequenceActor, 'Mars_DestructionSequence')
    sequence_actor.set_sequence(seq)
    settings = sequence_actor.get_editor_property('playback_settings')
    settings.set_editor_property('auto_play', True)
    settings.set_editor_property('loop_count', unreal.MovieSceneSequenceLoopCount(value=0))
    settings.set_editor_property('finish_completion_state_override', unreal.MovieSceneCompletionModeOverride.FORCE_KEEP_STATE)
    settings.set_editor_property('disable_movement_input', True)
    settings.set_editor_property('disable_look_at_input', True)
    sequence_actor.set_editor_property('playback_settings', settings)
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(cine_loc, cine.get_actor_rotation())
    assert unreal.EditorLevelLibrary.save_current_level()
    report = dict(map=level, sequence=seq.get_path_name(), animation=animation.get_path_name(),
                  animation_seconds=animation.get_play_length(), sequence_seconds=40,
                  skeletal_materials=len(slots), blackout=[33, 34], switch=33.5,
                  rim_morph='ScorchedRim', rift_morph='TectonicRift', animation_curves=[str(n) for n in curve_names],
                  final_collision=str(body.get_editor_property('collision_trace_flag')),
                  final_original_fragments=107, autoplay=True, looping=False,
                  runtime_cpp_changed=False, scope='Reusable cinematic assets and independent playable preview; SpaceWorld unchanged.')
    (ROOT / 'Saved/MarsDestruction/unreal_scorched_import.json').write_text(json.dumps(report, indent=2))
    unreal.log('MARS_SCORCHED_IMPORT_OK ' + json.dumps(report))


if __name__ == '__main__':
    main()
