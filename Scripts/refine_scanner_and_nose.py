"""Configure the wide ground scanner and independent recessed nose weapon assets."""
import unreal
from pathlib import Path
ROOT='/Game/Space/Ships/Refit'
SOURCE=Path('D:/projects/space/SourceArt/ShipRefit/ScannerNose')
EA=unreal.EditorAssetLibrary

def save(o):
    if isinstance(o,unreal.Blueprint): unreal.BlueprintEditorLibrary.compile_blueprint(o)
    assert EA.save_loaded_asset(o,False)

def apply():
    at=unreal.AssetToolsHelpers.get_asset_tools();meshes={}
    for f in SOURCE.glob('*.fbx'):
        task=unreal.AssetImportTask();task.filename=str(f);task.destination_path=ROOT+'/Meshes';task.destination_name=f.stem
        task.automated=True;task.replace_existing=True;task.save=True
        opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False
        opt.automated_import_should_detect_type=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
        d=opt.static_mesh_import_data;d.combine_meshes=True;d.generate_lightmap_u_vs=False;d.auto_generate_collision=False;d.convert_scene=True;d.force_front_x_axis=False;d.import_rotation=unreal.Rotator(yaw=180)
        task.options=opt;at.import_asset_tasks([task]);m=unreal.load_asset(ROOT+'/Meshes/'+f.stem);assert m
        for i,slot in enumerate(m.static_materials):
            key='_'.join(str(slot.material_slot_name).split('_')[:2]);mat=unreal.load_asset(ROOT+'/Materials/M_'+key);assert mat,key;m.set_material(i,mat)
        save(m);meshes[f.stem]=m
    path=ROOT+'/BP_ShipRecessedLaser'
    if EA.does_asset_exist(path):bp=unreal.load_asset(path)
    else:
        fac=unreal.BlueprintFactory();fac.set_editor_property('parent_class',unreal.JTSShipWeaponModule)
        bp=at.create_asset('BP_ShipRecessedLaser',ROOT,unreal.Blueprint,fac)
    w=unreal.get_default_object(bp.generated_class());original=unreal.get_default_object(unreal.load_asset(ROOT+'/BP_ShipLaser').generated_class())
    w.mount.set_static_mesh(meshes['SM_Nose_RecessHousing']);w.mount.set_editor_property('relative_scale3d',unreal.Vector(-1,1,1))
    w.turret.set_static_mesh(None);w.gun.set_static_mesh(meshes['SM_Nose_RecessBarrel']);w.gun.set_editor_property('relative_scale3d',unreal.Vector(-1,1,1))
    w.pitch_pivot.set_editor_property('relative_location',unreal.Vector(32,0,0));w.muzzle.set_editor_property('relative_location',unreal.Vector(-30,0,0))
    for c,src in [(w.beam,original.beam),(w.impact,original.impact)]:c.set_static_mesh(src.static_mesh);c.set_editor_property('relative_scale3d',src.relative_scale3d)
    for key in ['damage','fire_interval','range','fire_sound','beam_radius']:w.set_editor_property(key,original.get_editor_property(key))
    w.set_editor_property('limited_yaw',12);w.set_editor_property('limited_pitch',8);w.set_editor_property('turn_rate',65);w.set_editor_property('recoil_travel',3);save(bp)
    for path in ['/Game/Space/Ships/TwinforkR1/BP_Twinfork_R1','/Game/Space/Blueprints/Ships/BP_Spacecraft']:
        ship=unreal.load_asset(path);c=unreal.get_default_object(ship.generated_class());scan=c.scanner
        for key,value in [('zone_radius',330),('zone_height',240),('column_height',220),('offset_from_screen',unreal.Vector(0,390,0)),('camera_offset_from_screen',unreal.Vector(0,13,65)),('small_sample_seconds',4.2)]:scan.set_editor_property(key,value)
        hs=list(c.armament.hardpoints)
        for h in hs:
            if str(h.id) in ['L1','R1']:
                h.weapon_class=bp.generated_class();h.full_rotation=False
                h.mount_transform=unreal.Transform(location=unreal.Vector(648,-520 if str(h.id)=='L1' else 520,6))
        c.armament.set_editor_property('hardpoints',hs);save(ship)
    terminal=unreal.load_asset('/Game/Space/Ships/Docks/BP_Ship_CentralTerminal')
    sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
    def templates():
        out={}
        for h in sub.k2_gather_subobject_data_for_blueprint(terminal):
            o=lib.get_object_for_blueprint(lib.get_data(h),terminal)
            if o:out[o.get_name().removesuffix('_GEN_VARIABLE')]=(h,o)
        return out
    ts=templates();print('TERMINAL_COMPONENTS',list(ts))
    if 'ScannerCamera' not in ts:
        parent=ts['ScreenAnchor'][0]
        h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.StaticMeshComponent,blueprint_context=terminal));assert not str(reason),str(reason)
        assert sub.rename_subobject(h,unreal.Text('ScannerCamera'));ts=templates()
    cam=ts['ScannerCamera'][1];cam.set_static_mesh(meshes['SM_Scanner_LineCamera']);cam.set_editor_property('relative_location',unreal.Vector(0,0,65));cam.set_editor_property('relative_scale3d',unreal.Vector(-1,1,1));cam.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);save(terminal)
    print('Scanner radius 330cm; nose traversal yaw +/-12 pitch +/-8')

if __name__=='__main__':apply()
