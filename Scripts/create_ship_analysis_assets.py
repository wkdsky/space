import unreal
from pathlib import Path
ROOT='/Game/Space/Ships/Analysis'
at=unreal.AssetToolsHelpers.get_asset_tools()
def save(obj):
    if isinstance(obj,unreal.Blueprint):unreal.BlueprintEditorLibrary.compile_blueprint(obj)
    unreal.EditorAssetLibrary.save_loaded_asset(obj,False)
def asset(name,cls,factory,folder=ROOT):
    p=folder+'/'+name
    return unreal.load_asset(p) if unreal.EditorAssetLibrary.does_asset_exist(p) else at.create_asset(name,folder,cls,factory)
def create_visuals():
    out={}
    lib=unreal.MaterialEditingLibrary
    for name in ['Projection','Collection']:
        m=asset('M_ShipScanner_'+name,unreal.Material,unreal.MaterialFactoryNew(),ROOT+'/Materials')
        lib.delete_all_material_expressions(m)
        m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE if name=='Projection' else unreal.BlendMode.BLEND_MASKED)
        m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
        m.set_editor_property('two_sided',True)
        def node(cls,x=0,y=0):return lib.create_material_expression(m,cls,x,y)
        if name=='Projection':
            color=node(unreal.MaterialExpressionVertexColor,-250,0)
            strength=node(unreal.MaterialExpressionConstant,-250,100);strength.r=2
            glow=node(unreal.MaterialExpressionMultiply,-50,0)
            assert lib.connect_material_expressions(color,'',glow,'A')
            assert lib.connect_material_expressions(strength,'',glow,'B')
            assert lib.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            assert lib.connect_material_property(color,'A',unreal.MaterialProperty.MP_OPACITY)
        else:
            pos=node(unreal.MaterialExpressionWorldPosition,-600,100)
            noise=node(unreal.MaterialExpressionNoise,-400,100)
            noise.set_editor_property('scale',.12);noise.set_editor_property('quality',1);noise.set_editor_property('levels',1)
            noise.set_editor_property('output_min',0);noise.set_editor_property('output_max',1)
            assert lib.connect_material_expressions(pos,'',noise,'')
            progress=node(unreal.MaterialExpressionScalarParameter,-400,260);progress.set_editor_property('parameter_name','Progress');progress.set_editor_property('default_value',0)
            mask=node(unreal.MaterialExpressionSubtract,-150,100)
            assert lib.connect_material_expressions(noise,'',mask,'A')
            assert lib.connect_material_expressions(progress,'',mask,'B')
            assert lib.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY_MASK)
            color=node(unreal.MaterialExpressionConstant3Vector,-200,-100);color.constant=unreal.LinearColor(.12,2.8,2.4,1)
            assert lib.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            m.set_editor_property('opacity_mask_clip_value',.015)
        lib.recompile_material(m);save(m);out[name]=m
    return out
