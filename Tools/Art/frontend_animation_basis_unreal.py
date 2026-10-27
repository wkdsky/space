"""Adapt the canonical centimetre FBX animation to the existing metre-bone rig.

The shared skeleton stores a 100x root scale and metre child translations.
Interchange imports the new FBX as centimetre child tracks with a unit root.
Convert authored tracks once, without changing runtime component scales.
"""
import unreal


def adapt_relaxed_idle(asset):
    library=unreal.AnimationLibrary
    # Child translation values are metres under a 100x root. Preserve centimetre
    # weight shifts instead of compressing them as sub-millimetre noise.
    asset.set_editor_property('compression_error_threshold_scale',100.0)
    path='/Game/Space/FrontEnd/Animations/BCS_FE_Presentation'
    settings=unreal.load_asset(path)
    if not settings:
        settings=unreal.AssetToolsHelpers.get_asset_tools().create_asset('BCS_FE_Presentation',path.rsplit('/',1)[0],
            unreal.AnimBoneCompressionSettings,unreal.AnimBoneCompressionSettingsFactory())
        codec=unreal.new_object(unreal.load_class(None,'/Script/Engine.AnimCompress_LeastDestructive'),outer=settings)
        settings.set_editor_property('codecs',[codec])
        settings.set_editor_property('error_threshold',.00001)
        settings.set_editor_property('force_below_threshold',True)
        unreal.EditorAssetLibrary.save_loaded_asset(settings,only_if_is_dirty=False)
    asset.set_editor_property('bone_compression_settings',settings)
    root=library.get_bone_pose_for_frame(asset,'Root',0,False)
    if abs(root.scale3d.x-100)<.01:
        unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)
        return {'converted':False,'root_scale':root.scale3d.x}
    assert abs(root.scale3d.x-1)<.01, 'Unexpected source animation basis'
    controller=asset.get_editor_property('controller')
    model=asset.get_editor_property('data_model_interface')
    names=list(model.get_bone_track_names())
    count=model.get_number_of_keys()
    tracks={str(name):[[],[],[]] for name in names}
    options=unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('should_retarget',False)
    options.set_editor_property('evaluation_type',unreal.AnimDataEvalType.SOURCE)
    for frame in range(count):
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(asset,frame/30.0,options)
        transforms=[unreal.AnimPoseExtensions.get_bone_pose(pose,name,unreal.AnimPoseSpaces.LOCAL) for name in names]
        for name,transform in zip(names,transforms):
            position=transform.translation
            scale=transform.scale3d
            if str(name).lower()=='root':
                scale=scale*100
            else:
                position=position*.01
            track=tracks[str(name)]
            track[0].append(position);track[1].append(transform.rotation);track[2].append(scale)
    controller.open_bracket('Match relaxed idle to shared skeleton units',False)
    try:
        for name,keys in tracks.items():
            assert controller.set_bone_track_keys(name,*keys,False), name
    finally:
        controller.close_bracket(False)
    unreal.EditorAssetLibrary.set_metadata_tag(asset,'JTS.AnimationBasis','Root100_MetreChildren')
    unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)
    return {'converted':True,'tracks':len(names),'keys':count,'root_scale':100}
