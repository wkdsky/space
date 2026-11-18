"""Screen outline for encounter teams. Stencil ranges are reserved by the director."""
import unreal as u

def encounter_outline():
    folder='/Game/Space/Environment/MarsSystem/Chapter/Materials'
    name='M_Mars_EncounterOutline'
    m=u.load_asset(folder+'/'+name)
    if m and u.EditorAssetLibrary.get_metadata_tag(m,'JTS.MarsOutline')=='1':return m
    if not m:m=u.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,u.Material,u.MaterialFactoryNew())
    me=u.MaterialEditingLibrary;me.delete_all_material_expressions(m)
    m.set_editor_property('material_domain',u.MaterialDomain.MD_POST_PROCESS)
    m.set_editor_property('blendable_location',u.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    nodes=[]
    for i,(name,kind) in enumerate([('Scene','PPI_POST_PROCESS_INPUT0'),('Stencil','PPI_CUSTOM_STENCIL'),('Depth','PPI_SCENE_DEPTH'),('CustomDepth','PPI_CUSTOM_DEPTH')]):
        n=me.create_material_expression(m,u.MaterialExpressionSceneTexture,-500,i*150);n.set_editor_property('scene_texture_id',getattr(u.SceneTextureId,kind));nodes.append((n,name))
    time=me.create_material_expression(m,u.MaterialExpressionTime,-500,650);nodes.append((time,'Time'))
    outline=me.create_material_expression(m,u.MaterialExpressionCustom,0,0)
    inputs=[]
    for _,name in nodes:
        v=u.CustomInput();v.set_editor_property('input_name',name);inputs.append(v)
    outline.set_editor_property('inputs',inputs);outline.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT3)
    outline.set_editor_property('code','''
float2 uv=GetDefaultSceneTextureUV(Parameters,PPI_CustomStencil);
float2 pixel=GetSceneTextureBufferSize(PPI_CustomStencil).zw;
float center=Stencil.r;
float isCenter=((center>=10&&center<30)||(center>=50&&center<70));
float edge=0;float value=0;
[unroll] for(int y=-1;y<=1;y++) [unroll] for(int x=-1;x<=1;x++) {
    float2 p=ClampSceneTextureUV(uv+float2(x,y)*pixel*2,PPI_CustomStencil);
    float s=SceneTextureLookup(p,PPI_CustomStencil,false).r;
    float valid=((s>=10&&s<30)||(s>=50&&s<70));
    float visible=SceneTextureLookup(p,PPI_CustomDepth,false).r<=SceneTextureLookup(p,PPI_SceneDepth,false).r+1;
    if(valid*visible>edge) {edge=valid*visible;value=s;}
}
float hostile=value>=50;
float base=value-hostile*40;
float3 tint=base>=20?float3(1,.65,.2):float3(.8,.85,.9);
float strength=hostile?(.65+.3*sin(Time*5)):.65;
return lerp(Scene.rgb,tint,edge*(1-isCenter)*strength);
''')
    for n,name in nodes:assert me.connect_material_expressions(n,'',outline,name)
    assert me.connect_material_property(outline,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    me.recompile_material(m);u.EditorAssetLibrary.set_metadata_tag(m,'JTS.MarsOutline','1');u.EditorAssetLibrary.save_loaded_asset(m)
    return m
