"""Import the Mars geometry POC and create a separate autoplay preview map.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file> -unattended.
This script authors /Game/Space/Environment/MarsDestructionPreview/PlateDriftV2.
The masked surface handoff follows the source bone pose, without a material clock.
"""
import json
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
ART = ROOT / 'SourceArt/MarsDestruction'
MANIFEST = json.loads((ART / 'prototype_manifest.json').read_text(encoding='utf-8'))
FOLDER = '/Game/Space/Environment/MarsDestructionPreview/PlateDriftV2'
EA = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()


def save(asset):
    assert EA.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def import_fbx(filename, name, skeletal):
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.import_as_skeletal = skeletal
    options.mesh_type_to_import = (unreal.FBXImportType.FBXIT_SKELETAL_MESH if skeletal
                                   else unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.import_mesh = True
    options.import_materials = False
    options.import_textures = False
    options.import_animations = skeletal
    options.create_physics_asset = False
    if skeletal:
        options.skeletal_mesh_import_data.set_editor_property('use_t0_as_ref_pose', False)
    else:
        options.static_mesh_import_data.combine_meshes = True
        options.static_mesh_import_data.auto_generate_collision = False
    task = unreal.AssetImportTask()
    task.filename = str(ART / filename)
    task.destination_path = FOLDER
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    task.options = options
    AT.import_asset_tasks([task])
    return [unreal.load_asset(p) for p in task.imported_object_paths]


def import_animation(mesh):
    # Reimporting a SkeletalMesh does not refresh its existing AnimSequence.
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.import_mesh = False
    options.import_animations = True
    options.import_materials = options.import_textures = False
    options.skeleton = mesh.get_editor_property('skeleton')
    options.anim_sequence_import_data.set_editor_property('use_default_sample_rate', True)
    options.anim_sequence_import_data.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    task = unreal.AssetImportTask()
    task.filename = str(ART / 'SK_MarsExplosion_POC.fbx')
    task.destination_path = FOLDER
    task.destination_name = 'SK_MarsExplosion_POC_Anim'
    task.automated = task.replace_existing = task.replace_existing_settings = task.save = True
    task.options = options
    AT.import_asset_tasks([task])
    animation = unreal.load_asset(FOLDER + '/SK_MarsExplosion_POC_Anim')
    assert isinstance(animation, unreal.AnimSequence)
    assert abs(animation.get_play_length() - MANIFEST['duration_seconds']) < .05, animation.get_play_length()
    return animation


def import_texture(filename, name):
    task = unreal.AssetImportTask()
    task.filename = str(ART / filename)
    task.destination_path, task.destination_name = FOLDER, name
    task.automated = task.replace_existing = task.save = True
    AT.import_asset_tasks([task])
    texture = unreal.load_asset(FOLDER + '/' + name)
    assert texture
    return texture


def surface_mask(mat):
    """Read source bone contraction as progress, matching Blender while scrubbing.

    Both positions are in mesh local space. Actor translation/rotation/scale and
    the absolute world origin therefore do not control the handoff.
    """
    mel = unreal.MaterialEditingLibrary
    def node(cls, x, y):
        return mel.create_material_expression(mat, cls, x, y)
    def wire(src, dst, pin, output=''):
        names = mel.get_material_expression_input_names(dst)
        target = pin if pin in names else names[0] if pin in ('Input', 'Position') else pin
        assert mel.connect_material_expressions(src, output, dst, target), (src, dst, pin, names)
    zero = node(unreal.MaterialExpressionConstant, -1050, 800)
    zero.set_editor_property('r', 0)
    position = node(unreal.MaterialExpressionWorldPosition, -1400, 500)
    local = node(unreal.MaterialExpressionTransformPosition, -1150, 500)
    local.set_editor_property('transform_source_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    local.set_editor_property('transform_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    wire(position, local, 'Position')
    rest_vertex = node(unreal.MaterialExpressionPreSkinnedPosition, -1400, 800)
    rest = node(unreal.MaterialExpressionVertexInterpolator, -1150, 670)
    wire(rest_vertex, rest, 'Input')
    lengths = []
    for source, y in [(local, 500), (rest, 670)]:
        length = node(unreal.MaterialExpressionDistance, -900, y)
        wire(source, length, 'A')
        wire(zero, length, 'B')
        lengths.append(length)
    ratio = node(unreal.MaterialExpressionDivide, -650, 500)
    wire(lengths[0], ratio, 'A')
    wire(lengths[1], ratio, 'B')
    inverse = node(unreal.MaterialExpressionOneMinus, -450, 500)
    wire(ratio, inverse, 'Input')
    progress = node(unreal.MaterialExpressionDivide, -250, 500)
    progress.set_editor_property('const_b', .03)
    wire(inverse, progress, 'A')
    uv = node(unreal.MaterialExpressionTextureCoordinate, -650, 970)
    uv.set_editor_property('coordinate_index', 1)
    channel = node(unreal.MaterialExpressionComponentMask, -450, 970)
    channel.set_editor_property('r', False)
    channel.set_editor_property('g', True)
    channel.set_editor_property('b', False)
    channel.set_editor_property('a', False)
    wire(uv, channel, 'Input')
    # FBX import flips the V coordinate in every UV channel.
    unflip = node(unreal.MaterialExpressionOneMinus, -350, 1100)
    wire(channel, unflip, 'Input')
    width = node(unreal.MaterialExpressionMultiply, -250, 970)
    width.set_editor_property('const_b', .82)
    wire(unflip, width, 'A')
    margin = node(unreal.MaterialExpressionAdd, -50, 970)
    margin.set_editor_property('const_b', .08)
    wire(width, margin, 'A')
    field = node(unreal.MaterialExpressionSubtract, 180, 650)
    wire(margin, field, 'A')
    wire(progress, field, 'B')
    # Keep the mask sharp at the seam threshold; 1000 * field > .333.
    sharp = node(unreal.MaterialExpressionMultiply, 400, 650)
    sharp.set_editor_property('const_b', 1000)
    wire(field, sharp, 'A')
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property('opacity_mask_clip_value', .333)
    assert mel.connect_material_property(sharp, '', unreal.MaterialProperty.MP_OPACITY_MASK)


def material(name, texture=None):
    path = FOLDER + '/' + name
    mat = unreal.load_asset(path) if EA.does_asset_exist(path) else AT.create_asset(
        name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    mel = unreal.MaterialEditingLibrary
    mel.delete_all_material_expressions(mat)
    if texture:
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, 0, 0)
        node.texture = texture
        mel.connect_material_property(node, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
        fill = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 240, 150)
        fill.set_editor_property('const_b', .12)
        mel.connect_material_expressions(node, 'RGB', fill, 'A')
        mel.connect_material_property(fill, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        node = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, 0, 0)
        node.constant = unreal.LinearColor(.13, .026, .007, 1)
        mel.connect_material_property(node, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, 0, 250)
    rough.set_editor_property('r', .85)
    mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    if name == 'M_MarsIntact':
        surface_mask(mat)
    mat.set_editor_property('used_with_skeletal_mesh', True)
    mel.recompile_material(mat)
    save(mat)
    return mat


def main():
    EA.make_directory(FOLDER)
    textures = [import_texture('T_MarsIntact_4K.png', 'T_MarsIntact_4K'),
                import_texture('T_MarsDebris_Atlas.png', 'T_MarsDebris_Atlas')]
    mats = {'M_MarsIntact': material('M_MarsIntact', textures[0]),
            'M_MarsDebris_Atlas': material('M_MarsDebris_Atlas', textures[1])}
    imported = import_fbx('SK_MarsExplosion_POC.fbx', 'SK_MarsExplosion_POC', True)
    mesh = next(a for a in imported if isinstance(a, unreal.SkeletalMesh))
    animation = import_animation(mesh)
    materials = mesh.get_editor_property('materials')
    assert len(materials) == MANIFEST['export_materials'], len(materials)
    for index in range(len(materials)):
        slot = materials[index]
        name = str(slot.material_slot_name)
        assert name in mats, name
        slot.material_interface = mats[name]
        # Unreal arrays return a copy of each struct, so write the element back.
        materials[index] = slot
    mesh.set_editor_property('materials', materials)
    assert all(slot.material_interface == mats[str(slot.material_slot_name)] for slot in mesh.get_editor_property('materials'))
    save(mesh)
    final_assets = import_fbx('SM_MarsDebris_Final.fbx', 'SM_MarsDebris_Final', False)
    final = next(a for a in final_assets if isinstance(a, unreal.StaticMesh))
    assert len(final.static_materials) == 1
    final.set_material(0, mats['M_MarsDebris_Atlas'])
    # The preview has no gameplay collision. The production transition will enable a separate static body.
    save(final)
    level = FOLDER + '/L_MarsPlateDrift_Preview'
    if EA.does_asset_exist(level):
        assert unreal.EditorLevelLibrary.load_level(level)
    else:
        assert unreal.EditorLevelLibrary.new_level(level)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()

    def placed(cls, label, location, rotation=unreal.Rotator()):
        result = next((a for a in actors if a.get_actor_label() == label), None)
        if not result:
            result = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, location, rotation)
        result.set_actor_label(label)
        result.set_actor_location(location, False, False)
        result.set_actor_rotation(rotation, False)
        return result

    world = unreal.EditorLevelLibrary.get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode', unreal.GameModeBase)
    actor = placed(unreal.SkeletalMeshActor, 'MarsExplosion_GeometryPreview', unreal.Vector())
    actor.set_actor_scale3d(unreal.Vector(40, 40, 40))
    component = actor.skeletal_mesh_component
    component.set_skeletal_mesh_asset(mesh)
    for index, slot in enumerate(mesh.get_editor_property('materials')):
        component.set_material(index, slot.material_interface)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property('bounds_scale', 1.1)
    component.override_animation_data(animation, True, True, 0.0, 1.0)
    loc = unreal.Vector(0, 19000, 4800)
    rotation = unreal.MathLibrary.find_look_at_rotation(loc, unreal.Vector())
    camera = placed(unreal.CameraActor, 'MarsPreview_Camera', loc, rotation)
    camera.set_editor_property('auto_activate_for_player', unreal.AutoReceiveInput.PLAYER0)
    camera.camera_component.set_field_of_view(48)
    start = next((a for a in actors if isinstance(a, unreal.PlayerStart)), None)
    if start:
        start.set_actor_label('MarsPreview_PlayerStart')
    else:
        placed(unreal.PlayerStart, 'MarsPreview_PlayerStart', loc, rotation)
    for label, rot, intensity, color in [
        ('Key', unreal.Rotator(-35, 115, 0), 5.0, unreal.LinearColor(1, .83, .68, 1)),
        ('Fill', unreal.Rotator(-15, 315, 0), 1.2, unreal.LinearColor(.55, .68, 1, 1)),
    ]:
        light = placed(unreal.DirectionalLight, 'MarsPreview_' + label, unreal.Vector(), rot)
        light.light_component.set_intensity(intensity)
        light.light_component.set_light_color(color)
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc, rotation)
    assert unreal.EditorLevelLibrary.save_current_level()
    report = {'map': level, 'skeletal_mesh': mesh.get_path_name(), 'animation': animation.get_path_name(),
              'animation_seconds': animation.get_play_length(), 'skeletal_materials': len(materials),
              'static_final': final.get_path_name(), 'static_materials': len(final.static_materials),
              'autoplay_looping': True, 'visual_effects': 'Restrained rigid plate drift and pose-driven masked surface handoff. No shockwave or shell ejecta.',
              'integration': 'Separate preview map. Main SpaceWorld and multiplayer rules unchanged.'}
    assert abs(report['animation_seconds'] - MANIFEST['duration_seconds']) < .05, report
    (ROOT / 'Saved/MarsDestruction/unreal_import_report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('MARS_IMPORT_OK ' + json.dumps(report))


if __name__ == '__main__':
    main()
