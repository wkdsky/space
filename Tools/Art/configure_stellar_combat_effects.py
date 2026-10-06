"""Author balance, FX, audio and moon population in their owning UE assets.

Run with UnrealEditor-Cmd -ExecutePythonScript after the C++ module is rebuilt.
All paths below are editor configuration, never runtime C++ asset references.
"""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
ART = ROOT / "SourceArt" / "StellarEffects"
ASSETS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
MAT_PATH = "/Game/Space/Materials/Weapons/StellarFX"
MESH_PATH = "/Game/Space/Meshes/Weapons/StellarFX"
AUDIO_PATH = "/Game/Space/Audio/Weapons/Stellar"

tasks = []
for name in ("SM_StellarPlume", "SM_StellarOrbit"):
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
    task.filename = str(ART / (name + ".obj"))
    task.destination_path = MESH_PATH
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.factory = unreal.FbxFactory()
    task.options = options
    tasks.append(task)
for name in ("S_StellarFlameLoop", "S_StellarVoidLoop", "S_StellarFocusShot", "S_StellarRepulsion"):
    task = unreal.AssetImportTask()
    task.filename = str(ART / (name + ".wav"))
    task.destination_path = AUDIO_PATH
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.factory = unreal.SoundFactory()
    tasks.append(task)
ASSETS.import_asset_tasks(tasks)

def create(name, path, cls, factory):
    asset = unreal.load_asset(path + "/" + name)
    return asset or ASSETS.create_asset(name, path, cls, factory)

