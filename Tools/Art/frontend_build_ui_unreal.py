"""Editor-owned UMG composition. Invoked through the project Unreal MCP toolset."""
import unreal
import json
import runpy
from pathlib import Path
REPORT_DIR=Path('D:/projects/space/Saved/Art/FrontEnd')
REPORT_DIR.mkdir(parents=True,exist_ok=True)

ROOT=Path(unreal.Paths.project_dir()).resolve()
DEST='/Game/Space/FrontEnd'
EA=unreal.EditorAssetLibrary
AT=unreal.AssetToolsHelpers.get_asset_tools()
APPEARANCE=runpy.run_path(str(ROOT/'Tools/Art/frontend_appearance_polish_unreal.py'),init_globals={'STEP':'library'})
UMG=unreal.get_default_object(unreal.UMGToolSet)
INK=unreal.LinearColor(.93,.91,.85,1)
MUTED=unreal.LinearColor(.52,.56,.58,1)
ORANGE=unreal.LinearColor(1,.29,.035,1)
BG=unreal.LinearColor(.012,.016,.022,.96)
def runtime_font(weight):
    name='F_JTS_UI_'+weight
    path=DEST+'/Fonts/'+name+'_Font'
    if not EA.does_asset_exist(path):
        task=unreal.AssetImportTask();task.set_editor_property('filename',str(ROOT/('SourceArt/FrontEnd/Fonts/JTSUISans-'+weight+'.otf')))
        task.set_editor_property('destination_path',DEST+'/Fonts');task.set_editor_property('destination_name',name)
        task.set_editor_property('automated',True);task.set_editor_property('save',True)
        factory=unreal.FontFileImportFactory();factory.set_editor_property('batch_create_font_asset',unreal.BatchCreateFontAsset.YES)
        task.set_editor_property('factory',factory);AT.import_asset_tasks([task])
    font=unreal.load_asset(path);assert font,path
    assert font.get_class().get_name()=='Font',font
    EA.save_loaded_asset(font);return font
FONT=runtime_font('Regular')
BOLD=runtime_font('Bold')
assert FONT and BOLD

def call(name,*args):return UMG.call_method(name,args)
def brush(tex=None,color=None,box=False):
    b=unreal.SlateBrush()
    if tex:b.set_editor_property('resource_object',tex)
    b.set_editor_property('tint_color',unreal.SlateColor(specified_color=color or unreal.LinearColor.WHITE))
    b.set_editor_property('draw_as',unreal.SlateBrushDrawType.BOX if box else unreal.SlateBrushDrawType.IMAGE)
    if box:b.set_editor_property('margin',unreal.Margin(.17,.17,.17,.17))
    return b

