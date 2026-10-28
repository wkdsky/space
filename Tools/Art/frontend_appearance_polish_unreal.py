"""Editor-owned appearance layout and style; invoked using Unreal MCP."""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
DEST = '/Game/Space/FrontEnd'
EA = unreal.EditorAssetLibrary
UMG = unreal.get_default_object(unreal.UMGToolSet)
REPORT = {}


def call(name, *args):
    return UMG.call_method(name, args)


def color(hex_value, alpha=1):
    def linear(v):
        v = int(v, 16)/255
        return v/12.92 if v <= .04045 else ((v+.055)/1.055)**2.4
    return unreal.LinearColor(*(linear(hex_value[i:i+2]) for i in (0, 2, 4)), alpha)


INK = color('EDF3F6')
MUTED = color('8D9FAA')
CYAN = color('6DD9DF')
ORANGE = color('F7A65A')


def rounded(fill, outline='334653', radius=9, width=1):
    b = unreal.SlateBrush()
    b.set_editor_property('draw_as', unreal.SlateBrushDrawType.ROUNDED_BOX)
    b.set_editor_property('tint_color', unreal.SlateColor(specified_color=fill))
    settings = b.get_editor_property('outline_settings')
    settings.set_editor_property('corner_radii', unreal.Vector4(radius, radius, radius, radius))
    settings.set_editor_property('rounding_type', unreal.SlateBrushRoundingType.FIXED_RADIUS)
    settings.set_editor_property('color', unreal.SlateColor(specified_color=color(outline)))
    settings.set_editor_property('width', width)
    b.set_editor_property('outline_settings', settings)
    return b


def style_button(w, primary=False, swatch=False):
    st = w.get_editor_property('widget_style')
    fills = ('F7A65A', 'FFC184', 'CF8440', '303B42') if primary else ('1D2C37', '2C4655', '10222C', '172129')
    for prop, fill in zip(('normal', 'hovered', 'pressed', 'disabled'), fills):
        st.set_editor_property(prop, rounded(unreal.LinearColor.WHITE if swatch else color(fill),
                                          '94ECF0' if prop == 'hovered' else '405764' if not primary else fill,
                                          8, 2 if prop == 'hovered' else 1))
    for prop in ('normal_foreground', 'hovered_foreground', 'pressed_foreground'):
        st.set_editor_property(prop, unreal.SlateColor(specified_color=color('17212B') if primary else INK))
    st.set_editor_property('disabled_foreground', unreal.SlateColor(specified_color=MUTED))
    st.set_editor_property('normal_padding', unreal.Margin(10, 5, 10, 5))
    st.set_editor_property('pressed_padding', unreal.Margin(10, 7, 10, 3))
    w.set_style(st)
    w.set_background_color(unreal.LinearColor.WHITE)


class AppearanceLayout:
    def __init__(self, bp, parent):
        self.bp, self.parent = bp, parent
        self.font = unreal.load_asset(DEST + '/Fonts/F_JTS_UI_Regular_Font')
        self.bold = unreal.load_asset(DEST + '/Fonts/F_JTS_UI_Bold_Font')

    def add(self, cls, name, rect=None, parent=None):
        result = call('AddWidget', self.bp, cls, name, parent or self.parent, -1)
        w = result.widget
        assert w, name
        if rect:
            assert isinstance(w.slot, unreal.CanvasPanelSlot), name
            w.slot.set_anchors(unreal.Anchors())
            w.slot.set_alignment(unreal.Vector2D(0, 0))
            w.slot.set_auto_size(False)
            w.slot.set_position(unreal.Vector2D(rect[0], rect[1]))
            w.slot.set_size(unreal.Vector2D(rect[2], rect[3]))
        return w

    def text(self, name, value, rect=None, size=18, tint=None, bold=False, parent=None, center=False):
        w = self.add(unreal.TextBlock, name, rect, parent)
        w.set_text(value)
        w.set_font(unreal.SlateFontInfo(font_object=self.bold if bold else self.font, size=size))
        w.set_color_and_opacity(unreal.SlateColor(specified_color=tint or INK))
        w.set_auto_wrap_text(False)
        if center:
            w.set_editor_property('justification', unreal.TextJustify.CENTER)
        w.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        return w

    def panel(self, name, rect, fill='13212B', radius=12, outline='30424F'):
        w = self.add(unreal.Border, name, rect)
        w.set_brush(rounded(color(fill), outline, radius))
        w.set_padding(unreal.Margin(0))
        w.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        return w

    def button(self, name, label, action, rect, index=0, primary=False, swatch=False, size=18):
        w = self.add(unreal.load_class(None, '/Script/space.JTSMenuActionButton'), name, rect)
        w.set_editor_property('action', action)
        w.set_editor_property('index', index)
        w.set_editor_property('focus_sound', unreal.load_asset(DEST + '/Audio/S_UI_Focus'))
        w.set_editor_property('click_sound', unreal.load_asset(DEST + '/Audio/S_UI_Confirm'))
        style_button(w, primary, swatch)
        if label:
            self.text(name + '_Label', label, size=size, tint=color('17212B') if primary else INK,
                      bold=True, parent=w, center=True)
        return w

    def image(self, name, path, rect, parent=None):
        w = self.add(unreal.Image, name, rect, parent)
        w.set_brush_from_texture(unreal.load_asset(DEST + '/' + path))
        w.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        return w

    def section(self, index, title, y):
        self.text('SectionNumber' + str(index), '0' + str(index), (80, y+2, 32, 24), 14, CYAN, True)
        self.text('SectionTitle' + str(index), title, (120, y-2, 300, 32), 20, INK, True)


