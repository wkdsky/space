"""Editor Python: configure the reusable ship envelope and concurrent landing. No level placements change."""
import json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
assert not unreal.EditorLevelLibrary.get_pie_worlds(True)
path='/Game/Space/Ships/TwinforkR1/BP_Twinfork_R1'
bp=unreal.load_asset(path)
cls=unreal.load_class(None,path+'.BP_Twinfork_R1_C')
cdo=unreal.get_default_object(cls)
envelope=cdo.get_surface_envelope_component()
envelope.set_editor_property('settings',unreal.JTSSurfaceEnvelopeSettings())
movement=cdo.get_flight_movement_component()
movement.set_editor_property('assisted_landing_correction_speed',350.0)
presentation=cdo.get_component_by_class(unreal.JTSSpacecraftPresentationComponent)
presentation.set_editor_property('gear_deployment_duration',0.65)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
s=envelope.get_editor_property('settings')
keys=['flat_clearance','maximum_terrain_grade_degrees','shoulder_rounding_distance','smooth_maximum_width','minimum_sampling_radius','maximum_sampling_radius','prediction_time','sampling_interval','maximum_roughness_clearance','hover_offset','following_enter_height','following_exit_height']
report={'asset':path,'envelope':{k:s.get_editor_property(k) for k in keys},'correction_speed_cm_s':movement.get_editor_property('assisted_landing_correction_speed'),'gear_deploy_seconds':presentation.get_editor_property('gear_deployment_duration')}
(root/'Docs/Validation/spacecraft_surface_configuration.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report))
