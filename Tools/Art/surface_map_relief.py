"""Author the configurable UI relief material, without modifying planet materials."""
import unreal

FOLDER='/Game/Space/UI/Navigation'
LIB=unreal.MaterialEditingLibrary


def configure():
    unreal.EditorAssetLibrary.make_directory(FOLDER)
    assets=unreal.AssetToolsHelpers.get_asset_tools()
    target=unreal.load_asset(FOLDER+'/RT_SurfaceMapPlaceholder')
    if not target:
        target=assets.create_asset('RT_SurfaceMapPlaceholder',FOLDER,unreal.TextureRenderTarget2D,unreal.TextureRenderTargetFactoryNew())
    target.set_editor_property('render_target_format',unreal.TextureRenderTargetFormat.RTF_RGBA8)
    unreal.EditorAssetLibrary.save_loaded_asset(target)
    mat=unreal.load_asset(FOLDER+'/M_SurfaceMapRelief')
    if not mat:mat=assets.create_asset('M_SurfaceMapRelief',FOLDER,unreal.Material,unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(mat)
    mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
    custom=LIB.create_material_expression(mat,unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property('code',r'''
float3 albedo=Texture2DSample(Albedo,AlbedoSampler,UV).rgb;
float2 offsets[5]={float2(0,0),float2(-Texel,0),float2(Texel,0),float2(0,-Texel),float2(0,Texel)};
float heights[5];
float centerDepth=0;
for(int i=0;i<5;i++) {
    float2 p=saturate(UV+offsets[i]);
    float depth=Texture2DSample(Depth,DepthSampler,p).r;
    if(i==0) centerDepth=depth;
    float3 relative=CaptureOrigin+MapRight*((p.x-.5)*MapSpan)+MapForward*((.5-p.y)*MapSpan)-RadialUp*depth;
    heights[i]=length(relative)-ReferenceRadius;
}
if(centerDepth<1 || centerDepth>10000000) return albedo;
// Radial elevation removes the planet's spherical curvature before shading.
float step=max(Texel*MapSpan,1);
float2 gradient=float2(heights[2]-heights[1],heights[3]-heights[4])/(2*step);
float3 normal=normalize(float3(-gradient*ReliefContrast,1));
float shade=.52+.58*saturate(dot(normal,normalize(float3(-.45,.65,.9))));
float band=floor(saturate(.5+heights[0]/HeightRange)*7)/7;
float3 low=float3(.075,.14,.19);
float3 middle=float3(.25,.32,.35);
float3 high=float3(.59,.57,.48);
float3 terrain=band<.5 ? lerp(low,middle,band*2) : lerp(middle,high,(band-.5)*2);
float contourCoord=heights[0]/max(ContourSpacing,1);
float aa=max(fwidth(contourCoord),.015);
float contour=1-smoothstep(aa,aa*2,abs(frac(contourCoord+.5)-.5));
float luminance=dot(albedo,float3(.2126,.7152,.0722));
float3 color=terrain*shade*lerp(.72,1.16,saturate(luminance));
// Preserve dark real attachments and surface texture above the terrain tint.
color=lerp(color,albedo,.18)*lerp(.38,1,smoothstep(.015,.20,luminance));
return color*(1-contour*ContourOpacity);
''')
    connections=[]
    uv=LIB.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate);connections.append(('UV',uv))
    for name in ['Albedo','Depth']:
        node=LIB.create_material_expression(mat,unreal.MaterialExpressionTextureObjectParameter)
        node.set_editor_property('parameter_name',name);node.set_editor_property('texture',target)
        node.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        connections.append((name,node))
    for name,value in dict(MapSpan=12000,Texel=1/512,ReferenceRadius=12500,HeightRange=1600,
                           ContourSpacing=150,ContourOpacity=.22,ReliefContrast=2.2).items():
        node=LIB.create_material_expression(mat,unreal.MaterialExpressionScalarParameter)
        node.set_editor_property('parameter_name',name);node.set_editor_property('default_value',value)
        connections.append((name,node))
    for name,value in dict(CaptureOrigin=(0,0,32500),RadialUp=(0,0,1),MapRight=(0,1,0),MapForward=(1,0,0)).items():
        node=LIB.create_material_expression(mat,unreal.MaterialExpressionVectorParameter)
        node.set_editor_property('parameter_name',name);node.set_editor_property('default_value',unreal.LinearColor(*value,0))
        connections.append((name,node))
    inputs=[]
    for name,node in connections:
        item=unreal.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
    custom.set_editor_property('inputs',inputs)
    for name,node in connections:assert LIB.connect_material_expressions(node,'',custom,name)
    assert LIB.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return dict(material=mat.get_path_name())