class Layout:
    def __init__(self,bp):self.bp=bp;self.counter=0
    def add(self,cls,name,parent=None,rect=None):
        result=call('AddWidget',self.bp,cls,name,parent,-1)
        w=result.get_editor_property('widget'); assert w,name
        slot=result.get_editor_property('slot')
        if rect and isinstance(slot,unreal.CanvasPanelSlot):
            slot.set_position(unreal.Vector2D(rect[0],rect[1]));slot.set_size(unreal.Vector2D(rect[2],rect[3]))
        return w
    def text(self,name,text,parent,rect,size=20,color=INK,bold=False):
        w=self.add(unreal.TextBlock,name,parent,rect);w.set_text(text)
        w.set_font(unreal.SlateFontInfo(font_object=BOLD if bold else FONT,size=size))
        w.set_color_and_opacity(unreal.SlateColor(specified_color=color));w.set_auto_wrap_text(True)
        w.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        return w
    def image(self,name,asset,parent,rect,color=None):
        w=self.add(unreal.Image,name,parent,rect);w.set_brush(brush(unreal.load_asset(DEST+'/'+asset),color))
        w.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE);return w
    def panel(self,name,parent,rect,color=BG):
        w=self.add(unreal.Border,name,parent,rect);w.set_brush_color(color);w.set_padding(unreal.Margin(0));return w
    def button(self,name,label,action,parent,rect,index=0,primary=False,text_name=None,icon=None):
        w=self.add(unreal.load_class(None,'/Script/space.JTSMenuActionButton'),name,parent,rect)
        w.set_editor_property('action',action);w.set_editor_property('index',index)
        w.set_editor_property('focus_sound',unreal.load_asset(DEST+'/Audio/S_UI_Focus'))
        w.set_editor_property('click_sound',unreal.load_asset(DEST+'/Audio/S_UI_Confirm'))
        APPEARANCE['style_button'](w,primary)
        content=self.add(unreal.HorizontalBox,name+'_Content',w)
        if icon:
            img=self.image(name+'_Icon','UI/T_Icon_'+icon.lower(),content,None)
            img.slot.set_size(unreal.SlateChildSize(size_rule=unreal.SlateSizeRule.AUTOMATIC));img.slot.set_padding(unreal.Margin(0,0,14,0))
        text=self.text(text_name or name+'_Label',label,content,None,20 if rect[3]<75 else 22,INK,True)
        text.set_auto_wrap_text(rect[3]>=75)
        text.slot.set_size(unreal.SlateChildSize(value=1,size_rule=unreal.SlateSizeRule.FILL))
        text.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        return w
    def field(self,name,hint,parent,rect,password=False):
        w=self.add(unreal.EditableTextBox,name,parent,rect);w.set_hint_text(hint);w.set_is_password(password)
        style=w.get_editor_property('widget_style')
        text_style=style.get_editor_property('text_style')
        text_style.set_editor_property('font',unreal.SlateFontInfo(font_object=FONT,size=20))
        style.set_editor_property('text_style',text_style)
        style.set_editor_property('foreground_color',unreal.SlateColor(specified_color=INK))
        style.set_editor_property('background_color',unreal.SlateColor(specified_color=unreal.LinearColor(.035,.045,.055,1)))
        w.set_editor_property('widget_style',style);return w
    def page(self,name,title,subtitle,width=760):
        p=self.add(unreal.CanvasPanel,'SettingsPage' if name=='Settings' else name,self.pages)
        self.panel(name+'_Plate',p,(0,0,width,1080))
        self.image(name+'_Stripe','UI/T_UI_WarningStripe',p,(width-8,0,8,1080))
        self.text(name+'_Kicker','JUMP TO SPACE  /  EARTH LAUNCH BASE',p,(56,40,width-112,28),14,ORANGE,True)
        self.text(name+'_Title',title,p,(56,98,width-112,72),38,INK,True)
        self.text(name+'_Sub',subtitle,p,(56,176,width-112,70),17,MUTED)
        if name not in ['Home','Lobby']:self.button(name+'_Back','返回','Back',p,(56,966,160,54),icon='Back')
        return p

def make_catalog():
    path=DEST+'/Data/DA_FE_AvatarCatalog'
    data=unreal.load_asset(path)
    if not data:
        factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.load_class(None,'/Script/space.JTSAvatarCatalog'))
        data=AT.create_asset('DA_FE_AvatarCatalog',DEST+'/Data',unreal.DataAsset,factory)
    data.set_editor_property('suits',[unreal.load_asset(DEST+'/Characters/SK_FE_'+n) for n in ['Explorer','Scout','Engineer','Survey','Tactical']])
    if not data.get_editor_property('colors'):
        data.set_editor_property('colors',[unreal.LinearColor(*c) for c in [(.82,.80,.72,1),(1,.26,.035,1),(.06,.24,.55,1),(.10,.39,.24,1),(.31,.12,.5,1)]])
    data.set_editor_property('preview_idle',unreal.load_asset(DEST+'/Animations/A_FE_Idle'))
    data.set_editor_property('suit_thumbnails',[unreal.load_asset(DEST+'/UI/T_Thumb_'+n) for n in ['Explorer','Scout','Engineer','Survey','Tactical']])
    EA.save_loaded_asset(data);return data