def apply():
    visuals=create_visuals()
    factory=unreal.DataAssetFactory()
    cat=asset('DA_AnalysisCatalog',unreal.JTSAnalysisCatalog,factory)
    tag_info={
      '武器':'可作为武器使用，例如挥打、投掷或射击。',
      '素材':'可作为商店兑换物，换取其他物品。',
      '燃料来源':'物料处理的燃料输入物之一，需要加工后才能供能。',
      '食物来源':'物料处理的食物输入物之一，需要加工后才能食用。',
      '燃料':'可直接作为燃料使用。', '食物':'可直接作为食物使用。',
      '工具':'用于采集、照明或其他实用操作。', '补给':'用于补充远征中的日常消耗。'}
    book=unreal.load_asset('/Game/Space/Data/Ships/DA_MaterialProcessingRecipes')
    fuel_inputs=set();food_inputs=set()
    if book:
        for recipe in book.get_editor_property('recipes'):
            outputs={q.resource_type for q in recipe.outputs}
            if unreal.JTSResourceType.FUEL in outputs:fuel_inputs.update(q.resource_type for q in recipe.inputs)
            if unreal.JTSResourceType.FOOD in outputs:food_inputs.update(q.resource_type for q in recipe.inputs)
    entries=[]
    def entry(item_id,name,desc,tags,known=False,species='',mesh=None):
        value=unreal.JTSAnalysisEntry()
        for key,v in dict(item_id=item_id,specimen_id=species,name=name,description=desc,initially_known=known,sample_mesh=mesh).items():value.set_editor_property(key,v)
        items=[]
        for label in dict.fromkeys(tags):
            tag=unreal.JTSAnalysisTag();tag.set_editor_property('label',label);tag.set_editor_property('explanation',tag_info[label]);items.append(tag)
        value.set_editor_property('tags',items);entries.append(value)
    for suffix,name,description in [
        ('Rock','石块','质地坚硬，脾气稳定。兜里多了，步伐就少了。'),
        ('Ore','矿石','石头里的潜力股。兑现前请先敲开。'),
        ('Crystal','晶体','亮得很努力，暂时不负责照明。'),
        ('InorganicCarbon','无机碳','黑得很纯粹。加工一下，就能把沉默变成能量。')]:
        iid=getattr(unreal.JTSItemId,suffix.upper() if suffix!='InorganicCarbon' else 'INORGANIC_CARBON')
        rt=getattr(unreal.JTSResourceType,suffix.upper() if suffix!='InorganicCarbon' else 'INORGANIC_CARBON')
        tags=['素材']
        if suffix=='Rock':tags.insert(0,'武器')
        if rt in fuel_inputs:tags.append('燃料来源')
        if rt in food_inputs:tags.append('食物来源')
        entry(iid,name,description,tags)
    corpse_tags=[]
    if unreal.JTSResourceType.MOON_ANT_CORPSE in fuel_inputs:corpse_tags.append('燃料来源')
    if unreal.JTSResourceType.MOON_ANT_CORPSE in food_inputs:corpse_tags.append('食物来源')
    entry(unreal.JTSItemId.MOON_ANT_CORPSE,'月蚁','生前忙着搬家，身后忙着为科学做贡献。',corpse_tags)
    descriptions={
        'Riftlip':'嘴比胆子大。现在终于愿意安静接受检查了。',
        'MirrorSkimmer':'反光是它的本事，照镜子是你的选择。',
        'SpireWorker':'一生勤勤恳恳。请把这份敬业留给图鉴。',
        'SpireSoldier':'盔甲很硬，职业规划很单纯：守门。',
        'MirrorBoss':'生前气势很足。身后也得一块一块搬。'}
    for species,description in descriptions.items():
        bp=unreal.load_asset('/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_'+species)
        cdo=unreal.get_default_object(bp.generated_class());cdo.set_editor_property('specimen_id',species);save(bp)
        entry(unreal.JTSItemId.MOON_ANT_CORPSE,str(cdo.get_editor_property('display_name')),description,corpse_tags,False,species)
    for suffix,name,desc,tags in [('Fuel','燃料','飞船的饭。请别替它尝。',['燃料']),('Food','食物','味道不是重点，能继续出发才是。',['食物']),('Water','水','透明，平凡，离不开。',['补给'])]:
        entry(getattr(unreal.JTSItemId,suffix.upper()),name,desc,tags,True)
    # Existing human equipment is familiar. Its configuration stays in the original item definitions.
    for iid in unreal.JTSItemId:
        definition=unreal.JTSItemDefinitionLibrary.get_item_definition(None,iid)
        if not definition:continue
        if any(e.item_id==iid and str(e.specimen_id)=='None' for e in entries):continue
        if iid in [unreal.JTSItemId.NONE,unreal.JTSItemId.STELLAR_TEXT,unreal.JTSItemId.STELLAR_WEAPON]:continue
        tags=['工具'] if iid==unreal.JTSItemId.WAIST_LAMP else ['武器']
        entry(iid,str(definition.display_name),str(definition.description).split('\n')[0][:42],tags,True,mesh=definition.world_pickup_mesh)
    cat.set_editor_property('entries',entries)
    cat.set_editor_property('planet_names',{'Earth':'地球','Moon':'月球','Mars':'火星','Mercury':'水星','Venus':'金星','Europa':'木卫二','Titan':'土卫六'})
    save(cat)
    repair_corpse_assets()
    ships=[]
    for path in ['/Game/Space/Ships/TwinforkR1/BP_Twinfork_R1','/Game/Space/Blueprints/Ships/BP_Spacecraft']:
        bp=unreal.load_asset(path);c=unreal.get_default_object(bp.generated_class());scanner=c.scanner
        scanner.set_editor_property('analysis_catalog',cat)
        scanner.set_editor_property('projection_material',visuals['Projection'])
        scanner.set_editor_property('collection_material',visuals['Collection'])
        scanner.set_editor_property('offset_from_screen',unreal.Vector(0,230,0))
        scanner.set_editor_property('zone_radius',145)
        scanner.set_editor_property('zone_height',180)
        scanner.set_editor_property('small_sample_seconds',3)
        scanner.set_editor_property('large_sample_seconds',16)
        scanner.set_editor_property('large_sample_span',180)
        modules=c.dock_component
        modules.set_editor_property('catalog',[d for d in modules.catalog if str(d.module_id)!='Analysis'])
        modules.set_editor_property('initial_installed_ids',[i for i in modules.initial_installed_ids if str(i)!='Analysis'])
        save(bp);ships.append(path)
    for old in ['M_Analysis_ScanCurtain','M_Analysis_PlacementGhost']:
        p=ROOT+'/Materials/'+old
        if unreal.EditorAssetLibrary.does_asset_exist(p):assert unreal.EditorAssetLibrary.delete_asset(p),p
    return {'built_in_scanner':ships,'entries':len(cat.entries)}

