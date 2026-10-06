"""Author held-model catalog entries in the editor; preserve implemented attacks.

Run after import_stellar_held_models.py with the rebuilt project module loaded.
New entries explicitly use PresentationOnly until their attack rules are implemented.
"""
import unreal

catalog = unreal.load_asset('/Game/Space/Data/Weapons/DA_StellarWeaponCatalog')
table = catalog.get_editor_property('loot_table')
existing = {(str(d.core_id), str(d.attachment_id)): d for d in catalog.weapons}
models = {name + 'Tube': name for name in ('Jet', 'Explosion', 'Healing', 'Freezing',
          'Focus', 'Shaping', 'Effect', 'Diffusion', 'Shadow', 'BlackHole', 'Instance', 'Disassembly')}
def transform_at(x=0, z=0):
    transform = unreal.Transform()
    transform.set_editor_property('Translation', unreal.Vector(x, 0, z))
    transform.set_editor_property('Rotation', unreal.Quat(0, 0, 0, 1))
    transform.set_editor_property('Scale3D', unreal.Vector(1, 1, 1))
    return transform


identity = transform_at()
definitions = []
for entry in table.entries:
    attachment = str(entry.item_id)
    core = str(entry.compatible_core_id)
    if entry.get_editor_property('bCore') or core == 'None':
        continue
    model = models[attachment]
    key = (core, attachment)
    definition = existing.get(key)
    if definition is None:
        definition = unreal.JTSStellarWeaponDefinition()
        definition.set_editor_property('core_id', core)
        definition.set_editor_property('attachment_id', attachment)
        definition.set_editor_property('mode', unreal.JTSStellarWeaponMode.PRESENTATION_ONLY)
        definition.set_editor_property('base_damage', 0)
        definition.set_editor_property('status_damage_per_second', 0)
    mesh = unreal.load_asset('/Game/Space/Meshes/Weapons/Stellar/SM_Stellar' + model)
    if not mesh:
        raise RuntimeError('Missing held model: ' + model)
    definition.set_editor_property('held_mesh', mesh)
    definition.set_editor_property('held_grip_transform', identity)
    definition.set_editor_property('held_muzzle_transform', transform_at(8, 36))
    definition.set_editor_property('core_material_slot', 2)
    definition.set_editor_property('melee_presentation', False)
    definition.set_editor_property('upright_scepter', True)
    definition.set_editor_property('held_carry_rotation', unreal.Rotator(pitch=0, yaw=0, roll=0))
    if attachment == 'JetTube':
        definition.set_editor_property('range_centimeters', 1000.0)
        definition.set_editor_property('cone_angle_degrees', 120.0)
        definition.set_editor_property('focused_range_centimeters', 2200.0)
        definition.set_editor_property('focused_cone_angle_degrees', 30.0)
    definitions.append(definition)
    print('STELLAR_HELD_CATALOG', core, attachment, definition.mode, model)
catalog.set_editor_property('weapons', definitions)
unreal.EditorAssetLibrary.save_loaded_asset(catalog)
print('STELLAR_HELD_CATALOG_SAVED', len(definitions))