def build():
    catalog=make_catalog()
    path=DEST+'/Widgets/WBP_FE_Main'
    if EA.does_asset_exist(path):EA.delete_asset(path)
    bp=call('CreateWidgetBlueprint',DEST+'/Widgets','WBP_FE_Main',unreal.load_class(None,'/Script/space.JTSFrontEndShellWidget'))
    assert bp
    l=Layout(bp)
    safe=l.add(unreal.SafeZone,'SafeArea')
    scale=l.add(unreal.ScaleBox,'ResponsiveFrame',safe);scale.set_stretch(unreal.Stretch.SCALE_TO_FIT)
    size=l.add(unreal.SizeBox,'DesignSize',scale);size.set_width_override(1920);size.set_height_override(1080)
    root=l.add(unreal.CanvasPanel,'Root',size)
    l.pages=l.add(unreal.WidgetSwitcher,'FrontEndPages',root,(0,0,1920,1080))
    home=l.page('Home','准备好一起出发了吗？','1–4 人共享远征 · 从地球出发，探索真实球面星球')
    l.image('Logo','UI/T_Logo_Full',home,(52,255,500,136))
    for n,(title,action,icon) in enumerate([('开始 / 继续远征','Saves','Ship'),('加入远征队','Join','Network'),('设置','Settings','Settings'),('制作与素材来源','Credits','Info'),('退出游戏','Quit','Close')]):
        l.button('Home_'+action,title,action,home,(56,424+n*78,560,62),primary=n==0,icon=icon)
    l.text('HomeFoot','同一艘飞船，同一段旅程。',home,(56,964,560,40),16,MUTED)
    saves=l.page('Saves','选择远征存档','存档保存在房主设备上。继续远征也会先进入组队大厅。',850)
    for i in range(1,5):
        l.button('Save'+str(i),'空存档','Slot',saves,(56,270+(i-1)*156,620,128),i,primary=False,text_name='SlotLabel'+str(i))
        l.button('Delete'+str(i),'删除','Delete',saves,(690,304+(i-1)*156,100,56),i)
    setup=l.page('Setup','组建远征队','人数上限与隐私设置只影响本次房间，不会改动原存档进度。',850)
    l.text('SetupTitle','新远征 · 组建队伍',setup,(56,250,730,40),24,ORANGE,True)
    l.text('NameCaption','远征名称',setup,(56,302,300,30),17,MUTED)
    l.field('ExpeditionName','为远征命名（最多 40 字）',setup,(56,340,720,56))
    l.button('Public','公开房间','Privacy',setup,(56,420,350,56),0,True)
    l.button('Private','私密房间','Privacy',setup,(426,420,350,56),1)
    l.text('PrivacyLabel','公开 · 显示在远征队列表',setup,(56,487,720,32),17,MUTED)
    l.text('PlayersCaption','人数上限',setup,(56,542,220,30),18)
    l.button('FewerPlayers','−','Players',setup,(426,528,80,56),-1)
    l.text('PlayerLimit','4 / 4',setup,(532,540,128,40),24)
    l.button('MorePlayers','+','Players',setup,(696,528,80,56),1)
    l.field('HostPassword','房间密码（可留空）',setup,(56,620,720,56),True)
    l.field('CustomCode','房间码（留空自动生成）',setup,(56,700,720,56))
    l.button('CreateCrew','创建并进入大厅','Create',setup,(56,808,720,72),primary=True)
    join=l.page('Join','加入远征队','搜索公开房间，或输入好友提供的房间码。出发后停止加入。',1000)
    l.field('JoinCode','房间码（留空浏览公开队伍）',join,(56,255,640,54))
    l.button('SearchCrews','搜索 / 刷新','Search',join,(718,255,226,54),primary=True)
    l.text('ListingStatus','尚未搜索',join,(56,329,888,30),17,MUTED)
    for i in range(4):l.button('Listing'+str(i),'暂无可加入的远征队','Listing',join,(56,378+i*110,888,92),i,text_name='ListingLabel'+str(i))
    l.button('PreviousListings','上一页','ListingPage',join,(56,828,140,44),-1)
    l.button('NextListings','下一页','ListingPage',join,(210,828,140,44),1)
    l.field('JoinPassword','所选房间的密码（如有）',join,(382,828,562,48),True)
    l.button('Connect','加入所选队伍','Connect',join,(642,942,302,64),primary=True)
    appearance=l.page('Appearance','你的宇航员','眼睛、嘴型、款式与三段配色可独立设置。应用后保存。',920)
    for row,(action,label,choices) in enumerate([
        ('Eyes','眼睛',['圆点','笑眼','半眯','锐角','星星']),('Mouth','嘴型',['直线','微笑','惊讶','委屈','锯齿']),
        ('Suit','宇航服款式',['探索者','侦察员','工程师','测绘员','战术型']),
        ('Helmet','头盔配色',['象牙白','航天橙','深空蓝','苔绿','星云紫']),('Upper','上装配色',['象牙白','航天橙','深空蓝','苔绿','星云紫']),('Lower','下装配色',['象牙白','航天橙','深空蓝','苔绿','星云紫'])]):
        y=264+row*98;l.text(action+'_Caption',label,appearance,(56,y,800,26),17,MUTED)
        for i,choice in enumerate(choices):l.button('Option_'+action+str(i),choice,action,appearance,(56+i*166,y+32,152,52),i)
    l.button('Random','随机一下','Random',appearance,(56,878,186,58))
    l.button('Reset','默认外观','Reset',appearance,(258,878,186,58))
    l.button('ApplyAppearance','应用并保存','ApplyAppearance',appearance,(614,952,250,64),primary=True)
    l.button('HeadView','面部特写','HeadView',appearance,(1040,920,200,54))
    l.button('FullView','全身预览','FullView',appearance,(1260,920,200,54))
    settings=l.page('Settings','设置','显示设置需确认；声音、操作与辅助偏好会自动保存在本机。',1250)
    labels=[('Window','显示模式'),('Resolution','分辨率'),('Quality','画面质量'),('Vsync','垂直同步'),('FPS','帧率上限'),('Master','主音量'),('Music','音乐'),('Effects','音效'),('Voice','语音音量'),('Mic','麦克风'),('Input','输入设备'),('Output','输出设备'),('Sensitivity','鼠标灵敏度'),('Invert','垂直反转'),('UIScale','界面缩放'),('Motion','背景动画')]
    for n,(setting,label) in enumerate(labels):
        column=n//8;row=n%8;x=56+column*600;y=264+row*70
        l.text('Caption_'+setting,label,settings,(x,y+8,176,42),18)
        l.button('Decrease_'+setting,'‹',setting,settings,(x+182,y,48,48),-1)
        l.text('Value_'+setting,'',settings,(x+242,y+8,230,44),17,ORANGE)
        l.button('Increase_'+setting,'›',setting,settings,(x+490,y,48,48),1)
    l.text('VideoCountdown','',settings,(56,846,1120,32),18,ORANGE)
    l.button('ApplyVideo','应用显示设置','VideoApply',settings,(56,895,252,56),primary=True)
    l.button('KeepVideo','保留','VideoKeep',settings,(328,895,170,56))
    l.button('RevertVideo','恢复','VideoRevert',settings,(518,895,170,56))
    l.text('InputGuide','操作：WASD 移动 · 鼠标观察 · 空格跳跃 · E 交互\n菜单：方向键 / Tab 选择 · Enter 确认 · Esc / 手柄 B 返回',settings,(734,887,460,100),16,MUTED)
    credits=l.page('Credits','制作与素材来源','Jump to Space · 四人合作远征',1120)
    l.text('CreditsBody','原创素材\n五套宇航服、独立面部图案、基地模块、道具、图标、界面、动画与音频均通过 Blender / Unreal MCP 制作。\n\n项目素材\nTwinfork R1 飞船：基于项目原始模型制作展示用副本，重新整理网格、倒角和配色。\n\n开放素材与改编\nPoly Haven · Hangar Concrete Floor / Dimitrios Savva · CC0\n已调整色调、对比、法线强度、粗糙度与 AO。\nAdobe Source Han Sans SC 2.005R · SIL OFL 1.1\n改编为 JTS UI Sans，调整数字与标点字宽，保留完整中文覆盖。\nQuaternius Ultimate Modular Characters · CC0\n仅参考骨架兼容性，角色网格、权重与动画为原创。\n\n完整来源、许可文本、修改记录和 SHA256：SourceArt/FrontEnd/asset_manifest.json',credits,(56,260,1000,650),21)
    connecting=l.page('Connecting','正在连接远征队','正在与联机服务建立连接。完成后自动进入组队大厅。',850)
    l.image('LoadingIllustration','UI/T_Illus_LoadingShip',connecting,(180,280,460,460))
    l.text('ConnectionHint','请稍候…\n连接失败时会返回刚才的页面，并保留填写内容。',connecting,(56,772,730,100),22,MUTED)
    lobby=l.page('Lobby','远征队 · 出发准备','成员外观在服务器校验后同步。所有队员准备后，由房主出发。',1920)
    # Keep the center transparent so the four live avatars remain visible.
    plate=next(w.widget for w in call('GetWidgets',bp).widgets if str(w.widget_name)=='Lobby_Plate')
    plate.set_brush_color(unreal.LinearColor(.008,.012,.018,.12))
    l.text('LobbyCode','',lobby,(56,220,1500,42),24,ORANGE,True)
    l.button('CopyCode','复制房间码','Copy',lobby,(1610,210,254,56))
    for i in range(4):
        x=70+i*460
        p=l.panel('CrewPanel'+str(i),lobby,(x,735,400,146),unreal.LinearColor(.02,.027,.034,.9))
        c=l.add(unreal.CanvasPanel,'CrewCanvas'+str(i),p)
        l.text('CrewLabel'+str(i),'等待队员',c,(20,15,360,80),22,INK,True)
        l.button('MuteCrew'+str(i),'静音','MutePlayer',c,(18,98,164,38),i)
        l.button('KickCrew'+str(i),'移出','Kick',c,(202,98,180,38),i)
    l.text('LobbyHint','',lobby,(56,900,1450,34),20,ORANGE)
    for n,(label,action) in enumerate([('离开队伍','Leave'),('设置','Settings'),('关闭加入','CloseJoining'),('麦克风','Mute'),('准备就绪','Ready'),('出发','Launch')]):
        l.button('Lobby_'+action,label,action,lobby,(56+n*304,974,284,64),primary=action in ['Ready','Launch'],text_name='ReadyLabel' if action=='Ready' else 'MicLabel' if action=='Mute' else None)
    l.text('ProviderStatus','',root,(1530,36,334,36),14,MUTED)
    l.text('StatusLine','',root,(840,90,1000,100),19,ORANGE,True)
    confirm=l.panel('ConfirmationLayer',root,(0,0,1920,1080),unreal.LinearColor(0,0,0,.78))
    cc=l.add(unreal.CanvasPanel,'ConfirmationCanvas',confirm)
    l.panel('ConfirmationCard',cc,(510,360,900,360),unreal.LinearColor(.025,.035,.045,1))
    l.text('ConfirmationHeading','请确认',cc,(562,400,790,50),30,INK,True)
    l.text('ConfirmationMessage','',cc,(562,478,790,100),22)
    l.button('CancelConfirmation','取消','Cancel',cc,(562,620,370,60))
    l.button('AcceptConfirmation','确认','Confirm',cc,(962,620,390,60),primary=True)
    confirm.set_visibility(unreal.SlateVisibility.COLLAPSED)
    assert call('CompileWidgetBlueprint',bp)
    defaults=unreal.get_default_object(bp.generated_class());defaults.set_editor_property('avatar_catalog',catalog);defaults.set_editor_property('lobby',False)
    EA.save_loaded_asset(bp,only_if_is_dirty=False)
    lobby_path=DEST+'/Widgets/WBP_FE_Lobby'
    if EA.does_asset_exist(lobby_path):EA.delete_asset(lobby_path)
    lobby_bp=EA.duplicate_asset(path,lobby_path)
    for item in call('GetWidgets',lobby_bp).widgets:
        if str(item.widget_name)=='ProviderStatus':item.widget.slot.set_position(unreal.Vector2D(1060,36))
    assert call('CompileWidgetBlueprint',lobby_bp)
    unreal.get_default_object(lobby_bp.generated_class()).set_editor_property('lobby',True)
    EA.save_loaded_asset(lobby_bp,only_if_is_dirty=False)
    return {'success':True,'widgets':len(call('GetWidgets',bp).widgets),'blueprints':[path,lobby_path],'catalog':catalog.get_path_name()}

REPORT=build()
# Keep the editor-owned appearance pass when rebuilding the complete menu.
REPORT['appearance'] = runpy.run_path(str(ROOT/'Tools/Art/frontend_appearance_polish_unreal.py'),
                                    init_globals={'STEP': 'polish'}).get('REPORT', {})
REPORT['avatar_interaction'] = runpy.run_path(str(ROOT/'Tools/Art/frontend_avatar_interaction_unreal.py')).get('REPORT', {})
(REPORT_DIR/'ui_build_report.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
