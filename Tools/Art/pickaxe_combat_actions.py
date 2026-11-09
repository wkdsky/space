"""Scoped MCP authoring and runtime checks for pickaxe contacts and Riftlip seams."""
import json
import sys
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
ART=ROOT/'Tools/Art'
if str(ART) not in sys.path:sys.path.insert(0,str(ART))
BPS='/Game/Space/Blueprints/Planets/Moon/EcologyV3'
PLAYER='/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2'
MESHES='/Game/Space/Environment/MoonV3/Meshes'

def xyz(v):return [v.x,v.y,v.z]
def actors():return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
def inspect():
    rows=[]
    for kind in ('MirrorSkimmer','Riftlip'):
        cdo=unreal.get_default_object(unreal.load_asset(BPS+'/BP_'+kind).generated_class())
        rows.append(dict(kind=kind,walk=cdo.get_editor_property("walk_speed"),flee=cdo.get_editor_property("flee_speed"),body=cdo.get_component_by_class(unreal.SphereComponent).get_scaled_sphere_radius()))
    item=unreal.load_asset('/Game/Space/Data/Items/DA_Item_Pickaxe')
    props={k:str(item.get_editor_property(k)) for k in ('mining_reach','mining_swing_duration','mining_contact_fraction','held_presentation')}
    habitats=[]
    for a in actors():
        if isinstance(a,unreal.JTSPlanetEcologyHabitat) and a.get_actor_label().endswith('_Rift_Core'):
            sc=a.get_editor_property("surface_scatter")
            habitats.append(dict(label=a.get_actor_label(),data=str(sc.get_editor_property("distribution_data")),entries=[dict(meshes=[m.get_name() for m in e.get_editor_property('meshes')],cuts=[m.get_name() for m in e.get_editor_property('cut_volumes')],refuge=e.get_editor_property('refuge')) for e in sc.get_editor_property("entries")]))
    return dict(species=rows,item=props,habitats=habitats,world=str(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()))

def main(stage):
    if stage.startswith('boss_'):
        import mirror_boss_editor, importlib
        return importlib.reload(mirror_boss_editor).run(stage)
    if stage.startswith('moon_revision_'):
        import moon_combat_revision, importlib
        return importlib.reload(moon_combat_revision).run(stage)
    if stage.startswith('mirror_'):
        import mirror_contact_revision, importlib
        return importlib.reload(mirror_contact_revision).run(stage)
    if stage=='inspect':return inspect()
    if stage=='protected':
        import moon_surface_refinement
        return dict(protected=moon_surface_refinement.check_protected())
    if stage=='refuge_settings':
        return {kind:{p:str(unreal.get_default_object(unreal.load_asset('/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_'+kind).generated_class()).get_editor_property(p)) for p in ('refuge_entry_distance','flee_speed','animations')} for kind in ('MirrorSkimmer','Riftlip')}
    if stage=='close':
        assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
        unreal.SystemLibrary.quit_editor()
        return dict(closing=True)
    if stage=='configure':
        import pickaxe_combat_assets
        import importlib
        return importlib.reload(pickaxe_combat_assets).configure()
    if stage.startswith('qa_'):
        import pickaxe_combat_validation
        import importlib
        return importlib.reload(pickaxe_combat_validation).run(stage)
    raise ValueError(stage)