def repair_corpse_assets():
    """Use original creature assets in every sample presentation and configure research prerequisites."""
    cat=unreal.load_asset(ROOT+'/DA_AnalysisCatalog')
    entries=list(cat.get_editor_property('entries'))
    repaired=[]
    for e in entries:
        species=str(e.specimen_id)
        if species not in ['Riftlip','MirrorSkimmer','SpireWorker','SpireSoldier','MirrorBoss']:continue
        bp=unreal.load_asset('/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_'+species)
        c=unreal.get_default_object(bp.generated_class())
        mesh=next(iter(c.get_components_by_class(unreal.SkeletalMeshComponent)))
        sk=mesh.get_editor_property('skeletal_mesh_asset')
        key=unreal.JTSBossAction.DEATH if species=='MirrorBoss' else unreal.JTSForagerState.DEAD
        e.set_editor_property('sample_mesh',None)
        e.set_editor_property('specimen_mesh',sk)
        e.set_editor_property('death_animation',c.get_editor_property('animations')[key])
        e.set_editor_property('specimen_scale',1 if species=='MirrorBoss' else c.get_editor_property('body_scale'))
        e.set_editor_property('separate_corpse_parts',species=='MirrorBoss')
        e.set_editor_property('max_corpse_part_span',80)
        if species=='MirrorBoss':e.set_editor_property('description','生前气势很足。身后也得一块一块搬。')
        # Retain source vertex/skin data in packaged builds for original-part presentation.
        unreal.SkeletalMeshEditorSubsystem.set_allow_cpu_access(sk,True)
        save(sk);repaired.append(species)
    cat.set_editor_property('entries',entries);save(cat)
    habitats=[]
    for p in unreal.EditorAssetLibrary.list_assets('/Game/Space/Blueprints/Planets/Moon/EcologyV3',True,False):
        bp=unreal.load_asset(p)
        if not isinstance(bp,unreal.Blueprint):continue
        c=unreal.get_default_object(bp.generated_class())
        if not isinstance(c,unreal.JTSPlanetEcologyHabitat):continue
        enemy=c.get_editor_property('enemy_class')
        if enemy:
            enemy_defaults=unreal.get_default_object(enemy)
            if isinstance(enemy_defaults,unreal.JTSPlanetSurfaceCreature) and str(enemy_defaults.get_editor_property('specimen_id'))=='MirrorSkimmer':
                entrance=c.get_editor_property('can_block_entrance') or c.get_editor_property('destructible_entrance')
                c.set_editor_property('required_analyzed_specimen','MirrorSkimmer' if entrance else 'None');save(bp)
                if entrance:habitats.append(p)
    removed=[]
    for species in repaired:
        p=ROOT+'/Meshes/SM_Sample_'+species
        if unreal.EditorAssetLibrary.does_asset_exist(p):
            assert unreal.EditorAssetLibrary.delete_asset(p),p
            removed.append(p)
    return {'reused_species':repaired,'gated_habitats':habitats,'deleted_obsolete_meshes':removed}

