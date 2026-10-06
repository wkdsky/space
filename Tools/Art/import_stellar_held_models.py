"""Run inside Unreal's Python console after generating the OBJ sources.

Reimports only the dedicated stellar held meshes. Material assets are
created with the editor tools and are preserved across these mesh reimports.
"""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
names = ("SM_StellarJet", "SM_StellarFocus", "SM_StellarBlackHole", "SM_StellarExplosion",
         "SM_StellarHealing", "SM_StellarFreezing", "SM_StellarShaping", "SM_StellarEffect",
         "SM_StellarDiffusion", "SM_StellarShadow", "SM_StellarInstance", "SM_StellarDisassembly")
tasks = []
for name in names:
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.import_mesh = True
    options.import_as_skeletal = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.original_import_type = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    options.static_mesh_import_data.combine_meshes = True
    options.static_mesh_import_data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    task = unreal.AssetImportTask()
    task.filename = str(root / "SourceArt" / "StellarWeapons" / (name + ".obj"))
    task.destination_path = "/Game/Space/Meshes/Weapons/Stellar"
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    task.factory = unreal.FbxFactory()
    task.options = options
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
materials = [unreal.load_asset("/Game/Space/Materials/Weapons/" + n)
             for n in ("MI_StellarHeldShell", "MI_StellarHeldPanel", "M_StellarHeldCore")]
for name in names:
    mesh = unreal.load_asset("/Game/Space/Meshes/Weapons/Stellar/" + name)
    for slot, material in enumerate(materials):
        mesh.set_material(slot, material)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    print("STELLAR_HELD_MESH", name, mesh.get_bounds())
