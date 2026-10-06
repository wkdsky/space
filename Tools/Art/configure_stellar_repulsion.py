"""Author only the shallow-entry repulsion dome; safe to run independently of other weapons and maps."""
from pathlib import Path
import math
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
source = root / "SourceArt" / "StellarEffects" / "SM_StellarRepulsionDome.obj"
source.parent.mkdir(parents=True, exist_ok=True)
# Unit hemisphere: equator at local Z=0, radius 50 cm, smooth normals, no bottom cap.
segments, rings = 64, 24
vertices, faces = [(0.0, 0.0, 50.0)], []
for ring in range(1, rings + 1):
    latitude = ring * math.pi / (2 * rings)
    for segment in range(segments):
        angle = segment * 2 * math.pi / segments
        vertices.append((50 * math.sin(latitude) * math.cos(angle),
                         50 * math.sin(latitude) * math.sin(angle), 50 * math.cos(latitude)))
for segment in range(segments):
    faces.append((1, 2 + segment, 2 + (segment + 1) % segments))
for ring in range(rings - 1):
    top = 2 + ring * segments
    bottom = top + segments
    for segment in range(segments):
        following = (segment + 1) % segments
        faces.extend(((top + segment, bottom + segment, bottom + following),
                      (top + segment, bottom + following, top + following)))
lines = ["o SM_StellarRepulsionDome"]
lines.extend("v %.8f %.8f %.8f" % vertex for vertex in vertices)
lines.extend("vn %.8f %.8f %.8f" % tuple(value / 50 for value in vertex) for vertex in vertices)
lines.append("s 1")
lines.extend("f " + " ".join(f"{index}//{index}" for index in face) for face in faces)
source.write_text("\n".join(lines) + "\n", encoding="ascii")

assets = unreal.AssetToolsHelpers.get_asset_tools()
mesh_path = "/Game/Space/Meshes/Weapons/StellarFX"
options = unreal.FbxImportUI()
options.automated_import_should_detect_type = False
options.import_mesh = True
options.import_as_skeletal = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.original_import_type = unreal.FBXImportType.FBXIT_STATIC_MESH
options.import_materials = False
options.import_textures = False
options.static_mesh_import_data.combine_meshes = True
options.static_mesh_import_data.convert_scene = False
options.static_mesh_import_data.auto_generate_collision = False
task = unreal.AssetImportTask()
task.filename = str(source)
task.destination_path = mesh_path
task.destination_name = "SM_StellarRepulsionDome"
task.automated = True
task.replace_existing = True
task.save = True
task.factory = unreal.FbxFactory()
task.options = options
assets.import_asset_tasks([task])
mesh = unreal.load_asset(mesh_path + "/SM_StellarRepulsionDome")
if not mesh:
    raise RuntimeError("Repulsion hemisphere import failed")

material_path = "/Game/Space/Materials/Weapons/StellarFX"
name = "M_StellarRepulsionDome"
material = unreal.load_asset(material_path + "/" + name)
material = material or assets.create_asset(name, material_path, unreal.Material, unreal.MaterialFactoryNew())
library = unreal.MaterialEditingLibrary
library.delete_all_material_expressions(material)
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("two_sided", True)

def expression(cls, **properties):
    node = library.create_material_expression(material, cls)
    for key, value in properties.items():
        node.set_editor_property(key, value)
    return node

def constant(value):
    return expression(unreal.MaterialExpressionConstant, r=value)

def op(cls, a, b=None):
    node = expression(cls)
    slots = library.get_material_expression_input_names(node)
    if not library.connect_material_expressions(a, "", node, slots[0]):
        raise RuntimeError("Could not connect repulsion material " + cls.__name__)
    if b and not library.connect_material_expressions(b, "", node, slots[1]):
        raise RuntimeError("Could not connect repulsion material second input")
    return node

# Absolute view/normal dot keeps transparency readable both inside and outside the dome.
view = expression(unreal.MaterialExpressionCameraVectorWS)
normal = expression(unreal.MaterialExpressionPixelNormalWS)
edge = op(unreal.MaterialExpressionOneMinus,
          op(unreal.MaterialExpressionAbs, op(unreal.MaterialExpressionDotProduct, view, normal)))
edge = op(unreal.MaterialExpressionPower, edge, constant(3.0))
opacity = op(unreal.MaterialExpressionAdd, constant(.07),
             op(unreal.MaterialExpressionMultiply, edge, constant(.30)))
purple = expression(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(.30, .035, .72, 1))
library.connect_material_property(purple, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
library.recompile_material(material)
mesh.set_material(0, material)

bp_path = "/Game/Space/Blueprints/Weapons/Stellar/BP_StellarBlackHoleFX"
blueprint = unreal.load_asset(bp_path)
defaults = unreal.get_default_object(unreal.load_class(None, bp_path + ".BP_StellarBlackHoleFX_C"))
defaults.repulsion.set_static_mesh(mesh)
defaults.repulsion.set_material(0, material)
defaults.repulsion.set_visibility(False)
defaults.set_editor_property("bShowRepulsionBoundary", True)
defaults.set_editor_property("repulsion_sound", None)
unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

catalog = unreal.load_asset("/Game/Space/Data/Weapons/DA_StellarWeaponCatalog")
definitions = list(catalog.weapons)
for definition in definitions:
    if definition.mode == unreal.JTSStellarWeaponMode.BLACK_HOLE:
        definition.set_editor_property("black_hole_ground_clearance_centimeters", 45.0)
        definition.set_editor_property("repulsion_entry_depth_centimeters", 30.0)
        definition.set_editor_property("repulsion_exit_offset_centimeters", 12.0)
catalog.set_editor_property("weapons", definitions)
for asset in (material, mesh, blueprint, catalog):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
        raise RuntimeError("Could not save repulsion asset " + asset.get_name())
print("STELLAR_REPULSION_DOME_SAVED", mesh.get_bounds(), "entry=30cm exit=12cm")
