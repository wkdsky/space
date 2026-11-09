"""Focused editor authoring and PIE verification for the permanent Spire colony revision."""
import json
import hashlib
import sys
from pathlib import Path
import unreal
unreal.EditorPythonScripting.set_keep_python_script_alive(True)

ROOT=Path('D:/projects/space')
sys.path.insert(0,str(ROOT/'Tools/Art'))
import moon_surface_unreal as surface
from moon_population import _recovery

BPS=surface.BPS
OUT=ROOT/'Saved/SpireColony'
OUT.mkdir(parents=True,exist_ok=True)


def protected_snapshot():
    actors=surface.actors()
    labels=[n for n in actors if n.startswith('Moon_Nest_') or n in ('MoonPlanet','Moon_Corpse_SkeletonAstronaut')]
    source=ROOT/'Content/ThirdParty/Environment/UltimateSpace/Meshes/Planet_3.uasset'
    return dict(transforms={n:str(actors[n].get_actor_transform()).split('{rotation:',1)[-1] for n in labels},moon_sha256=hashlib.sha256(source.read_bytes()).hexdigest())


def configure_towers(import_assets=True):
    """Apply the tall sealed tower family and its three small foot pores, independently of ant assets or rules."""
    surface.editor_world();before=protected_snapshot()
    layouts=json.loads((ROOT/'SourceArt/SpireColony/colony_layout.json').read_text(encoding='utf-8'))
    folder='/Game/Space/Environment/MoonV3/SpireColony'
    if import_assets:
        surface.import_tasks([surface.fbx_task(ROOT/'SourceArt/SpireColony'/(row['name']+'.fbx'),folder,row['name']) for row in layouts])
    interior=surface.material('M_SpireColony_Interior',(.0015,.0015,.0015),1)
    colour=unreal.MaterialEditingLibrary.get_material_property_input_node(interior,unreal.MaterialProperty.MP_BASE_COLOR)
    colour.set_editor_property('constant',unreal.LinearColor(.0015,.0015,.0015,1))
    unreal.MaterialEditingLibrary.recompile_material(interior);surface.save(interior)
    mats={'M_SpireColony_Shell':unreal.load_asset(surface.MATERIALS+'/M_MS3_Tower'),
          'M_SpireColony_Ridge':unreal.load_asset(surface.MATERIALS+'/M_MS3_TowerLight'),
          'M_SpireColony_Interior':interior}
    assert all(mats.values())
    meshes=[]
    for row in layouts:
        asset=unreal.load_asset(folder+'/'+row['name']);slots=list(asset.static_materials)
        for slot in slots:slot.material_interface=mats[str(slot.material_slot_name)]
        asset.set_editor_property('static_materials',slots)
        asset.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        surface.save(asset);meshes.append(asset)
    bp=unreal.load_asset(BPS+'/BP_MoonClosedColony');cdo=unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('closed_mesh',meshes[0]);cdo.set_editor_property('height_range',unreal.Vector2D(360,440))
    cdo.get_editor_property('colony_defense').set_editor_property('entrance_offsets',[unreal.Vector(*v) for v in layouts[0]['entrances_cm']])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp);surface.save(bp)
    rows=[]
    nests=sorted((a for a in surface.actors().values() if isinstance(a,unreal.JTSPlanetColonyNest)),key=lambda a:a.get_actor_label())
    for index,nest in enumerate(nests):
        variant=index%len(meshes);nest.modify()
        nest.set_editor_property('closed_mesh',meshes[variant]);nest.get_editor_property('nest_mesh').set_static_mesh(meshes[variant])
        nest.set_editor_property('height_range',unreal.Vector2D(360,440))
        nest.get_editor_property('colony_defense').set_editor_property('entrance_offsets',[unreal.Vector(*v) for v in layouts[variant]['entrances_cm']])
        top=meshes[variant].get_bounding_box().max.z;nest.set_actor_scale3d(unreal.Vector(400/top,400/top,400/top))
        rows.append(dict(name=nest.get_actor_label(),height_cm=400,mesh=meshes[variant].get_name(),foot_pores=3))
    assert protected_snapshot()==before
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return rows


