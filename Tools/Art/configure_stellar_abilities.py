"""Run in rebuilt Unreal Editor; authors nine ability FX Blueprints and the weapon catalog.

Does not modify player, level, population, held meshes, or the existing black-hole balance.
"""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
ART = ROOT / "SourceArt" / "StellarEffects"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
BASE = "/Game/Space"
FX_DIR = BASE + "/Blueprints/Weapons/Stellar"
MAT_DIR = BASE + "/Materials/Weapons/StellarFX"
MESH_DIR = BASE + "/Meshes/Weapons/StellarFX"
AUDIO_DIR = BASE + "/Audio/Weapons/Stellar"

def asset(name, folder, cls, factory):
    return unreal.load_asset(folder + "/" + name) or TOOLS.create_asset(name, folder, cls, factory)

tasks = []
for name in ("SM_StellarAbilityDisc", "SM_StellarAbilityArc"):
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
    task.filename = str(ART / (name + ".obj")); task.destination_path = MESH_DIR
    task.destination_name = name; task.automated = True; task.replace_existing = True
    task.factory = unreal.FbxFactory(); task.options = options; tasks.append(task)
names = ("Explosion", "Healing", "Freezing", "Shaping", "Radiance", "Diffusion", "Shadow", "Instance", "Disassembly")
for name in names:
    task = unreal.AssetImportTask()
    task.filename = str(ART / ("S_Stellar" + name + ".wav")); task.destination_path = AUDIO_DIR
    task.destination_name = "S_Stellar" + name; task.automated = True; task.replace_existing = True
    task.factory = unreal.SoundFactory(); tasks.append(task)
TOOLS.import_asset_tasks(tasks)
for task in tasks:
    for path in task.imported_object_paths:
        unreal.EditorAssetLibrary.save_asset(path)

