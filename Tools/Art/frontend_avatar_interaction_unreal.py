"""Author the direct avatar entry point and its lightweight hover material."""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
DEST = '/Game/Space/FrontEnd'
EA = unreal.EditorAssetLibrary
UMG = unreal.get_default_object(unreal.UMGToolSet)


def call(name, *args):
    return UMG.call_method(name, args)


def hover_material():
    path = DEST + '/Materials/M_FE_AvatarHover'
    mat = unreal.load_asset(path)
    if mat and EA.get_metadata_tag(mat, 'JTS.AvatarHover') == '4':
        return mat
    if not mat:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'M_FE_AvatarHover', DEST + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    me = unreal.MaterialEditingLibrary
    me.delete_all_material_expressions(mat)
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
    mat.set_editor_property('blendable_location', unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    tint = me.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 180)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(.15, .8, .85, 1))
    strength = me.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 350)
    strength.set_editor_property('parameter_name', 'Strength')
    strength.set_editor_property('default_value', 0.0)
    stencil = me.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -600, -180)
    stencil.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_CUSTOM_STENCIL)
    scene = me.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -600, -350)
    scene.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    depth = me.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -600, -520)
    depth.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_SCENE_DEPTH)
    custom_depth = me.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -600, -690)
    custom_depth.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_CUSTOM_DEPTH)
    width = me.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 500)
    width.set_editor_property('parameter_name', 'Width')
    width.set_editor_property('default_value', 2.5)
    index = me.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 650)
    index.set_editor_property('parameter_name', 'Stencil')
    index.set_editor_property('default_value', 250)
    outline = me.create_material_expression(mat, unreal.MaterialExpressionCustom, -100, 0)
    names = ['Scene', 'StencilSample', 'Depth', 'CustomDepth', 'Tint', 'Strength', 'Width', 'Stencil']
    inputs = []
    for name in names:
        value = unreal.CustomInput()
        value.set_editor_property('input_name', name)
        inputs.append(value)
    outline.set_editor_property('inputs', inputs)
    outline.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    outline.set_editor_property('code', '''
float2 uv = GetDefaultSceneTextureUV(Parameters, PPI_CustomStencil);
float2 texel = GetSceneTextureBufferSize(PPI_CustomStencil).zw;
float center = abs(StencilSample.r - Stencil) < 0.5;
float edge = 0;
[unroll] for (int y = -1; y <= 1; ++y) {
    [unroll] for (int x = -1; x <= 1; ++x) {
        float2 p = ClampSceneTextureUV(uv + float2(x,y) * texel * Width, PPI_CustomStencil);
        float match = abs(SceneTextureLookup(p, PPI_CustomStencil, false).r - Stencil) < 0.5;
        float visible = SceneTextureLookup(p, PPI_CustomDepth, false).r <= SceneTextureLookup(p, PPI_SceneDepth, false).r + 1.0;
        edge = max(edge, match * visible);
    }
}
return lerp(Scene.rgb, Tint.rgb, edge * (1 - center) * saturate(Strength));
''')
    for source, name in zip((scene, stencil, depth, custom_depth, tint, strength, width, index), names):
        assert me.connect_material_expressions(source, '', outline, name)
    assert me.connect_material_property(outline, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    me.recompile_material(mat)
    EA.set_metadata_tag(mat, 'JTS.AvatarHover', '4')
    assert EA.save_loaded_asset(mat)
    return mat


def configure_widget(bp, material):
    widgets = {str(i.widget_name): i.widget for i in call('GetWidgets', bp).widgets}
    for name in ('Home_Appearance', 'Lobby_Appearance'):
        if name in widgets:
            assert call('RemoveWidget', bp, widgets[name])
    for page_name, widget_name in (('Home', 'Home_Avatar'), ('Lobby', 'Lobby_Avatars')):
        page = widgets[page_name]
        view = widgets.get(widget_name)
        if not view:
            view = call('AddWidget', bp, unreal.load_class(None, '/Script/space.JTSFrontEndAvatarWidget'),
                        widget_name, page, -1).widget
        view.slot.set_position(unreal.Vector2D(0, 0))
        view.slot.set_size(unreal.Vector2D(1920, 1080))
        view.slot.set_z_order(1)
        view.set_visibility(unreal.SlateVisibility.VISIBLE)
        view.set_editor_property('is_focusable', True)
        view.set_editor_property('name_anchor', 'Head')
        view.set_editor_property('name_offset', unreal.Vector(0, 0, 48))
        view.set_editor_property('name_font', unreal.SlateFontInfo(
            font_object=unreal.load_asset(DEST + '/Fonts/F_JTS_UI_Bold_Font'), size=21))
        view.set_editor_property('detail_font', unreal.SlateFontInfo(
            font_object=unreal.load_asset(DEST + '/Fonts/F_JTS_UI_Regular_Font'), size=14))
        view.set_editor_property('accent', unreal.LinearColor(.15, .8, .85, 1))
        view.set_editor_property('hover_material', material)
        view.set_editor_property('outline_stencil', 250)
        view.set_editor_property('outline_width', 2.5)
        editor_style = widgets['ExpeditionName'].get_editor_property('widget_style')
        editor_style.set_editor_property('background_color', unreal.SlateColor(specified_color=unreal.LinearColor(.025,.04,.055,1)))
        view.set_editor_property('name_editor_style', editor_style)
        view.set_editor_property('focus_sound', unreal.load_asset(DEST + '/Audio/S_UI_Focus'))
        view.set_editor_property('click_sound', unreal.load_asset(DEST + '/Audio/S_UI_Confirm'))
        # Follow the animated body rather than one large rectangular click area.
        bones = [('Head', 'Head', 27), ('Hips', 'Chest', 17), ('Chest', 'Neck', 15)]
        for side in ('L', 'R'):
            bones.extend([(f'Shoulder_{side}', f'UpperArm_{side}', 14),
                          (f'UpperArm_{side}', f'LowerArm_{side}', 13),
                          (f'LowerArm_{side}', f'Wrist_{side}', 12),
                          (f'Wrist_{side}', f'Middle3_{side}', 10),
                          (f'UpperLeg_{side}', f'LowerLeg_{side}', 10),
                          (f'LowerLeg_{side}', f'Foot_{side}', 8),
                          (f'Foot_{side}', f'Foot_{side}', 12)])
        regions = []
        for start, end, radius in bones:
            region = unreal.JTSAvatarPointerRegion()
            region.set_editor_property('start_bone', start)
            region.set_editor_property('end_bone', end)
            region.set_editor_property('radius', radius)
            regions.append(region)
        view.set_editor_property('pointer_regions', regions)
        probe = unreal.new_object(unreal.SkeletalMeshComponent)
        catalog = unreal.load_asset(DEST + '/Data/DA_FE_AvatarCatalog')
        for suit in catalog.get_editor_property('suits'):
            probe.set_skeletal_mesh_asset(suit)
            sockets = {str(n) for n in probe.get_all_socket_names()}
            assert all(start in sockets and end in sockets for start, end, _ in bones), suit.get_path_name()
        for child in page.get_all_children():
            if child == view:
                continue
            if isinstance(child.slot, unreal.CanvasPanelSlot):
                child.slot.set_z_order(0 if child.get_name() in (page_name + '_Plate', page_name + '_Stripe') else 2)
        if page_name == 'Home':
            view.set_navigation_rule_explicit(unreal.UINavigation.LEFT, widgets['Home_Join'])
            widgets['Home_Join'].set_navigation_rule_explicit(unreal.UINavigation.RIGHT, view)
        else:
            view.set_navigation_rule_explicit(unreal.UINavigation.DOWN, widgets['Lobby_Ready'])
            widgets['Lobby_Ready'].set_navigation_rule_explicit(unreal.UINavigation.UP, view)
    for i, action in enumerate(('Saves', 'Join', 'Settings', 'Credits', 'Quit')):
        widgets['Home_' + action].slot.set_position(unreal.Vector2D(56, 424 + i * 78))
    for i, action in enumerate(('Leave', 'Settings', 'CloseJoining', 'Mute', 'Ready', 'Launch')):
        w = widgets['Lobby_' + action]
        w.slot.set_position(unreal.Vector2D(56 + i * 304, 974))
        w.slot.set_size(unreal.Vector2D(284, 64))
    widgets['Home_Sub'].set_text('1–4 人合作远征 · 集结队友，从地球出发')
    widgets['Lobby_Sub'].set_text('等待队友时，点击自己的宇航员即可编辑外观。')
    assert call('CompileWidgetBlueprint', bp)
    assert EA.save_loaded_asset(bp)
    return {'widget': bp.get_path_name(), 'direct_avatar_entry': True}


material = hover_material()
REPORT = {'material': material.get_path_name(), 'widgets': [
    configure_widget(unreal.load_asset(DEST + '/Widgets/' + name), material)
    for name in ('WBP_FE_Main', 'WBP_FE_Lobby')]}
