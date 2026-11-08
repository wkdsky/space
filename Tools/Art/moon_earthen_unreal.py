"""MCP import, Blueprint tuning and visible emergence checks for the earthen burrow revision."""
import json
import math
from pathlib import Path
import unreal

ROOT = Path('D:/projects/space')
ART = ROOT/'SourceArt/MoonSurfaceV3/EarthenEntrances'
MESHES = '/Game/Space/Environment/MoonV3/Meshes'
TEXTURES = '/Game/Space/Environment/MoonV3/Textures'
BPS = '/Game/Space/Blueprints/Planets/Moon/EcologyV3'
TUNING = {'MirrorSkimmer': (140,360,.9,110), 'Riftlip': (110,300,1.0,100),
          'SpireWorker': (120,280,1.2,100), 'SpireSoldier': (150,380,1.1,140)}


def configure_species(species):
    bp = unreal.load_asset(BPS+'/BP_'+species)
    assert bp, species
    cdo = unreal.get_default_object(bp.generated_class())
    walk, run, interval, reach = TUNING[species]
    for key, value in dict(walk_speed=walk, flee_speed=run, attack_interval=interval, attack_reach=reach).items():
        cdo.set_editor_property(key, value)
    state = unreal.JTSForagerState
    cdo.set_editor_property('animation_play_rates', {state.WALK:1.8, state.FLEE:1.35,
        state.EMERGE:3.5, state.BURROW:2.4, state.ATTACK:2.0, state.HIT:2.5, state.STARTLE:2.0})
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    return dict(species=species,walk=walk,run=run,attack_interval=interval,reach=reach)


def _material():
    tasks=[]
    for suffix in ('Color','Normal'):
        name='T_MS3_ExcavatedSoil_'+suffix
        task=unreal.AssetImportTask(); task.filename=str(ART/(name+'.png'))
        task.destination_path=TEXTURES;task.destination_name=name
        task.automated=True;task.replace_existing=True;task.save=True; tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    color=unreal.load_asset(TEXTURES+'/T_MS3_ExcavatedSoil_Color')
    normal=unreal.load_asset(TEXTURES+'/T_MS3_ExcavatedSoil_Normal')
    normal.set_editor_property('srgb',False)
    normal.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
    assert unreal.EditorAssetLibrary.save_loaded_asset(normal)
    path='/Game/Space/Environment/MoonV3/Materials/M_MS3_ExcavatedSoil'
    mat=unreal.load_asset(path)
    if not mat:
        mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_MS3_ExcavatedSoil',path.rsplit('/',1)[0],unreal.Material,unreal.MaterialFactoryNew())
    lib=unreal.MaterialEditingLibrary
    mat.set_editor_property('two_sided',False)
    lib.delete_all_material_expressions(mat)
    uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate)
    uv.set_editor_property('u_tiling',2);uv.set_editor_property('v_tiling',2)
    for texture,prop in [(color,unreal.MaterialProperty.MP_BASE_COLOR),(normal,unreal.MaterialProperty.MP_NORMAL)]:
        sample=lib.create_material_expression(mat,unreal.MaterialExpressionTextureSample)
        sample.set_editor_property('texture',texture)
        if texture==normal: sample.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        lib.connect_material_expressions(uv,'',sample,'Coordinates')
        lib.connect_material_property(sample,'RGB',prop)
    for value,prop in [(.98,unreal.MaterialProperty.MP_ROUGHNESS),(.12,unreal.MaterialProperty.MP_SPECULAR)]:
        node=lib.create_material_expression(mat,unreal.MaterialExpressionConstant);node.set_editor_property('r',value)
        lib.connect_material_property(node,'',prop)
    lib.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def _apply():
    import moon_surface_unreal as surface
    import moon_surface_refinement as refinement
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert refinement.check_protected()
    material=_material()
    rows=[]
    for variant in range(4):
        for prefix in ('SM_MS3_SafeSlopingRefuge_','SM_MS3_EntranceRubble_'):
            name=prefix+str(variant)
            # These existing assets were authored with Geometry Script and have no FBX reimport data.
            # Transfer Blender's sculpted buffers into the same objects, preserving every frozen mesh reference.
            raw=json.loads((ART/(name+'.json')).read_text())
            dm=unreal.DynamicMesh()
            dm.append_buffers_to_mesh(unreal.GeometryScriptSimpleMeshBuffers(
                vertices=[unreal.Vector(*v) for v in raw['vertices']],
                # Blender uses the opposite front-face winding to Unreal's static mesh buffers.
                triangles=[unreal.IntVector(t[0],t[2],t[1]) for t in raw['triangles']],
                normals=[unreal.Vector(*v) for v in raw['normals']],
                uv0=[unreal.Vector2D(*v) for v in raw['uv']]),0)
            mesh=unreal.load_asset(MESHES+'/'+name)
            if mesh:
                unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dm,mesh,
                    unreal.GeometryScriptCopyMeshToAssetOptions(),unreal.GeometryScriptMeshWriteLOD())
            else:
                mesh,_=unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm,MESHES+'/'+name,
                    unreal.GeometryScriptCreateNewStaticMeshAssetOptions(enable_collision=True,
                        enable_recompute_normals=True,enable_recompute_tangents=True))
            assert mesh
            mesh.set_editor_property('static_materials',[unreal.StaticMaterial(material_interface=material,material_slot_name='ExcavatedRegolith')])
            mesh.set_editor_property('allow_cpu_access',True)
            mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
            rows.append(dict(mesh=mesh.get_name(),triangles=len(raw['triangles'])))
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    nests=[a for a in actors if a.get_actor_label().startswith('Moon_Nest_')]
    assert len(nests)==9
    for nest in nests:
        nest.modify()
        variant=(int(nest.get_actor_label()[-2:])-1)%4
        nest.get_editor_property('entrance_blocker').set_static_mesh(unreal.load_asset(MESHES+'/SM_MS3_EntranceRubble_'+str(variant)))
        nest.set_editor_property('entrance_interaction_offset',unreal.Vector(-55,0,23))
        nest.set_editor_property('entrance_emergence_offset',unreal.Vector(-48,0,0))
        nest.set_editor_property('entrance_exit_offset',unreal.Vector(-155,0,0))
        nest.get_editor_property('blockade').set_editor_property('defender_emergence_interval',.2)
    tuning=[configure_species(species) for species in TUNING]
    assert refinement.check_protected()
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return dict(meshes=rows,tuning=tuning,protected=True)


def run(stage):
    if stage=='apply':
        try:return json.dumps(_apply())
        except Exception:
            import traceback
            return json.dumps(dict(error=traceback.format_exc()))
    if stage=='inspect':
        rows=[]
        for species in TUNING:
            cdo=unreal.get_default_object(unreal.load_asset(BPS+'/BP_'+species).generated_class())
            rows.append(dict(species=species,walk=cdo.get_editor_property('walk_speed'),run=cdo.get_editor_property('flee_speed'),
                animations={str(k):v.get_play_length() for k,v in cdo.get_editor_property('animations').items()}))
        return json.dumps(rows)
    raise ValueError(stage)