def material(name, kind):
    mat = asset("M_StellarAbility" + name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT if kind == "robot" else unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE if kind == "robot" else unreal.BlendMode.BLEND_TRANSLUCENT if kind == "shield" else unreal.BlendMode.BLEND_ADDITIVE)
    def node(cls, **props):
        n = MEL.create_material_expression(mat, cls)
        for key, value in props.items(): n.set_editor_property(key, value)
        return n
    def c(value): return node(unreal.MaterialExpressionConstant, r=value)
    def op(cls, a, b=None):
        n = node(cls)
        inputs = MEL.get_material_expression_input_names(n)
        key = inputs[0]
        assert MEL.connect_material_expressions(a, "", n, key)
        if b is not None: assert MEL.connect_material_expressions(b, "", n, inputs[1])
        return n
    color = node(unreal.MaterialExpressionVectorParameter, parameter_name="Color", default_value=unreal.LinearColor(.3, .7, 1, 1))
    fade = node(unreal.MaterialExpressionScalarParameter, parameter_name="Fade", default_value=1)
    emission = op(unreal.MaterialExpressionMultiply, color, c(1.2 if kind == "shield" else 2.8))
    if kind == "robot":
        assert MEL.connect_material_property(op(unreal.MaterialExpressionMultiply, color, c(.3)), "", unreal.MaterialProperty.MP_BASE_COLOR)
        assert MEL.connect_material_property(c(.6), "", unreal.MaterialProperty.MP_METALLIC)
        assert MEL.connect_material_property(c(.3), "", unreal.MaterialProperty.MP_ROUGHNESS)
    elif kind == "shield":
        fresnel = node(unreal.MaterialExpressionFresnel, exponent=3, base_reflect_fraction=.01)
        opacity = op(unreal.MaterialExpressionMultiply, fresnel, c(.24))
    else:
        uv = node(unreal.MaterialExpressionTextureCoordinate)
        radial = node(unreal.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False)
        angular = node(unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
        for mask in (radial, angular):
            assert MEL.connect_material_expressions(uv, "", mask, MEL.get_material_expression_input_names(mask)[0])
        time = node(unreal.MaterialExpressionTime)
        wave = op(unreal.MaterialExpressionAbs, op(unreal.MaterialExpressionSine,
            op(unreal.MaterialExpressionSubtract, op(unreal.MaterialExpressionMultiply, radial, c(5)), op(unreal.MaterialExpressionMultiply, time, c(2)))))
        opacity = op(unreal.MaterialExpressionAdd, c(.07), op(unreal.MaterialExpressionMultiply, wave, c(.18)))
        if kind in ("disc", "arc"):
            sweep = node(unreal.MaterialExpressionScalarParameter, parameter_name="SweepFraction", default_value=1)
            angle = op(unreal.MaterialExpressionAbs, op(unreal.MaterialExpressionSubtract, angular, c(.5)))
            cutoff = op(unreal.MaterialExpressionSaturate, op(unreal.MaterialExpressionMultiply,
                op(unreal.MaterialExpressionSubtract, op(unreal.MaterialExpressionMultiply, sweep, c(.5)), angle), c(150)))
            opacity = op(unreal.MaterialExpressionMultiply, opacity, cutoff)
            rim = op(unreal.MaterialExpressionPower, radial, c(3 if kind == "disc" else 1))
            opacity = op(unreal.MaterialExpressionMultiply, opacity, rim)
        depth = node(unreal.MaterialExpressionDepthFade, fade_distance_default=35)
        assert MEL.connect_material_expressions(op(unreal.MaterialExpressionMultiply, opacity, fade), "", depth, MEL.get_material_expression_input_names(depth)[0])
        opacity = depth
    if kind != "robot":
        if kind == "shield": opacity = op(unreal.MaterialExpressionMultiply, opacity, fade)
        assert MEL.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    assert MEL.connect_material_property(op(unreal.MaterialExpressionMultiply, emission, fade), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.delete_unused_expressions(mat)
    MEL.layout_material_expressions(mat)
    errors = MEL.recompile_material(mat)
    if errors: raise RuntimeError("Material compile failed: " + str(errors))
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat

mats = {kind: material(name, kind) for kind, name in {
    "beam": "Beam", "disc": "Surface", "arc": "Blade", "shield": "Shield", "robot": "Robot"}.items()}
meshes = {
    "sphere": unreal.load_asset("/Engine/BasicShapes/Sphere"),
    "cube": unreal.load_asset("/Engine/BasicShapes/Cube"),
    "cylinder": unreal.load_asset("/Engine/BasicShapes/Cylinder"),
    "disc": unreal.load_asset(MESH_DIR + "/SM_StellarAbilityDisc"),
    "arc": unreal.load_asset(MESH_DIR + "/SM_StellarAbilityArc"),
    "orbit": unreal.load_asset(MESH_DIR + "/SM_StellarOrbit"),
}
for key in ("disc", "arc"):
    meshes[key].set_material(0, mats[key]); unreal.EditorAssetLibrary.save_loaded_asset(meshes[key])
parent = unreal.load_class(None, "/Script/space.JTSStellarEffectActor")
classes = {}
for name in names:
    bp_name = "BP_Stellar" + name + "FX"
    factory = unreal.BlueprintFactory(); factory.set_editor_property("parent_class", parent)
    bp = asset(bp_name, FX_DIR, unreal.Blueprint, factory)
    cls = unreal.load_class(None, FX_DIR + "/" + bp_name + "." + bp_name + "_C")
    cdo = unreal.get_default_object(cls)
    def configure(component, shape, kind):
        component.set_static_mesh(meshes[shape]); component.set_material(0, mats[kind]); component.set_visibility(False)
        component.set_cast_shadow(False)
        component.set_editor_property("translucency_sort_priority", 0)
    for component in (cdo.beam, cdo.beam_glow, cdo.impact, cdo.core):
        configure(component, "cube" if name == "Instance" and component == cdo.core else "sphere" if component in (cdo.impact, cdo.core) else "cylinder", "robot" if name == "Instance" and component == cdo.core else "beam")
    configure(cdo.field, "sphere" if name == "Explosion" else "arc" if name == "Shaping" else "disc", "beam" if name == "Explosion" else "arc" if name == "Shaping" else "disc")
    configure(cdo.orbit, "arc" if name == "Shaping" else "orbit", "arc" if name == "Shaping" else "beam")
    configure(cdo.orbit_inner, "orbit", "beam"); configure(cdo.repulsion, "sphere", "shield")
    for component in cdo.chain_beams: configure(component, "cylinder", "beam")
    sound = unreal.load_asset(AUDIO_DIR + "/S_Stellar" + name)
    sound.set_editor_property("attenuation_settings", unreal.load_asset(AUDIO_DIR + "/A_StellarWeapon"))
    sound.set_editor_property("looping", name in ("Healing", "Shadow", "Disassembly"))
    unreal.EditorAssetLibrary.save_loaded_asset(sound)
    cdo.set_editor_property("shot_sound", sound if name not in ("Healing", "Shadow", "Disassembly") else None)
    cdo.set_editor_property("loop_sound", sound if name in ("Healing", "Shadow", "Disassembly") else None)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp); unreal.EditorAssetLibrary.save_loaded_asset(bp)
    classes[name] = cls

config = {
    "Explosion": dict(base_damage=500., range_centimeters=3000., area_radius_centimeters=300., status_damage_per_second=100., primary_energy_per_cast=14., primary_interval=.65, secondary_energy=30., secondary_cooldown=8., charge_seconds=.8),
    "Healing": dict(base_damage=0., range_centimeters=2500., area_radius_centimeters=300., energy_per_second=18.),
    "Freezing": dict(base_damage=100., range_centimeters=3000., area_radius_centimeters=500., energy_per_second=22., secondary_energy=26., primary_interval=.2, beam_radius_centimeters=12., base_pierce_targets=2),
    "Shaping": dict(base_damage=400., range_centimeters=240., area_radius_centimeters=240., primary_energy_per_cast=8., primary_interval=.4, secondary_energy=25., secondary_cooldown=6., charge_seconds=1.2),
    "Radiance": dict(base_damage=340., area_radius_centimeters=400., energy_per_second=26., secondary_energy=25., secondary_cooldown=12.),
    "Diffusion": dict(base_damage=90., range_centimeters=4500., status_damage_per_second=100., energy_per_second=22., primary_interval=.125, beam_radius_centimeters=14., base_pierce_targets=2, chain_radius_centimeters=500.),
    "Shadow": dict(base_damage=0., status_damage_per_second=380., area_radius_centimeters=400., energy_per_second=24., secondary_energy=22., secondary_cooldown=12., near_ground_height=200.),
    "Instance": dict(base_damage=140., range_centimeters=1800., area_radius_centimeters=200., primary_energy_per_cast=18., primary_interval=.65, charge_seconds=2., robot_health=180., robot_explosion_damage=650.),
    "Disassembly": dict(base_damage=1100., range_centimeters=2500., primary_energy_per_cast=28., charge_seconds=.8, chain_radius_centimeters=600.),
}
colors = {"Explosion": (1,.16,.015), "Healing": (.1,.85,.65), "Freezing": (.18,.65,1), "Shaping": (1,.72,.18), "Radiance": (1,.88,.4), "Diffusion": (.45,.055,.8), "Shadow": (.22,.045,.42), "Instance": (.12,1,.35), "Disassembly": (.1,1,.6)}
catalog = unreal.load_asset(BASE + "/Data/Weapons/DA_StellarWeaponCatalog")
weapons = list(catalog.weapons)
for definition in weapons:
    name = str(definition.attachment_id).removesuffix("Tube")
    if name == "Effect": name = "Radiance"
    if name in config:
        definition.set_editor_property("mode", getattr(unreal.JTSStellarWeaponMode, name.upper()))
        definition.set_editor_property("effect_class", classes[name])
        definition.set_editor_property("color", unreal.LinearColor(*colors[name], 1))
        for key, value in config[name].items(): definition.set_editor_property(key, value)
    definition.set_editor_property("upright_scepter", True)
    definition.set_editor_property("melee_presentation", False)
catalog.set_editor_property("weapons", weapons)
unreal.EditorAssetLibrary.save_loaded_asset(catalog)
# Add restrained core coronas to the two existing straight attacks without replacing their assets.
for name in ("Jet", "Focus"):
    path = FX_DIR + "/BP_Stellar" + name + "FX"
    bp = unreal.load_asset(path); cdo = unreal.get_default_object(unreal.load_class(None, path + ".BP_Stellar" + name + "FX_C"))
    cdo.core.set_static_mesh(meshes["sphere"]); cdo.core.set_material(0, mats["beam"])
    cdo.orbit.set_static_mesh(meshes["orbit"]); cdo.orbit.set_material(0, mats["beam"])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp); unreal.EditorAssetLibrary.save_loaded_asset(bp)
print("STELLAR_ABILITIES_CONFIGURED", len(weapons))
status_script = ROOT / "Tools/Art/configure_stellar_status_effects.py"
exec(compile(status_script.read_text(encoding="utf-8"), str(status_script), "exec"))
