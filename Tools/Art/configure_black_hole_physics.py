"""Author only the black-hole presentation and force parameters; leaves maps and other weapons alone."""
import unreal

library = unreal.MaterialEditingLibrary
path = "/Game/Space/Materials/Weapons/StellarFX/M_StellarVoidCore"
material = unreal.load_asset(path)
if not material:
    raise RuntimeError("Missing authored black-hole material: " + path)
library.delete_all_material_expressions(material)
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
material.set_editor_property("two_sided", False)

def expression(cls, **properties):
    node = library.create_material_expression(material, cls)
    for key, value in properties.items():
        node.set_editor_property(key, value)
    return node

def constant(value):
    return expression(unreal.MaterialExpressionConstant, r=value)

def connect(node, prop):
    if not library.connect_material_property(node, "", prop):
        raise RuntimeError("Could not connect black-hole material property " + str(prop))

black = expression(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(.004, .005, .009, 1))
connect(black, unreal.MaterialProperty.MP_BASE_COLOR)
connect(constant(.55), unreal.MaterialProperty.MP_METALLIC)
connect(constant(.7), unreal.MaterialProperty.MP_SPECULAR)
connect(constant(.16), unreal.MaterialProperty.MP_ROUGHNESS)
# A narrow, low-intensity edge catches the light without outlining the attraction radius.
rim = expression(unreal.MaterialExpressionFresnel, exponent=12.0, base_reflect_fraction=0.0)
rim_color = expression(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(.025, .032, .045, 1))
multiply = expression(unreal.MaterialExpressionMultiply)
library.connect_material_expressions(rim, "", multiply, "A")
library.connect_material_expressions(rim_color, "", multiply, "B")
connect(multiply, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
library.recompile_material(material)
if not unreal.EditorAssetLibrary.save_loaded_asset(material, False):
    raise RuntimeError("Could not save black-hole material")

bp_path = "/Game/Space/Blueprints/Weapons/Stellar/BP_StellarBlackHoleFX"
blueprint = unreal.load_asset(bp_path)
defaults = unreal.get_default_object(unreal.load_class(None, bp_path + ".BP_StellarBlackHoleFX_C"))
defaults.core.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Sphere"))
defaults.core.set_material(0, material)
defaults.core.set_cast_shadow(True)
defaults.set_editor_property("repulsion_sound", None)
for component in (defaults.core, defaults.field, defaults.orbit, defaults.orbit_inner, defaults.repulsion):
    component.set_visibility(False)
unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False):
    raise RuntimeError("Could not save black-hole FX Blueprint")

catalog = unreal.load_asset("/Game/Space/Data/Weapons/DA_StellarWeaponCatalog")
definitions = list(catalog.weapons)
for definition in definitions:
    if definition.mode == unreal.JTSStellarWeaponMode.BLACK_HOLE:
        definition.set_editor_property("black_hole_core_radius_centimeters", 65.0)
        definition.set_editor_property("black_hole_ground_clearance_centimeters", 45.0)
        definition.set_editor_property("repulsion_stiffness", 180.0)
catalog.set_editor_property("weapons", definitions)
if not unreal.EditorAssetLibrary.save_loaded_asset(catalog, False):
    raise RuntimeError("Could not save black-hole force parameters")
from pathlib import Path
import runpy
runpy.run_path(str(Path(unreal.Paths.project_dir()) / "Tools/Art/configure_stellar_repulsion.py"))
print("BLACK_HOLE_PHYSICS_ASSETS_SAVED: opaque black sphere, subtle highlights, purple dome, damped repulsion")
