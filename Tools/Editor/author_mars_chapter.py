"""Mars chapter content recipes. All placement, IDs and asset choices live here/UE assets."""
import math
import unreal as u

ROOT='/Game/Space'
BP=ROOT+'/Blueprints/Planets/Mars/Chapter'
DATA=ROOT+'/Data/Planets/Mars'
ART=ROOT+'/Environment/MarsSystem/Chapter'

def author():
    sub=u.get_editor_subsystem(u.EditorActorSubsystem)
    assets=u.AssetToolsHelpers.get_asset_tools()
    actors={a.get_actor_label():a for a in sub.get_all_level_actors()}
    planets={str(a.get_planet_id()):a for a in actors.values() if isinstance(a,u.JTSPlanetAnchor)}
    saved=[]
    from pathlib import Path
    artroot=Path(u.Paths.project_dir()).resolve()/'SourceArt/MarsSystem'
    def import_file(file,destination,name):
        existing=u.load_asset(destination+'/'+name)
        if existing:return existing
        if not file.exists():return None
        t=u.AssetImportTask();setp(t,filename=str(file),destination_path=destination,destination_name=name,automated=True,save=True,replace_existing=False)
        assets.import_asset_tasks([t])
        return u.load_asset(destination+'/'+name)
    def setp(obj,**kw):
        for k,v in kw.items():obj.set_editor_property(k,v)
        return obj
    def save(obj):
        u.EditorAssetLibrary.save_loaded_asset(obj,False); saved.append(obj.get_path_name()); return obj
    photos=[import_file(artroot/'References'/('T_Deimos_Dust_0'+str(i)+'.png'),ART+'/Narrative','T_Deimos_Dust_0'+str(i)) for i in range(1,4)]
    cage_audio=import_file(artroot/'Audio/S_Mars_Cage.wav',ART+'/Audio','S_Mars_Cage')
    attenuation=u.load_asset(ART+'/Audio/SA_Mars_Room')
    if not attenuation:attenuation=assets.create_asset('SA_Mars_Room',ART+'/Audio',u.SoundAttenuation,u.SoundAttenuationFactory())
    attenuation_config=u.SoundAttenuationSettings()
    setp(attenuation_config,attenuate=True,spatialize=True,attenuation_shape=u.AttenuationShape.SPHERE,attenuation_shape_extents=u.Vector(700,0,0),falloff_distance=1600,enable_occlusion=True,occlusion_volume_attenuation=.25)
    setp(attenuation,attenuation=attenuation_config);save(attenuation)
    audio={}
    for name in ('UndergroundHum','Furnace','MothFlutter','IceCrack','CameraOff'):
        sound=import_file(artroot/('Audio/S_Mars_'+name+'.wav'),ART+'/Audio','S_Mars_'+name)
        setp(sound,looping=name in ('UndergroundHum','Furnace','MothFlutter'),attenuation_settings=attenuation);save(sound);audio[name]=sound
    def material(name,color,glow=0):
        path=ART+'/Materials/'+name
        m=u.load_asset(path)
        if not m:
            m=assets.create_asset(name,ART+'/Materials',u.Material,u.MaterialFactoryNew())
            c=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionConstant3Vector,-300,0)
            setp(c,constant=u.LinearColor(*color,1))
            u.MaterialEditingLibrary.connect_material_property(c,'',u.MaterialProperty.MP_BASE_COLOR)
            if glow:u.MaterialEditingLibrary.connect_material_property(c,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
            r=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionConstant,-300,180)
            setp(r,r=.8); u.MaterialEditingLibrary.connect_material_property(r,'',u.MaterialProperty.MP_ROUGHNESS)
            setp(m,two_sided=True); u.MaterialEditingLibrary.recompile_material(m)
        return save(m)
    mats={k:material('M_Mars_'+k,c,k=='Lamp') for k,c in {
        'White':(.93,.925,.91),'Orange':(.95,.416,.106),'Dark':(.075,.085,.10),
        'Wall':(.25,.22,.20),'Rib':(.604,.306,.212),'Ice':(.81,.89,.92),
        'Woodlouse':(.557,.541,.525),'Moth':(.804,.75,.64),'Worm':(.69,.48,.408),
        'Gold':(.95,.85,.65),'Lamp':(1,.85,.63),'Alloy':(.55,.65,.72),
        'Panel':(.035,.09,.18),'Dust':(.55,.38,.26)}.items()}
    cube=u.load_asset('/Engine/BasicShapes/Cube'); sphere=u.load_asset('/Engine/BasicShapes/Sphere'); cylinder=u.load_asset('/Engine/BasicShapes/Cylinder')
    stationroot=artroot.parent/'MarsStation'
    domeparts=[import_file(stationroot/(name+'.fbx'),ROOT+'/Environment/MarsSystem/Mars/Station',name) for name in ('SM_Mars_EcologyDome_Cushions','SM_Mars_EcologyDome_Ribs')]
    for m in (mats['Moth'],):
        setp(m,used_with_instanced_static_meshes=True);u.MaterialEditingLibrary.recompile_material(m);save(m)
    etfe=material('M_Mars_ETFE',(.82,.86,.78),1)
    setp(etfe,blend_mode=u.BlendMode.BLEND_TRANSLUCENT,two_sided=True)
    opacity=next((e for e in u.ObjectIterator(u.MaterialExpressionConstant) if e.get_outer()==etfe and abs(e.get_editor_property('r')-.24)<.001),None)
    if not opacity:opacity=u.MaterialEditingLibrary.create_material_expression(etfe,u.MaterialExpressionConstant,-300,300);setp(opacity,r=.24)
    u.MaterialEditingLibrary.connect_material_property(opacity,'',u.MaterialProperty.MP_OPACITY);u.MaterialEditingLibrary.recompile_material(etfe);save(etfe)
    def blueprint(name,parent,config=None,source=None):
        path=BP+'/'+name; b=u.load_asset(path)
        if not b:
            if source:b=assets.duplicate_asset(name,BP,u.load_asset(source))
            else:
                f=u.BlueprintFactory(); setp(f,parent_class=parent); b=assets.create_asset(name,BP,u.Blueprint,f)
        c=u.get_default_object(b.generated_class())
        if config:config(c)
        u.BlueprintEditorLibrary.compile_blueprint(b); save(b)
        return b.generated_class()
    controllers={}
    for pid in ('Mars','MarI','MarII'):
        controllers[pid]=blueprint('BP_'+pid+'_SurfaceGameplay',u.JTSPlanetSurfaceController,lambda c,pid=pid:setp(c,planet_id=pid))
    gmasset=u.load_asset(ROOT+'/Blueprints/Modes/BP_SpaceWorldGameMode'); gm=u.get_default_object(gmasset.generated_class())
    definitions=gm.get_editor_property('surface_gameplay_controllers')
    for index,d in enumerate(definitions):
        pid=str(d.get_editor_property('planet_id'))
        if pid in controllers:
            replacement=u.JTSSurfaceGameplayControllerDefinition()
            assert replacement.import_text('(PlanetId="%s",ControllerClass="%s")' % (pid,controllers[pid].get_path_name()))
            definitions[index]=replacement
    setp(gm,surface_gameplay_controllers=definitions); u.BlueprintEditorLibrary.compile_blueprint(gmasset); save(gmasset)
    for ship_path in (ROOT+'/Blueprints/Ships/BP_Spacecraft',ROOT+'/Ships/TwinforkR1/BP_Twinfork_R1'):
        shipbp=u.load_asset(ship_path); ship=u.get_default_object(shipbp.generated_class())
        dock=ship.get_component_by_class(u.JTSShipDockComponent)
        setp(dock,initial_installed_ids=[v for v in dock.get_editor_property('initial_installed_ids') if str(v)!='StellarExchange'])
        hull=ship.get_component_by_class(u.JTSSpacecraftDurabilityComponent)
        setp(hull,maximum_hull=600,reconstruction_costs={u.JTSResourceType.ROCK:20,u.JTSResourceType.ORE:10})
        u.BlueprintEditorLibrary.compile_blueprint(shipbp);save(shipbp)
    # Shared asset definitions preserve enum values and use the existing resource capability.
    for iid,name,desc,mat in [('ManufacturedPart','人造构件','从人类设备残骸中回收的可用零件。','Orange'),('HighGradeAlloy','高品位合金','从火星母机核心中回收的高强度合金。','Alloy')]:
        path=ROOT+'/Data/Items/DA_Item_'+iid; d=u.load_asset(path)
        if not d:
            f=u.DataAssetFactory();setp(f,data_asset_class=u.JTSItemDefinition);d=assets.create_asset('DA_Item_'+iid,ROOT+'/Data/Items',u.JTSItemDefinition,f)
        enum=getattr(u.JTSItemId,{'ManufacturedPart':'MANUFACTURED_PART','HighGradeAlloy':'HIGH_GRADE_ALLOY'}[iid])
        setp(d,item_id=enum,display_name=name,description=desc,primary_category=u.JTSItemCategory.RESOURCES,capability_mask=32,max_stack_size=1,world_pickup_mesh=cube,accent_color=u.LinearColor(1,.5,.2,1));save(d)
    def key_config(c,i,title,rewards=None):
        setp(c,important_item_id=i,pickup_notice=title)
        mesh=c.get_component_by_class(u.StaticMeshComponent);mesh.set_static_mesh(cube);mesh.set_relative_scale3d(u.Vector(.36,.22,.05));mesh.set_material(0,mats['Orange'])
        if rewards:setp(c,resource_reward=rewards)
    keys={i:blueprint('BP_Key_'+i,u.JTSExpeditionKeyPickup,lambda c,i=i,t=t:key_config(c,i,t,{u.JTSResourceType.HIGH_GRADE_ALLOY:20} if i=='Mars_LogisticsChip' else None)) for i,t in
          [('Mars_Badge_B1','获得菌丝工坊主管工牌'),('Mars_Badge_B2','获得授粉温室主管工牌'),('Mars_Badge_B3','获得鱼菜共生池主管工牌'),('Mars_Badge_B4','获得土壤工厂主管工牌'),('Mars_TrackingCollar','追踪项圈：夜之迷宫坐标已装载'),('Mars_LogisticsChip','物流芯片：土卫六坐标已装载；高品位合金已入库')]}
    def actor(cls,label,pos,rot=None,parent=None):
        label='MC_'+label
        a=actors.get(label)
        if not a:a=sub.spawn_actor_from_class(cls,pos,rot or u.Rotator());actors[label]=a
        a.set_actor_label(label);a.set_folder_path('MarsChapter/'+(str(parent.get_planet_id()) if isinstance(parent,u.JTSPlanetAnchor) else 'Content'))
        a.set_actor_location(pos,False,False)
        a.set_actor_scale3d(u.Vector(1,1,1))
        if rot:a.set_actor_rotation(rot,False)
        if parent:
            target=parent.get_editor_property('gameplay_surface_actor') if isinstance(parent,u.JTSPlanetAnchor) and not a.get_editor_property('replicates') else parent
            a.attach_to_actor(target,'',u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,False)
        return a
    def block(label,p,rot,dim,mat='Wall',parent=None,mesh=None):
        a=actor(u.StaticMeshActor,label,p,rot,parent);c=a.static_mesh_component
        c.set_mobility(u.ComponentMobility.MOVABLE);c.set_static_mesh(mesh or cube);c.set_material(0,mats[mat]);c.set_collision_profile_name('BlockAll');c.set_collision_object_type(u.CollisionChannel.ECC_WORLD_STATIC)
        a.set_actor_scale3d(u.Vector(*(v/100 for v in dim)));return a
    def text(label,p,rot,value,parent,size=60):
        a=actor(u.TextRenderActor,label,p,rot,parent);c=a.get_component_by_class(u.TextRenderComponent); c.set_text(value);c.set_world_size(size);c.set_text_render_color(u.Color(242,180,90,255));return a
    def frame_at(p,origin):
        f=p.get_surface_frame_at(origin,u.Vector(1,0,0));return f
    station=actors['Mars_Site_Station'].get_actor_location();noctis=actors['Mars_Site_Noctis'].get_actor_location()
    frames={'Mars':frame_at(planets['Mars'],station),'Noctis':frame_at(planets['Mars'],noctis)}
    for pid in ('MarI','MarII'):frames[pid]=frame_at(planets[pid],planets[pid].get_editor_property('landing_anchor_actor').get_actor_location())
    def basis(k):
        f=frames[k];return f.location,f.forward,f.right,f.up,f.transform.rotation.rotator()
    def point(k,x=0,y=0,z=0,surface=False):
        o,f,r,up,rot=basis(k);p=o+f*x+r*y+up*z
        if surface:
            planet=planets['Mars' if k=='Noctis' else k];hit=planet.project_point_to_surface(p)
            if not hit:raise RuntimeError('No real mesh hit '+str((k,x,y,z)))
            p=hit.impact_point+planet.get_radial_up_vector(hit.impact_point)*z
        return p
    def placed(cls,label,k,xyz=(0,0,0),**props):
        p=planets['Mars' if k=='Noctis' else k];a=actor(cls,label,point(k,*xyz),basis(k)[4],p);setp(a,**props);return a
    def conditions(a,ids,source='Flag',n=0):
        c=a.get_component_by_class(u.JTSFacilityConditionComponent)
        req=[]
        for item in ids:
            kind,key=item if isinstance(item,tuple) else (source,item)
            r=u.JTSFacilityRequirement();setp(r,source=getattr(u.JTSFacilityFact,{'Flag':'FLAG','ImportantItem':'IMPORTANT_ITEM','BlockedEntrance':'BLOCKED_ENTRANCE','DefeatedEncounter':'DEFEATED_ENCOUNTER','Coordinate':'COORDINATE'}[kind]),id=key);req.append(r)
        setp(c,requirements=req,mode=u.JTSConditionMode.AT_LEAST if n else u.JTSConditionMode.ALL,minimum_satisfied=n or 1)
    def facility(label,k,xyz,flag,title,requires=(),auto=False,hide=False,dim=(100,70,150),cls=None,n=0,source='Flag'):
        a=placed(cls or u.JTSFacilityController,label,k,xyz,completed_flag=flag,display_name=title,automatic=auto,hide_mesh_when_active=hide,repair_seconds=3)
        a.get_component_by_class(u.StaticMeshComponent).set_static_mesh(cube);a.set_actor_scale3d(u.Vector(*(v/100 for v in dim)));a.get_component_by_class(u.StaticMeshComponent).set_material(0,mats['Orange']);conditions(a,requires,source,n);return a
    rooms={}; floors={};lights=[]
    def room(label,k,xyz,size=(2000,2800,600),doors=('N',),title=''):
        p=planets['Mars' if k=='Noctis' else k];center=point(k,*xyz);rot=basis(k)[4];f,r,up=basis(k)[1:4]
        w,d,h=size
        zone=actor(u.JTSInteriorZone,label+'_Zone',center+up*h/2,rot,p);setp(zone,zone_id=label,planet=p,display_name=title or label);zone.bounds.set_box_extent(u.Vector(w/2,d/2,h/2+30))
        floor=block(label+'_Floor',center-up*20,rot,(w,d,40),parent=p)
        block(label+'_Roof',center+up*(h+20),rot,(w+80,d+80,40),'Dark',p)
        # Six-face shell, with actual 4m door openings on selected edges.
        for edge,axis,span,pos in [('N',r,w,center+r*d/2),('S',r,w,center-r*d/2),('E',f,d,center+f*w/2),('W',f,d,center-f*w/2)]:
            dims=(w,40,h) if edge in ('N','S') else (40,d,h)
            if edge not in doors:block(label+'_Wall_'+edge,pos+up*h/2,rot,dims,'Wall',p)
            else:
                side=f if edge in ('N','S') else r
                for sign in (-1,1):
                    length=(span-400)/2;dim=(length,40,h) if edge in ('N','S') else (40,length,h)
                    jamb=label+'_Door_'+edge+str(sign)
                    if length>0:
                        block(jamb,pos+side*(sign*(200+length/2))+up*h/2,rot,dim,'Wall',p)
                    elif 'MC_'+jamb in actors:
                        sub.destroy_actor(actors.pop('MC_'+jamb))
                dim=(400,40,max(30,h-400)) if edge in ('N','S') else (40,400,max(30,h-400))
                block(label+'_DoorHeader_'+edge,pos+up*(400+(h-400)/2),rot,dim,'White',p)
        setp(zone,floor_actors=[floor]);rooms[label]=zone;floors[label]=floor
        light=actor(u.PointLight,label+'_Light',center+up*(h-90),rot,p);light.point_light_component.set_mobility(u.ComponentMobility.MOVABLE);light.point_light_component.set_intensity(max(6500,w*d*.001));light.point_light_component.set_cast_shadows(False);light.point_light_component.set_attenuation_radius(max(w,d)*.75);light.point_light_component.set_light_color(u.LinearColor(1,.8,.6,1));lights.append(light)
        text(label+'_Sign',center-f*(w/2-50)+up*180,rot,title or label,p,38)
        return zone
    def marker(label,k,xyz):return placed(u.TargetPoint,label,k,xyz)
    import runpy
    outline=runpy.run_path(str(Path(u.Paths.project_dir()).resolve()/'Tools/Editor/mars_chapter_materials.py'))['encounter_outline']()
    pp=actor(u.PostProcessVolume,'EncounterOutline',station)
    setp(pp,unbound=True);pp.add_or_update_blendable(outline,1)
    def transit(label,k,xyz,dest,title,req=(),ride=0):
        a=placed(u.JTSInteriorTransit,label,k,xyz,destination=dest,display_name=title,ride_seconds=ride)
        a.get_component_by_class(u.StaticMeshComponent).set_static_mesh(cube);a.set_actor_scale3d(u.Vector(.7,.45,1.3));a.get_component_by_class(u.StaticMeshComponent).set_material(0,mats['Orange']);conditions(a,req);return a
    # Surface station: projected independent equipment, no changes to the planet asset.
    for i,(x,y) in enumerate(((-4900,3800),(0,6200),(4900,3800))):
        for j in range(12):
            xx=x+(j%4-1.5)*650;yy=y+(j//4-1)*1200;p=point('Mars',xx,yy,140,True)
            block('Solar_'+str(i)+'_'+str(j),p,basis('Mars')[4],(550,1100,35),'Panel',planets['Mars'])
        box=facility('Sector_'+chr(65+i),'Mars',(x,y-2200,100),'Mars_Sector_'+chr(65+i)+'_Repaired','修复 '+chr(65+i)+' 区汇流箱')
        setp(box,repair_costs={u.JTSResourceType.ORE:6,u.JTSResourceType.CRYSTAL:2},show_progress_on_hud=True,followup_flag='Mars_Sector_'+chr(65+i)+'_Clean')
        box.set_actor_location(point('Mars',x,y-2200,100,True),False,False)
    old=actors.get('MC_DustSweep')
    if old:sub.destroy_actor(old);actors.pop('MC_DustSweep')
    for i,(x,y) in enumerate(((-4900,3800),(0,6200),(4900,3800))):
        flag='Mars_Sector_'+chr(65+i)
        dust=placed(u.JTSFacilityHazard,'DustSweep_'+chr(65+i),'Mars',(x,y,180),kind=u.JTSFacilityHazardKind.DUST_SWEEP,warning_seconds=120+i*20,recovery_seconds=15,sweep_flags=[flag+'_Clean'])
        dust.bounds.set_box_extent(u.Vector(1600,2100,500));conditions(dust,[flag+'_Repaired'])
        dust.visual.set_static_mesh(sphere);dust.visual.set_relative_scale3d(u.Vector(6,6,20));dust.visual.set_material(0,mats['Dust'])
    power=facility('Power','Mars',(0,-1400,100),'Mars_Power','站内供电',requires=['Mars_Sector_'+s+'_'+state for s in 'ABC' for state in ('Repaired','Clean')],auto=True)
    for label in list(actors):
        if label.startswith(('MC_EcologyDome_1','MC_EcologyDome_2','MC_DomeRib_1_','MC_DomeRib_2_')):
            sub.destroy_actor(actors.pop(label))
    if all(domeparts):
        for label in list(actors):
            if label=='MC_EcologyDome_0' or label.startswith('MC_DomeRib_0_'):sub.destroy_actor(actors.pop(label))
        for i,mesh in enumerate(domeparts):
            a=actor(u.StaticMeshActor,'DomeModule_'+str(i),point('Mars',0,-3900,0,True),basis('Mars')[4],planets['Mars']);a.static_mesh_component.set_static_mesh(mesh);a.static_mesh_component.set_material(0,etfe if i==0 else mats['Rib']);a.set_actor_enable_collision(False)
        glow=placed(u.PointLight,'Dome_Lantern','Mars',(0,-3900,1000));glow.point_light_component.set_intensity(12000);glow.point_light_component.set_attenuation_radius(2600);glow.point_light_component.set_light_color(u.LinearColor(1,.8,.6,1))
    for label,k,xyz,half in [('Station','Mars',(0,0,0),2000),('Noctis','Noctis',(0,0,0),1800),('Phobos','MarI',(0,0,0),1800),('Deimos','MarII',(0,0,0),1500)]:
        s=placed(u.JTSPlanetLandingSite,label+'_Landing',k,xyz,planet_anchor=planets['Mars' if k=='Noctis' else k]);s.get_editor_property('landing_volume').set_box_extent(u.Vector(half,half,800))
    landing=planets['Mars'].get_editor_property('landing_anchor_actor')
    h=planets['Mars'].project_point_to_surface(point('Mars',2500,-1000,0))
    frame=planets['Mars'].get_surface_frame_at(h.impact_point,basis('Mars')[1])
    landing.set_actor_location_and_rotation(h.impact_point,frame.transform.rotation.rotator(),False,False)
    actors['MC_Station_Landing'].set_actor_location_and_rotation(h.impact_point,frame.transform.rotation.rotator(),False,False)
    for a in actors.values():
        if isinstance(a,u.JTSPlanetArrivalAnchor) and (a.get_editor_property('planet_anchor')==planets['Mars'] or str(a.get_editor_property('planet_id'))=='Mars'):
            a.set_actor_location_and_rotation(h.impact_point,frame.transform.rotation.rotator(),False,False)
            a.get_editor_property('spacecraft_arrival_point').set_relative_location(u.Vector(0,0,450),False,False)
            a.get_editor_property('player_arrival_point').set_relative_location(u.Vector(-1000,0,150),False,False)
    for label,k,coord,name in [('Station','Mars','Mars_Station','赤原生态站'),('Noctis','Noctis','Mars_Noctis','夜之迷宫')]:placed(u.JTSSurfaceWaypoint,label+'_Waypoint',k,(0,0,300),planet=planets['Mars'],coordinate_id=coord,display_name=name)
    # Underground hub and four walkable wings, all with their own indoor floor binding.
    room('Hub','Mars',(0,0,-3200),(1200,1200,600),('N','S','E','W'),'中心井')
    room('B1','Mars',(-3000,0,-3200),(2000,2800,600),('E',),'B1 · 菌丝工坊')
    room('B2','Mars',(3000,0,-3200),(2000,2800,600),('W',),'B2 · 授粉温室')
    room('B3','Mars',(0,3400,-3200),(2000,2800,600),('S','E'),'B3 · 鱼菜共生池')
    room('B4','Mars',(0,-3400,-3200),(2000,2800,600),('N',),'B4 · 土壤工厂')
    room('SeedVault','Mars',(2700,3400,-3200),(1400,1400,1000),('W',),'种子库 · 封条完好')
    for label,xyz,size,door in [('West',(-1300,0,-3200),(1400,400,400),('E','W')),('East',(1300,0,-3200),(1400,400,400),('E','W')),('North',(0,1300,-3200),(400,1400,400),('N','S')),('South',(0,-1300,-3200),(400,1400,400),('N','S')),('Vault',(1500,3400,-3200),(1000,400,400),('E','W'))]:room('Hall_'+label,'Mars',xyz,size,door)
    entry=marker('Hub_Arrival','Mars',(0,0,-3050));outside=marker('Station_Arrival','Mars',(400,-1000,150))
    transit('Station_Elevator','Mars',(0,-1100,100),entry,'乘电梯前往地下',['Mars_Power'])
    # The surface entrance is a separate six-metre building, with a four-metre doorway.
    for name,xyz,dim in [('Roof',(0,-1100,520),(640,640,40)),('Back',(0,-800,250),(600,40,500)),('Left',(-300,-1100,250),(40,600,500)),('Right',(300,-1100,250),(40,600,500)),('JambL',(-250,-1400,250),(100,40,500)),('JambR',(250,-1400,250),(100,40,500)),('Header',(0,-1400,450),(400,40,100))]:
        block('ElevatorBuilding_'+name,point('Mars',*xyz),basis('Mars')[4],dim,'White',planets['Mars'])
    transit('Hub_Elevator','Mars',(-300,-300,-3100),outside,'返回赤原基地')
    vault=facility('SeedVault_Door','Mars',(1900,3400,-3000),'Mars_SeedVault_Open','刷入主管工牌',requires=['Mars_Badge_B'+str(i) for i in range(1,5)],source='ImportantItem',n=3,hide=True,dim=(50,400,400))
    shortcut=marker('Soil_Exit','Mars',(0,3000,150));transit('B4_Shortcut','Mars',(600,-4200,-3100),shortcut,'沿塌陷隧道返回板阵')
    badgepos=[(-3400,800,-3070),(3400,800,-3070),(500,4200,-3070),(-500,-4200,-3070)]
    for i,pos in enumerate(badgepos,1):placed(keys['Mars_Badge_B'+str(i)],'Badge_B'+str(i),'Mars',pos)
    corpse=u.load_asset(ROOT+'/Characters/SkeletonAstronaut/SM_SkeletonAstronaut_Corpse')
    if corpse:
        for i,pos in enumerate(badgepos+[(0,-1600,15)],1):
            a=actor(u.StaticMeshActor,'SupervisorRemains_'+str(i),point('Mars',pos[0]-100,pos[1]-100,-3190 if i<5 else 10),basis('Mars')[4],planets['Mars']);a.static_mesh_component.set_static_mesh(corpse);a.set_actor_enable_collision(False)
    # Noctis closed pockets, three long branches, one breakable hidden room, and a static cage transfer.
    room('NoctisHall','Noctis',(0,0,-2800),(2400,2400,700),('N','S','E','W'),'夜之迷宫 · 三岔厅')
    room('NoctisLoading','Noctis',(0,-4800,-1900),(1600,1200,700),('N',),'矿井前厅 · 装卸场')
    room('NoctisRampExit','Noctis',(0,-1700,-2800),(400,1000,450),('N','S'),'三岔厅')
    # A walkable enclosed spiral descends nine metres; the planet itself remains intact.
    ramp_floors=[];up=basis('Noctis')[3];forward,right=basis('Noctis')[1:3]
    for i in range(49):
        angle=-math.pi/2+i*3*math.pi/48
        x,y,z=math.cos(angle)*1000,-3200+math.sin(angle)*1000,-1900-i*900/48
        tangent=-forward*math.sin(angle)+right*math.cos(angle)
        outward=forward*math.cos(angle)+right*math.sin(angle)
        rot=u.MathLibrary.make_rot_from_xz(tangent,up);pos=point('Noctis',x,y,z)
        ramp_floors.append(block('Spiral_Step_'+str(i),pos-up*20,rot,(250,450,40),'Wall',planets['Mars']))
        block('Spiral_Roof_'+str(i),pos+up*450,rot,(280,530,40),'Dark',planets['Mars'])
        for side in (-1,1):block('Spiral_Wall_'+str(i)+'_'+str(side),pos+outward*side*245+up*215,rot,(270,40,470),'Wall',planets['Mars'])
    ramp=placed(u.JTSInteriorZone,'Spiral_Zone','Noctis',(0,-3200,-2150),zone_id='NoctisSpiral',planet=planets['Mars'],display_name='螺旋坡道')
    ramp.bounds.set_box_extent(u.Vector(1350,1350,800));setp(ramp,floor_actors=ramp_floors)
    for i in range(4):
        lamp=placed(u.PointLight,'Spiral_Light_'+str(i),'Noctis',(0,-3200,-1800-i*200))
        lamp.point_light_component.set_intensity(650);lamp.point_light_component.set_attenuation_radius(1600)
    for name,xyz,size,doors in [('FurnaceBranch',(-4000,0,-2800),(5600,500,500),('E','W')),('IceBranch',(4000,0,-2800),(5600,500,500),('E','W')),('CampBranch',(0,4000,-2800),(500,5600,500),('N','S'))]:room(name,'Noctis',xyz,size,doors)
    for name,xyz in [('Furnace',(-7200,0,-2800)),('IceVein',(7200,0,-2800)),('Camp',(0,7200,-2800))]:room('Nest_'+name,'Noctis',xyz,(1800,1800,600),('N','S','E','W'),name+' 组装巢')
    room('EmptyRoom','Noctis',(7200,2200,-2800),(800,800,800),('S',),'空室')
    room('EmptyLink','Noctis',(7200,1500,-2800),(400,600,400),('N','S'))
    room('FabricatorHall','Noctis',(8000,-3000,-4000),(4000,4000,2000),('W',),'母机大厅')
    noctin=marker('Noctis_Arrival','Noctis',(0,-4800,-1740));noctout=marker('Noctis_Exit','Noctis',(400,0,150))
    transit('Noctis_Entrance','Noctis',(0,0,100),noctin,'进入矿井前厅');transit('Noctis_Return','Noctis',(0,-800,-2700),noctout,'返回地表')
    hallreturn=marker('Noctis_BranchReturn','Noctis',(0,0,-2640))
    for branch,xyz in [('Furnace',(-7600,500,-2670)),('IceVein',(7600,500,-2670)),('Camp',(500,7600,-2670))]:
        transit(branch+'_Return','Noctis',xyz,hallreturn,'沿单向落差返回三岔厅')
    main=facility('Noctis_MainGate','Noctis',(0,1000,-2600),'Mars_Noctis_MainGate_Open','主矿井闸门',requires=['Noctis_Nest_'+s for s in ('Furnace','IceVein','Camp')],source='BlockedEntrance',n=2,auto=True,hide=True,dim=(400,60,400))
    for i,name in enumerate(('Furnace','IceVein','Camp')):
        light=placed(u.PointLight,'LockLight_'+name,'Noctis',(-280+i*280,850,-2280))
        light.point_light_component.set_light_color(u.LinearColor(.2,1,.45,1));light.point_light_component.set_intensity(700);light.point_light_component.set_attenuation_radius(400)
        latch=facility('LockState_'+name,'Noctis',(-280+i*280,980,-2280),'Noctis_Lock_'+name,'巢穴供能锁',requires=['Noctis_Nest_'+name],source='BlockedEntrance',auto=True,dim=(90,30,50))
        setp(latch,powered_actors=[light]);latch.mesh.set_material(0,mats['Dark'])
        text('LockLabel_'+name,point('Noctis',-400+i*280,950,-2190),basis('Noctis')[4],name,planets['Mars'],22)
    bossin=marker('Fabricator_Arrival','Noctis',(6500,-3000,-3840));bossout=marker('Cage_Return','Noctis',(0,600,-2650))
    transit('Noctis_Cage','Noctis',(0,800,-2700),bossin,'启动货笼 · 下降 50 秒',['Mars_Noctis_MainGate_Open'],50)
    transit('Fabricator_Return','Noctis',(6400,-3400,-3900),bossout,'乘货笼返回',[('DefeatedEncounter','MarsNoctisFabricator')])
    if hasattr(u,'JTSBreakableObstacle'):
        facility('EmptyRoom_Wall','Noctis',(7200,1800,-2600),'Mars_Noctis_EmptyRoom_Open','有裂纹的岩壁',hide=True,dim=(400,65,400),cls=u.JTSBreakableObstacle)
    # Satellite pockets and the merchant, deliberately without cameras.
    room('Deimos_A','MarII',(0,0,-2200),(1400,1800,600),('E',),'坍塌洞穴')
    room('Deimos_Link','MarII',(1000,0,-2200),(600,400,400),('E','W'))
    room('Deimos_B','MarII',(2400,0,-2200),(2200,2000,700),('W',),'收藏室')
    deimin=marker('Deimos_Arrival','MarII',(-300,0,-2040));deimout=marker('Deimos_SurfaceArrival','MarII',(400,0,160))
    transit('Deimos_Entrance','MarII',(0,0,100),deimin,'进入塌陷坑');transit('Deimos_Return','MarII',(-500,-500,-2100),deimout,'返回地表')
    if hasattr(u,'JTSBreakableObstacle'):facility('Deimos_Wall','MarII',(1200,0,-2000),'Deimos_HiddenWall_Open','有细裂纹的死胡同岩壁',hide=True,dim=(65,400,400),cls=u.JTSBreakableObstacle)
    if hasattr(u,'JTSFuelDepot'):facility('Phobos_Fuel','MarI',(700,600,120),'Phobos_FuelDepot_Repaired','修复港口燃料库',cls=u.JTSFuelDepot).set_editor_property('planet',planets['MarI'])
    # Narrative assets carry full text; no field-guide/scanner entries.
    logs=[
      ('Director','站长便笺','第 412 日：汇流箱 A、B、C 都要接回去。板上积尘等下一轮尘卷扫过，电梯才能启动。','Mars',(-600,-1500,120)),
      ('Power','电力值班记录','第 413 日：灯灭以后不要进地下。先修三只橙色汇流箱，再等风把板阵擦净。','Mars',(1600,3100,120)),
      ('B1','菌丝工坊主管','第 414 日：工牌留在工作台旁。孢子雾里先开腰灯，别摸黑走。','Mars',(-3500,650,-3070)),
      ('B2','授粉温室主管','第 415 日：绒蛾总追着灯。关掉腰灯，等它们散开再拿工牌。','Mars',(3500,650,-3070)),
      ('B3','鱼菜共生池主管','第 416 日：冰格最多承两个人。分开走，冰下那东西会先顶出一道白纹。','Mars',(500,4000,-3070)),
      ('B4','土壤工厂主管','第 417 日：出口在南端。尘圈冒起来时往旁边让，别站在蠕虫嘴上。','Mars',(-500,-4000,-3070)),
      ('Seal','种子库交接','第 418 日：四区主管任取三张工牌。封条从外面贴好，里面没有值班人员。','Mars',(1650,3150,-3050)),
      ('Matriarch','行为观察','第 410 日：团团冲起来不会转弯。引它撞加固柱，再一起掀翻；一个人要多撑三秒。','Mars',(2200,3850,-3070)),
      ('Larvae','育幼观察','第 411 日：幼体出来时先留好退路。旧壳掉下来的地方泛着浅色，那层新壳还很软。','Mars',(3000,3850,-3070)),
      ('Miner1','矿工记录 01','第 421 日：矿机这几天不听指令了。先停两座组装巢，主闸才会让我们通过。','Noctis',(-700,300,-2670)),
      ('Miner2','矿工记录 02','第 422 日：它们的固件被改过，不是我们改的。熔炉不停，母机身上的甲片就一直补。','Noctis',(-6100,150,-2670)),
      ('Miner3','矿工记录 03','第 423 日：冰脉的小机子藏在顶上。先听落冰，再找它的突刺方向。','Noctis',(6000,150,-2670)),
      ('Miner4','矿工记录 04','第 424 日：废料堆会动。营地巢还在运转时，别背对那些蜷成团的机器。','Noctis',(150,6000,-2670)),
      ('Nameplate','检修规程','第 390 日：真机铭牌是 ORIGINAL FABRICATOR · UNIT 000。用腰灯照金属面；复制核心挨打会炸。','Noctis',(6800,-4200,-3870)),
      ('Empty','折回的检修单','第 426 日：冰脉尽头的墙有细裂纹。敲开后什么都没有，地面却平得不像矿洞。','Noctis',(7100,2300,-2670)),
      ('Phobos_Rover','履带旁的便笺','第 432 日：我把徽记画大了一半。若有人从火星上看见，请来斯蒂克尼港。燃料泵还修得好。','MarI',(-600,600,130)),
      ('Phobos_Telescope','望远镜值班日志','第 433 日：小的那颗卫星又喷尘了。顺着间歇的白烟找坑口。照片里那道细亮线不是天线。','MarI',(900,-800,130)),
      ('Phobos_Fuel','港口补给说明','第 429 日：泵修好后，每次靠港只能抽一批燃料。废墟里的构件还能用采矿镐拆下来。','MarI',(650,450,130)),
      ('DoorSkeleton','门边的遗言','第 437 日：三张工牌就够了。若有人能看见这些字，记得把灯带上。','Mars',(-500,-1200,130)),
      ('Deimos_Wall','岩壁上的划痕','几道新划痕沿裂纹伸向墙后。这里的石面比周围薄，采矿镐也许敲得开。','MarII',(700,-300,-2070))]
    def document(name,title,body):
        path=DATA+'/Logs/DA_Log_'+name; d=u.load_asset(path)
        if not d:
            f=u.DataAssetFactory();setp(f,data_asset_class=u.JTSReadableDocumentData);d=assets.create_asset('DA_Log_'+name,DATA+'/Logs',u.JTSReadableDocumentData,f)
        setp(d,document_id='MarsLog_'+name,title=title,body=body);return save(d)
    for name,title,body,k,xyz in logs:
        a=placed(u.JTSReadableDocument,'Log_'+name,k,xyz,document=document(name,title,body));a.get_component_by_class(u.StaticMeshComponent).set_static_mesh(cube);a.set_actor_scale3d(u.Vector(.45,.32,.04));a.get_component_by_class(u.StaticMeshComponent).set_material(0,mats['White'])
    telescope_log=u.load_asset(DATA+'/Logs/DA_Log_Phobos_Telescope')
    if photos[0]:setp(telescope_log,image=photos[0]);save(telescope_log)
    for i in (1,2):
        if photos[i]:
            d=document('Phobos_Exposure_'+str(i+1),'望远镜曝光 '+str(i+1),'第 433 日：同一喷尘点的连续曝光。火卫二坑口间歇排气。');setp(d,image=photos[i]);save(d)
            a=placed(u.JTSReadableDocument,'Exposure_'+str(i+1),'MarI',(900+i*80,-800,130),document=d);a.get_component_by_class(u.StaticMeshComponent).set_static_mesh(cube);a.set_actor_scale3d(u.Vector(.45,.32,.04));a.get_component_by_class(u.StaticMeshComponent).set_material(0,mats['White'])
    first='\n\n'.join(['啊，终于！我的演——旅人们！请坐，请坐。','这身旧衣服？都是人类的好手艺，我舍不得换。','别敲那个电视，它已经很努力了。','你们走过来的样子，和我想的一样。','这个快乐杆送给你们，装在船上就能用。','顺手替你们接好了星际耦合，先留着。','木星坐标也给你们了。','去那颗最大行星的卫星看看——等它安静下来以后。','不用付钱，见到你们就值回来了。','演员们——旅人们，别总站在门口。','头盔？这里冷，我怕冷。','下次来，给我讲讲路上捡到的东西。'])
    merchant=placed(u.JTSMerchant,'Jummt','MarII',(2700,200,-2070),document=document('Jummt_First','Jummt',first),repeat_dialogue=document('Jummt_Repeat','Jummt','旅人们，又回来了！快乐杆和坐标已经给过你们。路上有什么新鲜事？'),gift_state_id='Deimos_Merchant_Gifted',dock_module_id='StellarExchange',gift_coordinate_id='Jupiter',gift_important_item_id='InterstellarCoupling')
    merchant.get_component_by_class(u.StaticMeshComponent).set_static_mesh(cylinder);merchant.set_actor_scale3d(u.Vector(.65,.65,1.8));merchant.get_component_by_class(u.StaticMeshComponent).set_material(0,mats['White'])
    block('Jummt_Visor',point('MarII',2700,200,-1930),basis('MarII')[4],(60,65,50),'Alloy',planets['MarII'],sphere)
    # Species remain variants of the existing authoritative settlement template.
    creatures={}
    species=[('Woodlouse','穴虱','ROLL','Woodlouse',(1.4,.8,.7),500,12,120),('Larva','穴虱幼体','ROLL','Woodlouse',(.56,.32,.28),300,5,35),
             ('Moth','绒蛾群','LIGHT_FLOCK','Moth',(.35,.5,.1),180,2,50),('DuskMoth','黄昏绒蛾','LIGHT_FLOCK','Moth',(.35,.5,.1),100,0,40),
             ('Icegill','冰鳃鱼','ICE_AMBUSH','Ice',(1.2,.6,.6),420,10,100),('Worm','掘土蠕虫','BURROW_BITE','Worm',(1,1,3),0,18,160),
             ('HeavyMiner','重型矿机','HEAVY_MINER','Orange',(1.6,1.4,2.3),90,22,240),('FastMiner','冰脉快速单位','CEILING_RUNNER','White',(1,.8,.5),480,12,80),
             ('FeignMiner','装死矿机','FEIGN_WRECK','White',(1.8,.9,1.1),350,16,110)]
    def creature_config(c,name,title,tactic,mat,dim,speed,damage,hp):
        setp(c,display_name=title,specimen_id='Mars_'+name,body_scale=1,combat_body_bone='',defends_nest=False,retaliates=True,leash_radius=1600)
        c.get_component_by_class(u.SphereComponent).set_sphere_radius(max(dim[0],dim[1])*32)
        setp(c.get_component_by_class(u.JTSHealthComponent),max_health=hp)
        t=c.get_component_by_class(u.JTSCreatureTacticsComponent)
        setp(t,tactic=getattr(u.JTSCreatureTactic,tactic),body_mesh=sphere if 'Miner' not in name else cube,body_material=mats[mat],body_size=u.Vector(*dim),body_offset=u.Vector(0,0,dim[2]*45),attack_speed=speed,damage=damage,passive=name=='DuskMoth',detect_radius=1200)
        setp(c.get_component_by_class(u.JTSPlanetSurfaceSteeringComponent),body_radius=max(15,dim[1]*25),body_ground_clearance=8,max_slope_degrees=48)
    for spec in species:
        creatures[spec[0]]=blueprint('BP_Mars_'+spec[0],u.JTSPlanetSurfaceCreature,lambda c,spec=spec:creature_config(c,*spec))
    def settlement(name,k,xyz,enemy,count=4,radius=600):
        a=placed(u.JTSPlanetEnemySettlement,name,k,xyz,enemy_class=creatures[enemy],enemy_count=count,spawn_radius=radius,minimum_spacing=130,maintain_population=False)
        setp(a,tags=['Mars' if k=='Noctis' else k]);return a
    settlement('B1_Woodlice','Mars',(-3000,0,-3200),'Woodlouse',3,700)
    settlement('B2_Moths','Mars',(3000,0,-3200),'Moth',2,700)
    settlement('B3_Icegills','Mars',(0,3400,-3200),'Icegill',3,650)
    settlement('B4_Worms','Mars',(0,-3400,-3200),'Worm',3,700)
    settlement('Solar_DuskMoths','Mars',(0,4300,0),'DuskMoth',3,1800)
    for i,(x,y) in enumerate(((-4900,3800),(0,6200),(4900,3800))):
        settlement('Solar_Woodlice_'+str(i),'Mars',(x,y,0),'Woodlouse',3,1100)
    settlement('Soil_ExitWorms','Mars',(0,3000,0),'Worm',2,900)
    nests=[]
    for name,xyz,enemy in [('Furnace',(-7200,0,-2800),'HeavyMiner'),('IceVein',(7200,0,-2800),'FastMiner'),('Camp',(0,7200,-2800),'FeignMiner')]:
        def nest_config(c,enemy=enemy):
            setp(c,requires_specimen_analysis=False,destructible_entrance=True,enemy_class=creatures[enemy],enemy_count=4,spawn_radius=700,minimum_spacing=160,maintain_population=True,refill_batch_size=1,refill_interval=12)
            c.refuge_mesh.set_static_mesh(cylinder);c.refuge_mesh.set_relative_scale3d(u.Vector(4,4,1));c.refuge_mesh.set_material(0,mats['Orange']);c.refuge_mesh.set_relative_location(u.Vector(0,0,50),False,False)
            setp(c.get_component_by_class(u.JTSHealthComponent),max_health=180)
        cls=blueprint('BP_Nest_'+name,u.JTSPlanetEcologyHabitat,nest_config)
        nest=placed(cls,'Nest_'+name,'Noctis',xyz,tags=['Mars']);setp(nest.blockade,persistent_entrance_id='Noctis_Nest_'+name);nests.append(nest)
    def attack(windup=1,active=.4,recovery=1,damage=15,reach=500,radius=200,cooldown=4):
        return setp(u.JTSBossAttack(),windup=windup,active=active,recovery=recovery,damage=damage,reach=reach,radius=radius,cooldown=cooldown)
    def phase(threshold,walk,charge,mult=1,summon=None,count=0,attacks=None,**extra):
        return setp(u.JTSBossPhase(),enter_below_health=threshold,walk_speed=walk,charge_speed=charge,damage_multiplier=mult,summon_class=summon,summon_count=count,attacks=attacks or {},**extra)
    actions=u.JTSBossAction
    tuanph=[phase(1,120,1000,attacks={actions.CHARGE:attack(.9,1.8,1.2,22,500,170,4)}),
            phase(.66,100,750,summon=creatures['Larva'],count=4,attacks={actions.SLAM:attack(damage=18)}),
            phase(.33,260,1250,1.5,attacks={actions.CHARGE:attack(.6,1.3,.8,24),actions.SWEEP:attack(.7,.5,.7,18,500)})]
    def boss_config(c,name,title,hp,phases,flip=False):
        setp(c,display_name=title,specimen_id='Mars_'+name,extra_player_health=hp*.35)
        setp(c.health,max_health=hp);setp(c.get_component_by_class(u.JTSBossPhaseComponent),phases=phases,cooperative_flip=flip)
        setp(c.combat,stagger_on_charge_obstacle=flip,stagger_seconds=6 if flip else .9,leash_radius=4000,aggro_radius=5000,turn_speed_degrees=85)
        c.mesh.set_material(0,mats['Woodlouse' if flip else 'Orange'])
    tuan=blueprint('BP_Mars_Tuantuan',u.JTSPlanetBoss,lambda c:boss_config(c,'Tuantuan','铠母 · 团团',900,tuanph,True),ROOT+'/Blueprints/Planets/Moon/EcologyV3/BP_MirrorBoss')
    def core_config(c):c.mesh.set_static_mesh(sphere);c.mesh.set_relative_scale3d(u.Vector(1.2,1.2,1.2));c.mesh.set_material(0,mats['Lamp'])
    core=blueprint('BP_FabricatorCore',u.JTSBossCoreDecoy,core_config)
    def slag_config(c):setp(c,kind=u.JTSFacilityHazardKind.SLAG,damage_per_second=9);c.bounds.set_box_extent(u.Vector(220,220,150));c.visual.set_static_mesh(cylinder);c.visual.set_relative_scale3d(u.Vector(4,4,.1));c.visual.set_material(0,mats['Orange'])
    slag=blueprint('BP_FabricatorSlag',u.JTSFacilityHazard,slag_config)
    def assembly_config(c):
        setp(c,display_name='组装点 · 击破以中断增援',requires_mining_tool=False,completed_flag='',repair_seconds=0)
        c.mesh.set_static_mesh(cylinder);c.mesh.set_relative_scale3d(u.Vector(1.6,1.6,1.2));c.mesh.set_relative_location(u.Vector(0,0,60),False,False);c.mesh.set_material(0,mats['Lamp']);setp(c.health,max_health=45)
    assembly=blueprint('BP_FabricatorAssembly',u.JTSBreakableObstacle,assembly_config)
    fabph=[phase(1,0,0,summon=creatures['FastMiner'],count=2,assembly_point_class=assembly,assembly_seconds=4,attacks={actions.SLAM:attack(1.2,.5,1.2,25,1500,600),actions.SWEEP:attack(1.3,.8,1.5,20,1800,400)}),
           phase(.66,170,650,attacks={actions.CHARGE:attack(1.3,1.8,1.3,26,500),actions.SWEEP:attack(1,.5,1.3,20,800)},trail_hazard_class=slag),
           phase(.33,0,0,attacks={actions.SLAM:attack(1.5,.4,2,15,1000,350)},core_copies=True,core_class=core)]
    fab=blueprint('BP_Mars_Fabricator',u.JTSPlanetBoss,lambda c:boss_config(c,'Fabricator','母机',1500,fabph),ROOT+'/Blueprints/Planets/Moon/EcologyV3/BP_MirrorBoss')
    for name,k,xyz,zone,cls,encid,reward,req in [('Tuantuan','Mars',(2850,3400,-3200),'SeedVault',tuan,'MarsSeedVaultMatriarch','Mars_TrackingCollar','Mars_SeedVault_Open'),('Fabricator','Noctis',(8000,-3000,-4000),'FabricatorHall',fab,'MarsNoctisFabricator','Mars_LogisticsChip','Mars_Noctis_MainGate_Open')]:
        e=placed(u.JTSNestBossEncounter,'Encounter_'+name,k,xyz,planet=planets['Mars'],room_trigger=True,trigger_room=rooms[zone],boss_class=cls,encounter_id=encid,reward_class=keys[reward],reward_forward_offset=220,wake_delay=1)
        conditions(e,[req])
        door=block(name+'_ArenaDoor',point(k,1900,3400,-3000) if name=='Tuantuan' else point(k,6000,-3000,-3800),basis(k)[4],(60,400,400),'White',planets['Mars'])
        setp(e,arena_doors=[door]);door.set_actor_hidden_in_game(True);door.set_actor_enable_collision(False)
        if name=='Fabricator':
            mods=[]
            for i,n in enumerate(('Furnace','IceVein','Camp')):
                m=setp(u.JTSNestBossModifier(),entrance_id='Noctis_Nest_'+n,armor_multiplier=.6 if i==0 else 1)
                if i:setp(m,summon_class=creatures['FastMiner' if i==1 else 'FeignMiner'],summon_count=3)
                mods.append(m)
            setp(e,remaining_nest_modifiers=mods)
    for x,y in [(2250,3000),(3350,3800)]:block('Tuantuan_Pillar_'+str(x),point('Mars',x,y,-2900),basis('Mars')[4],(130,130,600),'White',planets['Mars'],cylinder)
    # Range-based resource generation uses the existing resource template with configured payload.
    for label,k,xyz,resource in [('B1','Mars',(-3000,0,-3200),u.JTSResourceType.ORGANIC),('B2','Mars',(3000,0,-3200),u.JTSResourceType.FOOD),('B3','Mars',(0,3400,-3200),u.JTSResourceType.WATER),('B4','Mars',(0,-3400,-3200),u.JTSResourceType.ROCK)]:
        s=placed(u.JTSMoonResourceSpawner,'Resource_'+label,k,xyz,tags=['Mars'],coverage_planet=planets['Mars'],override_coverage_radius=True,radius=750,resource_density=4,override_primary_resource=True,primary_resource=resource)
        ratios=u.JTSMoonResourceDistribution()
        for f in ('small_rock','medium_rock','large_rock','medium_metal_rock','large_metal_rock','medium_crystal_rock','large_crystal_rock'):setp(ratios,**{f:100 if f=='medium_rock' else 0})
        setp(s,resource_ratios=ratios)
    if hasattr(u,'JTSFacilityHazard'):
        fog=placed(u.JTSFacilityHazard,'B1_Spores','Mars',(-3000,0,-2900),kind=u.JTSFacilityHazardKind.SPORE_FOG);fog.bounds.set_box_extent(u.Vector(900,1300,290))
        ice=placed(u.JTSFacilityHazard,'B3_IceLoad','Mars',(0,3300,-3060),kind=u.JTSFacilityHazardKind.ICE_LOAD,maximum_occupants=2,warning_seconds=1.2,damage_per_second=15,trigger_sound=audio['IceCrack'])
        ice.bounds.set_box_extent(u.Vector(650,650,180))
        icefloor=block('B3_BrittleIce',point('Mars',0,3300,-3218),basis('Mars')[4],(1300,1300,35),'Ice',planets['Mars']);setp(ice,affected_floors=[icefloor])
        floors['B3'].set_actor_location(point('Mars',0,3400,-3520),False,False)
        rims=[]
        for name,xyz,dim in [('L',(-825,3400,-3220),(350,2800,40)),('R',(825,3400,-3220),(350,2800,40)),('N',(0,4375,-3220),(1300,850,40)),('S',(0,2325,-3220),(1300,650,40))]:
            rims.append(block('B3_PoolRim_'+name,point('Mars',*xyz),basis('Mars')[4],dim,'White',planets['Mars']))
        for i in range(6):rims.append(block('B3_PoolStep_'+str(i),point('Mars',-625+i*50,3300,-3250-i*50),basis('Mars')[4],(50,300,50),'White',planets['Mars']))
        rooms['B3'].set_actor_location(point('Mars',0,3400,-3050),False,False);rooms['B3'].bounds.set_box_extent(u.Vector(1000,1400,480));setp(rooms['B3'],floor_actors=[icefloor,floors['B3']]+rims)
        for i in range(3):
            h=placed(u.JTSFacilityHazard,'IceBranch_Falling_'+str(i),'Noctis',(2500+i*1000,0,-2600),kind=u.JTSFacilityHazardKind.FALLING_ICE,warning_seconds=1.3,damage_per_second=18,trigger_sound=audio['IceCrack'])
            h.bounds.set_box_extent(u.Vector(350,220,250));h.visual.set_static_mesh(sphere);h.visual.set_relative_scale3d(u.Vector(.4,.4,1.2));h.visual.set_material(0,mats['Ice'])
    # Exactly five refresh-on-entry cameras; zero on Deimos.
    for label,k,xyz in [('Hub','Mars',(450,450,-2720)),('SeedVault','Mars',(3200,3900,-2300)),('NoctisHall','Noctis',(950,950,-2200)),('EmptyRoom','Noctis',(7450,2450,-2120)),('FabricatorHall','Noctis',(9500,-4500,-2300))]:
        cam=placed(u.JTSBreakableObstacle,'Camera_'+label,k,xyz,display_name='旧式监控器',requires_mining_tool=False,refresh_on_entry=rooms[label],shutdown_seconds=.3,shutdown_sound=audio['CameraOff']);cam.mesh.set_static_mesh(cube);cam.set_actor_scale3d(u.Vector(.45,.3,.25));cam.mesh.set_material(0,mats['White']);cam.health.set_max_health(25,True)
    for label,k,xyz,sound,volume in [('HubHum','Mars',(0,0,-2850),'UndergroundHum',.5),('B1Hum','Mars',(-3000,0,-2850),'UndergroundHum',.4),('B2Hum','Mars',(3000,0,-2850),'UndergroundHum',.3),('B4Hum','Mars',(0,-3400,-2850),'UndergroundHum',.4),('GreenhouseFlutter','Mars',(3000,0,-2900),'MothFlutter',.75),('FurnaceHum','Noctis',(-7000,0,-2550),'Furnace',.7),('FabricatorHum','Noctis',(8000,-3000,-3700),'Furnace',.6)]:
        a=placed(u.AmbientSound,'Audio_'+label,k,xyz);a.audio_component.set_sound(audio[sound]);a.audio_component.set_volume_multiplier(volume)
    for label,k,xyz in [('Phobos_Salvage','MarI',(-900,400,140)),('Furnace_Salvage','Noctis',(-7000,-500,-2660)),('Camp_Salvage','Noctis',(600,7000,-2660))]:
        a=facility(label,k,xyz,label+'_Recovered','人类设备废料',hide=True,dim=(240,180,140),cls=u.JTSBreakableObstacle);setp(a,salvage={u.JTSResourceType.MANUFACTURED_PART:8})
    # Half sprout is large independent geometry, attached to Phobos without distance-hiding logic.
    for i,(x,y,angle) in enumerate([(-350,0,-25),(350,0,25),(0,-600,0)]):
        block('Phobos_Sprout_'+str(i),point('MarI',x,y,8,True),(basis('MarI')[4].quaternion()*u.Rotator(yaw=angle).quaternion()).rotator(),(550,1200,15) if i<2 else (120,1600,15),'White',planets['MarI'],sphere if i<2 else cube)
    block('Phobos_Rover',point('MarI',-600,800,120,True),basis('MarI')[4],(450,240,220),'White',planets['MarI'])
    block('Phobos_Telescope',point('MarI',900,-1000,180,True),basis('MarI')[4],(120,120,300),'Orange',planets['MarI'],cylinder)
    # Director and combat archetypes are configured only in the Mars system.
    spaceclasses=[]
    for ray in (False,True):
        def unit_config(c,ray=ray):
            setp(c,light_sail_ray=ray,size_tier=0 if ray else 2,salvage={u.JTSResourceType.ORGANIC:3} if ray else {u.JTSResourceType.ROCK:3,u.JTSResourceType.ORE:1})
            c.mesh.set_static_mesh(sphere if ray else cube);c.mesh.set_relative_scale3d(u.Vector(3,12,.25) if ray else u.Vector(8,8,8));c.mesh.set_material(0,mats['Gold' if ray else 'Wall']);setp(c.health,max_health=45 if ray else 65)
        spaceclasses.append(blueprint('BP_Space_'+('LightSailRay' if ray else 'Asteroid'),u.JTSSpaceEncounterUnit,unit_config))
    placed(u.JTSSpaceEncounterDirector,'Mars_EncounterDirector','Mars',(0,0,0),system_id='MarsSystem',unit_classes=spaceclasses,active_teams=4,approach_planet_id='Mars',approach_coordinate_id='Mars_Station')
    # Cage remains stationary. Decorative shaft bands scroll; guards use the same settlement lifecycle.
    guards=settlement('Cage_Guards','Noctis',(0,0,-2800),'FastMiner',3,650);setp(guards,tags=[],maintain_population=True,refill_interval=10,refill_batch_size=1)
    bands=[]
    for i in range(5):
        bands.append(block('Cage_ShaftBand_'+str(i),point('Noctis',0,1100,-2600+i*200),basis('Noctis')[4],(420,30,25),'Rib',planets['Mars']))
    setp(actors['MC_Noctis_Cage'],ride_walls=bands,ride_guards=guards,ride_sound=cage_audio)
    # Human industrial headframe, without touching the underlying canyon mesh.
    for i,(x,y) in enumerate(((-600,-600),(-600,600),(600,-600),(600,600))):
        block('Noctis_Headframe_'+str(i),point('Noctis',x,y,900,True),basis('Noctis')[4],(75,75,1800),'Orange',planets['Mars'])
    block('Noctis_HeadframeTop',point('Noctis',0,0,1800,True),basis('Noctis')[4],(1200,1200,90),'White',planets['Mars'])
    # Periodic dust vent is a satellite clue, with no navigation icon or camera.
    plume=placed(u.JTSFacilityHazard,'Deimos_DustVent','MarII',(0,0,900),kind=u.JTSFacilityHazardKind.DUST_SWEEP,warning_seconds=22,recovery_seconds=8)
    plume.visual.set_static_mesh(sphere);plume.visual.set_relative_scale3d(u.Vector(3,3,24));plume.visual.set_material(0,mats['Dust'])
    for name,xyz,dim,mat in [('Sofa',(3100,-500,-2060),(180,85,75),'White'),('TV',(2900,-650,-1880),(90,55,70),'Dark'),('Shelf',(3100,550,-1950),(180,45,240),'Orange'),('Desk',(2300,600,-2040),(160,80,75),'White')]:
        block('Jummt_'+name,point('MarII',*xyz),basis('MarII')[4],dim,mat,planets['MarII'])
    for i in range(8):block('Jummt_Paper_'+str(i),point('MarII',2250+(i%4)*25,580+(i//4)*25,-1995),basis('MarII')[4],(23,18,2),'White',planets['MarII'])
    # Replace the technical spike only; retain authored locations and original celestial assets.
    setp(power,powered_actors=[a for a in lights if a.get_actor_label().startswith(('MC_Hub','MC_B','MC_SeedVault','MC_Hall_'))])
    cp=station+basis('Mars')[3]*6500-basis('Mars')[1]*9000
    cr=u.MathLibrary.make_rot_from_xz(station-cp,basis('Mars')[3])
    u.EditorLevelLibrary.set_level_viewport_camera_info(cp,cr)

    # Remove only the disposable technical-spike shell; retain the two authored site markers.
    for a in list(actors.values()):
        if a.get_actor_label().startswith('Mars_Spike_'):sub.destroy_actor(a)
    u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
    return {'actors_authored':len([a for a in actors if a.startswith('MC_')]),'assets_saved':len(saved),'rooms':list(rooms),'unchanged_planet_meshes':True}