def material(name, kind):
    m = create(name, MAT_PATH, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(m)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE if kind == "void" else
                          unreal.BlendMode.BLEND_TRANSLUCENT if kind == "shell" else unreal.BlendMode.BLEND_ADDITIVE)
    def expr(cls, **props):
        node = MEL.create_material_expression(m, cls)
        for k, v in props.items():
            node.set_editor_property(k, v)
        return node
    def op(cls, a, b=None):
        node = expr(cls)
        input_name = "A" if b else MEL.get_material_expression_input_names(node)[0]
        if not MEL.connect_material_expressions(a, "", node, input_name):
            raise RuntimeError("Could not connect " + cls.__name__ + "." + input_name)
        if b:
            if not MEL.connect_material_expressions(b, "", node, "B"):
                raise RuntimeError("Could not connect " + cls.__name__ + ".B")
        return node
    def c(value):
        return expr(unreal.MaterialExpressionConstant, r=value)
    color = expr(unreal.MaterialExpressionVectorParameter, parameter_name="Color", default_value=unreal.LinearColor(1, .25, .05, 1))
    fade = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Fade", default_value=1.0)
    fresnel = expr(unreal.MaterialExpressionFresnel, exponent=4.0, base_reflect_fraction=0.02)
    if kind == "void":
        m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        black = expr(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(.004, .005, .009, 1))
        MEL.connect_material_property(black, "", unreal.MaterialProperty.MP_BASE_COLOR)
        MEL.connect_material_property(c(.55), "", unreal.MaterialProperty.MP_METALLIC)
        MEL.connect_material_property(c(.7), "", unreal.MaterialProperty.MP_SPECULAR)
        MEL.connect_material_property(c(.16), "", unreal.MaterialProperty.MP_ROUGHNESS)
        rim_color = expr(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(.025, .032, .045, 1))
        fresnel.set_editor_property("exponent", 12.0)
        fresnel.set_editor_property("base_reflect_fraction", 0.0)
        emission = op(unreal.MaterialExpressionMultiply, rim_color, fresnel)
    else:
        uv = expr(unreal.MaterialExpressionTextureCoordinate)
        along = expr(unreal.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False)
        if not MEL.connect_material_expressions(uv, "", along, MEL.get_material_expression_input_names(along)[0]):
            raise RuntimeError("Could not connect the effect texture coordinate")
        time = expr(unreal.MaterialExpressionTime)
        flow = op(unreal.MaterialExpressionSubtract,
                  op(unreal.MaterialExpressionMultiply, along, c(7 if kind == "flame" else 3)),
                  op(unreal.MaterialExpressionMultiply, time, c(4 if kind == "flame" else 2)))
        bands = op(unreal.MaterialExpressionAbs, op(unreal.MaterialExpressionSine, flow))
        emission = op(unreal.MaterialExpressionMultiply, color, c(4 if kind == "flame" else 6))
        if kind == "shell":
            opacity = op(unreal.MaterialExpressionMultiply, fresnel, c(.22))
        elif kind == "flame":
            opacity = op(unreal.MaterialExpressionMultiply,
                         op(unreal.MaterialExpressionAdd, c(.035), op(unreal.MaterialExpressionMultiply, bands, c(.16))),
                         op(unreal.MaterialExpressionOneMinus, along))
        else:
            opacity = op(unreal.MaterialExpressionAdd, c(.30), op(unreal.MaterialExpressionMultiply, bands, c(.30)))
        opacity = op(unreal.MaterialExpressionMultiply, opacity, fade)
        if not MEL.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY):
            raise RuntimeError("Could not connect effect opacity")
        emission = op(unreal.MaterialExpressionMultiply, emission, fade)
    if not MEL.connect_material_property(emission, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Could not connect effect emissive color")
    MEL.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m

mats = {k: material(n, k) for k, n in {
    "flame": "M_StellarFlameFlow", "laser": "M_StellarLaserGlow",
    "shell": "M_StellarFieldShell", "void": "M_StellarVoidCore"}.items()}
meshes = {"cone": unreal.load_asset(MESH_PATH + "/SM_StellarPlume"),
          "orbit": unreal.load_asset(MESH_PATH + "/SM_StellarOrbit"),
          "sphere": unreal.load_asset("/Engine/BasicShapes/Sphere"),
          "cylinder": unreal.load_asset("/Engine/BasicShapes/Cylinder")}
for key in ("cone", "orbit"):
    meshes[key].set_material(0, mats["flame" if key == "cone" else "laser"])
    unreal.EditorAssetLibrary.save_loaded_asset(meshes[key])
    print("STELLAR_FX_MESH_BOUNDS", key, meshes[key].get_bounds())

attenuation = create("A_StellarWeapon", AUDIO_PATH, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
settings = attenuation.get_editor_property("attenuation")
settings.set_editor_property("attenuation_shape_extents", unreal.Vector(300, 0, 0))
settings.set_editor_property("falloff_distance", 5000.0)
attenuation.set_editor_property("attenuation", settings)
unreal.EditorAssetLibrary.save_loaded_asset(attenuation)
sounds = {}
for name in ("S_StellarFlameLoop", "S_StellarVoidLoop", "S_StellarFocusShot", "S_StellarRepulsion"):
    sound = unreal.load_asset(AUDIO_PATH + "/" + name)
    sound.set_editor_property("looping", name.endswith("Loop"))
    sound.set_editor_property("attenuation_settings", attenuation)
    unreal.EditorAssetLibrary.save_loaded_asset(sound)
    sounds[name] = sound

for mode in ("Jet", "Focus", "BlackHole"):
    path = "/Game/Space/Blueprints/Weapons/Stellar/BP_Stellar" + mode + "FX"
    bp = unreal.load_asset(path)
    cdo = unreal.get_default_object(unreal.load_class(None, path + ".BP_Stellar" + mode + "FX_C"))
    def set_mesh(component, shape, mat):
        component.set_static_mesh(meshes[shape])
        component.set_material(0, mats[mat])
        component.set_visibility(False)
    if mode != "BlackHole":
        shape, mat = ("cone", "flame") if mode == "Jet" else ("cylinder", "laser")
        set_mesh(cdo.beam, shape, mat)
        set_mesh(cdo.beam_glow, shape, mat)
        set_mesh(cdo.impact, "sphere", "laser")
    else:
        set_mesh(cdo.field, "sphere", "shell")
        set_mesh(cdo.core, "sphere", "void")
        set_mesh(cdo.repulsion, "orbit", "void")
        cdo.core.set_cast_shadow(True)
        cdo.set_editor_property("bShowRepulsionBoundary", False)
        set_mesh(cdo.orbit, "orbit", "laser")
        set_mesh(cdo.orbit_inner, "orbit", "laser")
    for link in cdo.chain_beams:
        set_mesh(link, "cylinder", "laser")
    cdo.set_editor_property("loop_sound", sounds["S_StellarFlameLoop" if mode == "Jet" else "S_StellarVoidLoop"] if mode != "Focus" else None)
    cdo.set_editor_property("shot_sound", sounds["S_StellarFocusShot"] if mode == "Focus" else None)
    cdo.set_editor_property("repulsion_sound", None)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    print("STELLAR_FX_CONFIGURED", mode)

catalog = unreal.load_asset("/Game/Space/Data/Weapons/DA_StellarWeaponCatalog")
balance = {
    "JetTube": dict(range_centimeters=1000.0, base_damage=460.0, status_damage_per_second=60.0,
                    cone_angle_degrees=120.0, focused_range_centimeters=2200.0,
                    focused_cone_angle_degrees=30.0, range_per_level=60.0, maximum_targets=200),
    "FocusTube": dict(range_centimeters=5500.0, base_damage=84.0, beam_radius_centimeters=18.0,
                      base_pierce_targets=5, base_chain_targets=1, chain_radius_centimeters=900.0, maximum_targets=200),
    "BlackHoleTube": dict(range_centimeters=3000.0, base_damage=220.0, area_radius_centimeters=900.0,
                          radius_per_level=50.0, repulsion_radius_centimeters=650.0, pull_speed=550.0, maximum_targets=200,
                          black_hole_energy_per_cast=30.0, black_hole_lifetime_seconds=8.0,
                          black_hole_cast_cooldown=0.5, black_hole_base_maximum_fields=1,
                          black_hole_core_radius_centimeters=65.0, black_hole_ground_clearance_centimeters=45.0,
                          repulsion_stiffness=180.0),
}
definitions = list(catalog.weapons)
for d in definitions:
    for key, value in balance.get(str(d.attachment_id), {}).items():
        d.set_editor_property(key, value)
catalog.set_editor_property("weapons", definitions)
unreal.EditorAssetLibrary.save_loaded_asset(catalog)

for map_name in ("L_SpaceWorld", "L_SpaceWorld_Authoring"):
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    level.load_level("/Game/Space/Maps/" + map_name)
    clusters = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
                if isinstance(a, unreal.JTSMoonCubeCluster)]
    if map_name == "L_SpaceWorld" and len(clusters) != 1:
        raise RuntimeError("Expected exactly one moon cube settlement; found " + str(len(clusters)))
    for cluster in clusters:
        for key, value in dict(EnemyCount=200, SpawnRadius=2800.0, MinimumSpacing=145.0,
                               bMaintainPopulation=True, RefillInterval=1.0, RefillBatchSize=20,
                               MinimumPlayerDistance=800.0).items():
            cluster.set_editor_property(key, value)
        print("MOON_POPULATION_CONFIGURED", map_name, cluster.get_name(), cluster.get_editor_property("EnemyCount"))
    if clusters:
        level.save_current_level()
import runpy
runpy.run_path(str(ROOT / "Tools/Art/configure_stellar_repulsion.py"))
print("STELLAR_COMBAT_CONFIGURATION_COMPLETE")