def configure():
    surface.editor_world()
    before=protected_snapshot()
    folder='/Game/Space/Environment/MoonV3/SpireColony'
    tasks=[surface.fbx_task(ROOT/'SourceArt/SpireColony'/(f'SM_SpireColony_{i}.fbx'),folder,f'SM_SpireColony_{i}') for i in range(3)]
    species='SpireSoldier';name='SK_'+species;sf=surface.SPECIES+'/'+species
    tasks.append(surface.fbx_task(surface.CREATURES/species/(name+'.fbx'),sf,name,True))
    surface.import_tasks(tasks)
    mesh=unreal.load_asset(sf+'/'+name)
    mats={}
    colours={'Shell':(.34,.38,.40),'Wear':(.22,.25,.27),'Joint':(.065,.08,.09),'Crown':(.66,.61,.50)}
    for role,colour in colours.items():
        mats['M_MC_SpireSoldier_'+role]=surface.material('M_SpireSoldier_'+role,colour,.82)
    slots=list(mesh.materials)
    for slot in slots: slot.material_interface=mats[str(slot.material_slot_name)]
    mesh.set_editor_property('materials',slots)
    manifest=json.loads((surface.CREATURES/'manifest.json').read_text(encoding='utf-8'))
    clips=next(r for r in manifest['creatures'] if r['species']==species)['clips']
    surface.import_tasks([surface.fbx_task(surface.CREATURES/species/(c['action']+'.fbx'),sf+'/Animations',c['action'],True,True,mesh.skeleton) for c in clips])
    for asset in [mesh,mesh.skeleton]+[unreal.load_asset(sf+'/Animations/'+c['action']) for c in clips]: surface.save(asset)
    for kind in ('SpireWorker','SpireSoldier'):
        bp=unreal.load_asset(BPS+'/BP_'+kind);cdo=unreal.get_default_object(bp.generated_class())
        cdo.set_editor_property('body_scale',.5)
        for key,value in dict(defends_nest=False,retaliates=False,repairs_refuge=False,roam_radius=600,
                              refuge_entry_distance=45,leash_radius=1800,walk_speed=65 if kind=='SpireWorker' else 85,
                              flee_speed=185 if kind=='SpireWorker' else 260,attack_reach=80 if kind=='SpireWorker' else 100).items():
            cdo.set_editor_property(key,value)
        steering=cdo.get_editor_property('steering')
        for key,value in dict(probe_distance=18,body_radius=5,body_ground_clearance=8,heading_response=8).items():
            steering.set_editor_property(key,value)
        state=unreal.JTSForagerState
        rates=dict(cdo.get_editor_property('animation_play_rates'))
        rates.update({state.WALK:1.8,state.FLEE:2.0,state.EMERGE:3.5,state.BURROW:2.4})
        cdo.set_editor_property('animation_play_rates',rates)
        if kind=='SpireSoldier':
            cdo.get_editor_property('mesh').set_skeletal_mesh_asset(mesh)
            # Bind the complete new jaw/head silhouette to the same animated body bone.
            cdo.set_editor_property('combat_body_bone','body')
            cdo.set_editor_property('combat_body_center',unreal.Vector(5,0,13))
            cdo.set_editor_property('combat_body_half_extent',unreal.Vector(36,16,13))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp);surface.save(bp)
    rows=configure_towers(False)
    nests=sorted((a for a in surface.actors().values() if isinstance(a,unreal.JTSPlanetColonyNest)),key=lambda a:a.get_actor_label())
    for index,nest in enumerate(nests):
        nest.modify()
        defense=nest.get_editor_property('colony_defense')
        defense.set_editor_property('roaming_workers',unreal.IntPoint(8,10))
        defense.set_editor_property('fighting_workers',unreal.IntPoint(2,3))
        defense.set_editor_property('fighting_soldiers',unreal.IntPoint(4,6))
        for key,count in [('workers',20),('guards',12)]:
            habitat=nest.get_editor_property(key);habitat.modify()
            for k,v in dict(enemy_count=count,spawn_radius=650,core_radius=650,minimum_spacing=35,
                            maintain_population=True,recovery=_recovery(40,420,1000,1),
                            maintenance_target=nest,refuges=[nest]).items():habitat.set_editor_property(k,v)
    assert protected_snapshot()==before
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return json.dumps(dict(nests=rows,protected=True))


def inspect():
    subsystem=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    w=subsystem.get_game_world() or subsystem.get_editor_world()
    rows=[]
    for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.JTSPlanetColonyNest):
        c=a.get_editor_property('colony_defense')
        rows.append(dict(name=a.get_name(),workers=c.get_living_workers(),soldiers=c.get_living_soldiers(),
                         outside_workers=c.get_outside_workers(),outside_soldiers=c.get_outside_soldiers(),
                         health=a.get_editor_property('health').get_health()))
    return json.dumps(rows)


import toolset_registry
from toolset_registry.registration import Registration
@unreal.uclass()
class SpireColonyTools(unreal.ToolsetDefinition):
    @toolset_registry.tool_call
    @staticmethod
    def configure_spire_colonies()->str:
        """Import the revised soldier and towers; configure finite renewable colonies without changing lunar terrain."""
        return configure()
    @toolset_registry.tool_call
    @staticmethod
    def inspect_spire_colonies()->str:
        """Inspect living and outside populations of the permanent Spire colonies in the editor or PIE."""
        return inspect()
    @toolset_registry.tool_call
    @staticmethod
    def validate_spire_colony(stage:str)->str:
        """Disposable PIE probes: summary, attack, pursuit, leave, deplete, accelerate_recovery, client_authority, capture."""
        import spire_colony_validation
        return json.dumps(spire_colony_validation.run(stage))

registration=Registration([SpireColonyTools]);registration.register()
