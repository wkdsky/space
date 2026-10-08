import unreal
import json
for path in ("/Game/Space/Materials/Weapons/M_StellarHeld", "/Game/Space/Ships/TwinforkR1/Mat/M_Twinfork_Hull"):
    m=unreal.load_asset(path)
    print("PARAMETERS",path,"vector",unreal.MaterialEditingLibrary.get_vector_parameter_names(m),"scalar",unreal.MaterialEditingLibrary.get_scalar_parameter_names(m))
new=unreal.load_asset("/Game/Space/Characters/SkeletonAstronaut/SK_SkeletonAstronaut")
old=unreal.load_asset("/Game/ThirdParty/Player/UltimateModularMen/Casual_2/Mesh/Casual_2")
for m in (old,new):
    print("MESH",m.get_path_name(),"skeleton",m.skeleton.get_path_name(),"bounds",m.get_bounds(),"slots",[str(s.material_slot_name) for s in m.materials])
    print("SOURCE",m.get_editor_property("asset_import_data").extract_filenames())
reg=unreal.AssetRegistryHelpers.get_asset_registry()
print("ANIMATIONS",[str(a.package_name) for a in reg.get_assets_by_path("/Game/ThirdParty/Player/UltimateModularMen/Casual_2",recursive=True) if a.asset_class_path.asset_name == "AnimSequence"])
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
print("WORLD",world.get_path_name())