def polish(bp):
    widgets = {str(i.widget_name): i.widget for i in call('GetWidgets', bp).widgets}
    # Restyle existing menu actions without changing their routes, labels or layout.
    primary_actions = {'Create', 'Connect', 'ApplyAppearance', 'VideoApply', 'Launch', 'Ready', 'Confirm'}
    for w in widgets.values():
        if w.get_class().get_name() == 'JTSMenuActionButton':
            primary = str(w.get_editor_property('action')) in primary_actions or w.get_name() == 'Home_Saves'
            style_button(w, primary)
            stack = list(w.get_all_children())
            while stack:
                c = stack.pop()
                if isinstance(c, unreal.TextBlock):
                    c.set_color_and_opacity(unreal.SlateColor(specified_color=color('17212B') if primary else INK))
                if isinstance(c, unreal.PanelWidget):
                    stack.extend(c.get_all_children())
    parent = widgets['Appearance']
    # Remove descendants from the WidgetTree as well, so old duplicate names and actions cannot survive.
    children = list(parent.get_all_children())
    for child in children:
        call('RemoveWidget', bp, child)
    l = AppearanceLayout(bp, parent)
    l.panel('Appearance_Shadow', (32, 44, 840, 1004), '0A131C', 20, '0A131C')
    l.panel('Appearance_Plate', (40, 36, 824, 1000), '101D27', 18, '38505F')
    l.panel('Appearance_Accent', (80, 75, 32, 4), 'F7A65A', 2, 'F7A65A')
    l.text('Appearance_Kicker', 'CREW IDENTITY  /  01', (126, 64, 650, 28), 14, ORANGE, True)
    l.text('Appearance_Title', '你的宇航员', (80, 102, 720, 64), 36, INK, True)
    l.text('Appearance_Sub', '选一套装备，配上只属于你的颜色和表情。', (80, 169, 728, 30), 17, MUTED)
    l.section(1, '宇航服款式', 221)
    for i, (asset, label) in enumerate(zip(('Explorer', 'Scout', 'Engineer', 'Survey', 'Tactical'),
                                          ('探索者', '侦察员', '工程师', '测绘员', '战术型'))):
        b = l.button('Option_Suit' + str(i), '', 'Suit', (80+i*148, 264, 136, 126), i)
        c = l.add(unreal.CanvasPanel, 'SuitCanvas' + str(i), parent=b)
        l.image('SuitThumb' + str(i), 'UI/T_Thumb_' + asset, (18, 0, 80, 80), c)
        l.text('SuitName' + str(i), label, (0, 84, 116, 25), 16, bold=True, parent=c, center=True)
        b.set_tool_tip_text(label)
    l.section(2, '面部表情', 414)
    for action, label, values, tips, y in (
        ('Eyes', '眼睛', ('•  •', '^  ^', '−  −', '>  <', '★  ★'), ('圆点', '笑眼', '半眯', '锐角', '星星'), 458),
        ('Mouth', '嘴型', ('—', 'ω', '○', '⌒', 'vvv'), ('直线', '微笑', '惊讶', '委屈', '锯齿'), 528)):
        l.text(action + '_Caption', label, (80, y+14, 82, 30), 17, MUTED)
        for i, value in enumerate(values):
            b = l.button('Option_' + action + str(i), value, action, (180+i*128, y, 116, 54), i, size=25)
            b.set_tool_tip_text(tips[i])
    l.section(3, '分区配色', 608)
    for i, label in enumerate(('头盔', '上装', '下装')):
        b = l.button('ColorPart' + str(i), '', 'ColorPart', (80+i*248, 650, 236, 48), i)
        c = l.add(unreal.CanvasPanel, 'PartCanvas' + str(i), parent=b)
        chip = l.add(unreal.Border, 'PartColor' + str(i), (10, 7, 22, 22), c)
        chip.set_brush(rounded(unreal.LinearColor.WHITE, '8D9FAA', 5))
        chip.set_padding(unreal.Margin(0))
        chip.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        l.text('PartLabel' + str(i), label, (44, 4, 150, 30), 18, bold=True, parent=c)
    palette = unreal.load_asset(DEST + '/Data/DA_FE_AvatarCatalog').get_editor_property('colors')
    for i in range(min(len(palette), 20)):
        b = l.button('Swatch' + str(i), '', 'Swatch', (80+(i%10)*74, 714+(i//10)*42, 62, 32), i, swatch=True)
        b.set_tool_tip_text('预设色 %02d' % (i+1))
    for i, (key, label) in enumerate((('Hue', '色相'), ('Saturation', '饱和'), ('Brightness', '明度'))):
        y = 811+i*36
        l.text(key + '_Label', label, (80, y, 76, 28), 15, MUTED)
        bar = l.add(unreal.load_class(None, '/Script/space.JTSGradientBar'), key + 'Bar', (166, y+7, 570, 12))
        bar.set_editor_property('corner_radius', 6)
        bar.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        s = l.add(unreal.Slider, key + 'Slider', (166, y-1, 570, 28))
        st = s.get_editor_property('widget_style')
        for prop in ('normal_bar_image', 'hovered_bar_image', 'disabled_bar_image'):
            brush = unreal.SlateBrush()
            brush.set_editor_property('draw_as', unreal.SlateBrushDrawType.NO_DRAW_TYPE)
            st.set_editor_property(prop, brush)
        for prop in ('normal_thumb_image', 'hovered_thumb_image', 'disabled_thumb_image'):
            brush = rounded(INK, '101D27', 5, 2)
            image_size = brush.get_editor_property('image_size')
            image_size.set_editor_property('x', 10)
            image_size.set_editor_property('y', 24)
            brush.set_editor_property('image_size', image_size)
            st.set_editor_property(prop, brush)
        s.set_editor_property('widget_style', st)
        s.set_editor_property('mouse_uses_step', False)
        s.set_step_size(.01)
        l.text(key + 'Value', '', (750, y, 76, 28), 15, INK, center=True)
    l.panel('Appearance_Divider', (80, 923, 744, 1), '30424F', 0, '30424F')
    l.button('Appearance_Back', '返回', 'Back', (80, 944, 110, 52))
    l.button('Random', '随机一下', 'Random', (202, 944, 154, 52))
    l.button('Reset', '重置', 'Reset', (368, 944, 112, 52))
    l.button('ApplyAppearance', '应用并保存  →', 'ApplyAppearance', (548, 940, 276, 60), primary=True)
    l.text('Appearance_SaveHint', '返回取消修改 · 应用后保存外观', (80, 1006, 728, 22), 12, MUTED)
    l.panel('PreviewPlate', (1110, 944, 512, 64), '101D27', 14, '38505F')
    l.button('FullView', '全身预览', 'FullView', (1118, 952, 246, 48))
    l.button('HeadView', '面部特写', 'HeadView', (1374, 952, 240, 48))
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('selection_outline', CYAN)
    cdo.set_editor_property('selection_outline_width', 2)
    assert call('CompileWidgetBlueprint', bp), bp.get_path_name()
    assert EA.save_loaded_asset(bp)
    return len(call('GetWidgets', bp).widgets)


def configure_lobby_camera():
    stages = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
              if a.get_class().get_name() == 'JTSPreLaunchLobbyStage']
    assert stages, 'Load L_PreLaunchLobby before configuring its appearance camera'
    for stage in stages:
        stage.set_editor_property('appearance_full_view', unreal.Transform(
            location=unreal.Vector(650, 135, 50), rotation=unreal.Rotator(pitch=-4.5, yaw=180)))
        stage.set_editor_property('appearance_head_view', unreal.Transform(
            location=unreal.Vector(340, 70, 65), rotation=unreal.Rotator(pitch=-2.5, yaw=180)))
        stage.set_editor_property('appearance_fov', 50.0)
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return len(stages)


STEP = globals().get('STEP', 'polish')
if STEP == 'polish':
    REPORT['widgets'] = {n: polish(unreal.load_asset(DEST + '/Widgets/' + n)) for n in ('WBP_FE_Main', 'WBP_FE_Lobby')}
elif STEP == 'lobby_camera':
    REPORT['configured_lobby_stages'] = configure_lobby_camera()
elif STEP != 'library':
    raise ValueError(STEP)
