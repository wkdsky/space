"""Dispatch production front-end authoring through Unreal MCP."""
import json
import runpy
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
job_path=ROOT/'Saved/FrontEndJob.json'
job=json.loads(job_path.read_text(encoding='utf-8-sig')) if job_path.exists() else {'step':'inspect'}
step=job['step']
scripts={'ui':'frontend_build_ui_unreal.py','avatar_interaction':'frontend_avatar_interaction_unreal.py','daylight_assets':'frontend_daylight_unreal.py',
         'presentation_polish':'frontend_presentation_polish_unreal.py','entry':'frontend_maps_unreal.py','lobby':'frontend_maps_unreal.py'}
if step in scripts:
    REPORT=runpy.run_path(str(ROOT/'Tools/Art'/scripts[step])).get('REPORT',{})
elif step=='player':
    bp=unreal.load_asset('/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2')
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cls=unreal.load_class(None,'/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2.BP_JTSPlayer_Casual_2_C')
    cdo=unreal.get_default_object(cls)
    cdo.set_editor_property('avatar_catalog',unreal.load_asset('/Game/Space/FrontEnd/Data/DA_FE_AvatarCatalog'))
    component=cdo.get_component_by_class(unreal.load_class(None,'/Script/space.JTSAvatarAppearanceComponent'))
    assert component
    component.set_editor_property('catalog',unreal.load_asset('/Game/Space/FrontEnd/Data/DA_FE_AvatarCatalog'))
    subobjects=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library=unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subobjects.k2_gather_subobject_data_for_blueprint(bp):
        data=library.get_data(handle)
        obj=library.get_object_for_blueprint(data,bp)
        if obj and obj.get_class().get_name()=='JTSAvatarAppearanceComponent':
            obj.set_editor_property('catalog',component.get_editor_property('catalog'))
    cdo.mesh.set_skeletal_mesh_asset(unreal.load_asset('/Game/Space/FrontEnd/Characters/SK_FE_Explorer'))
    assert unreal.EditorAssetLibrary.save_asset('/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2',only_if_is_dirty=False)
    REPORT={'success':True,'pawn':cls.get_path_name()}
elif step=='inspect':
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    REPORT={'world':world.get_path_name() if world else None}
else:
    raise ValueError('Unknown authoring step: '+step)
report=ROOT/'Saved/Art/FrontEnd/configure_report.json'
report.parent.mkdir(parents=True,exist_ok=True)
report.write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
