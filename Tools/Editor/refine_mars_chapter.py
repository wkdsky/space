"""Content refinement of existing Mars chapter. Planet assets are read-only."""
from pathlib import Path
import json, math, runpy
import unreal as u

ROOT='/Game/Space/Environment/MarsSystem/Chapter/Refined'
BP='/Game/Space/Blueprints/Planets/Mars/Chapter'
DATA='/Game/Space/Data/Planets/Mars'

def context():
 sub=u.get_editor_subsystem(u.EditorActorSubsystem)
 aa={a.get_actor_label():a for a in sub.get_all_level_actors()}
 planets={str(a.get_planet_id()):a for a in aa.values() if isinstance(a,u.JTSPlanetAnchor)}
 frames={k:planets['Mars' if k=='Noctis' else k].get_surface_frame_at(aa['Mars_Site_'+('Noctis' if k=='Noctis' else 'Station')].get_actor_location() if k in ('Mars','Noctis') else planets[k].get_editor_property('landing_anchor_actor').get_actor_location(),u.Vector(1,0,0)) for k in ('Mars','Noctis','MarI','MarII')}
 return sub,aa,planets,frames
def setp(o,**kw):
 for k,v in kw.items():o.set_editor_property(k,v)
 return o
def refine():
 sub,aa,planets,frames=context();at=u.AssetToolsHelpers.get_asset_tools()
 source=Path(u.Paths.project_dir()).resolve()/'SourceArt/MarsStation/Refined'
 manifest=json.loads((source/'manifest.json').read_text(encoding='utf8'))
 def save(a):u.EditorAssetLibrary.save_loaded_asset(a,False);return a
 # Three surface types at most: palette, luminous palette and transparent ETFE.
 texture=u.load_asset(ROOT+'/T_MP_Palette')
 if not texture:
  task=setp(u.AssetImportTask(),filename=str(source/'T_MP_Palette.png'),destination_path=ROOT,destination_name='T_MP_Palette',automated=True,save=True)
  at.import_asset_tasks([task]);texture=u.load_asset(ROOT+'/T_MP_Palette')
 setp(texture,filter=u.TextureFilter.TF_NEAREST);save(texture)
 def material(name,glow=False,glass=False,color=None):
  m=u.load_asset(ROOT+'/'+name)
  if m:return m
  m=at.create_asset(name,ROOT,u.Material,u.MaterialFactoryNew());setp(m,two_sided=True,used_with_instanced_static_meshes=True,used_with_skeletal_mesh=True)
  if color:
   c=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionConstant3Vector,-300,0);setp(c,constant=u.LinearColor(*color,1));pin=''
  else:
   c=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionTextureSample,-300,0);setp(c,texture=texture);pin='RGB'
  u.MaterialEditingLibrary.connect_material_property(c,pin,u.MaterialProperty.MP_BASE_COLOR)
  if glow:u.MaterialEditingLibrary.connect_material_property(c,pin,u.MaterialProperty.MP_EMISSIVE_COLOR)
  r=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionConstant,-300,220);setp(r,r=.85);u.MaterialEditingLibrary.connect_material_property(r,'',u.MaterialProperty.MP_ROUGHNESS)
  if glass:
   setp(m,blend_mode=u.BlendMode.BLEND_TRANSLUCENT)
   op=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionConstant,-300,350);setp(op,r=.13);u.MaterialEditingLibrary.connect_material_property(op,'',u.MaterialProperty.MP_OPACITY)
  u.MaterialEditingLibrary.recompile_material(m);return save(m)
 mats={'MP_Palette':material('M_MP_Palette'),'MP_Luminous':material('M_MP_Luminous',True),'MP_Glass':material('M_MP_ETFE',glass=True,color=(.50,.67,.62))}
 amber=material('M_MP_Amber',True,color=(1,.18,.015));green=material('M_MP_Green',True,color=(.06,.8,.36))
 meshes={};tasks=[]
 for key,v in manifest.items():
  if key=='palette':continue
  name=v['mesh'];m=u.load_asset(ROOT+'/'+name)
  if m and u.EditorAssetLibrary.get_metadata_tag(m,'MarsRefinementRevision')=='2':meshes[key]=m;continue
  ui=setp(u.FbxImportUI(),import_mesh=True,import_materials=False,import_textures=False,import_animations=False,import_as_skeletal=False,mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH,automated_import_should_detect_type=False)
  setp(ui.static_mesh_import_data,combine_meshes=True,auto_generate_collision=v['collision']!='none',generate_lightmap_u_vs=False)
  task=setp(u.AssetImportTask(),filename=str(source/(name+'.fbx')),destination_path=ROOT,destination_name=name,automated=True,save=True,replace_existing=True,replace_existing_settings=True,options=ui);tasks.append(task)
 if tasks:at.import_asset_tasks(tasks)
 for key,v in manifest.items():
  if key=='palette':continue
  m=meshes.get(key) or u.load_asset(ROOT+'/'+v['mesh']);assert m,'Failed import '+key
  for i,slot in enumerate(m.get_editor_property('static_materials')):m.set_material(i,mats[str(slot.material_slot_name)])
  if v['collision']=='complex' or key in ('Solar_Tracker','Corridor_Rib','Door_Frame','Lab_Bench','Seed_Rack','Grow_Bed','Nursery_Rack'):
   setp(m.get_editor_property('body_setup'),collision_trace_flag=u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
  u.EditorAssetLibrary.set_metadata_tag(m,'MarsRefinementRevision','2');save(m);meshes[key]=m
 def pid(k):return 'Mars' if k=='Noctis' else k
 def position(k,xyz,surface=False):
  f=frames[k];x,y,z=xyz;p=f.location+f.forward*x+f.right*y+f.up*z
  if surface:
   hit=planets[pid(k)].project_point_to_surface(p);assert hit.blocking_hit
   return hit.impact_point+planets[pid(k)].get_radial_up_vector(hit.impact_point)*z
  return p
 def rotation(k,yaw=0,world=None):
  f=frames[k]
  if world is not None:f=planets[pid(k)].get_surface_frame_at(world,frames[k].forward)
  return (f.transform.rotation*u.Rotator(yaw=yaw).quaternion()).rotator()
 def actor(cls,label,k,xyz,yaw=0,surface=False):
  label='MC_R_'+label;a=aa.get(label);p=position(k,xyz,surface)
  if not a:a=sub.spawn_actor_from_class(cls,p,rotation(k,yaw,p if surface else None));aa[label]=a
  a.set_actor_label(label);a.set_folder_path('MarsChapter/Refined/'+k)
  a.set_actor_location_and_rotation(p,rotation(k,yaw,p if surface else None),False,False);a.set_actor_scale3d(u.Vector(1,1,1))
  parent=planets[pid(k)] if a.get_editor_property('replicates') else planets[pid(k)].get_editor_property('gameplay_surface_actor')
  a.attach_to_actor(parent,'',u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,False)
  return a
 def apply(a,key,collision=True):
  c=a.get_component_by_class(u.StaticMeshComponent);c.set_mobility(u.ComponentMobility.MOVABLE);c.set_static_mesh(meshes[key]);c.set_relative_scale3d(u.Vector(1,1,1))
  setp(c,override_materials=[]);c.set_collision_profile_name('BlockAll' if collision else 'NoCollision');a.set_actor_scale3d(u.Vector(1,1,1));return c
 def prop(name,key,k,xyz,yaw=0,surface=False,collision=True,scale=None):
  if key in ('Nursery_Rack','Lab_Bench','Seed_Rack','Door_Frame','Vault_Door','Vault_Frame','Furnace_Bank','Water_Recycler','Soil_Processor','Lift_Housing'):yaw+=180
  a=actor(u.StaticMeshActor,name,k,xyz,yaw,surface);apply(a,key,collision)
  if scale:a.set_actor_scale3d(u.Vector(*scale))
  return a
 def sign(name,k,xyz,body,size=25,yaw=0,color=(205,226,208),surface=False):
  a=actor(u.TextRenderActor,'Sign_'+name,k,xyz,yaw,surface);c=a.get_component_by_class(u.TextRenderComponent)
  c.set_text(body);c.set_world_size(size);c.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER);c.set_text_render_color(u.Color(*color,255));c.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
  return a
 def light(name,k,xyz,intensity=1600,radius=650,color=(.60,.85,.74)):
  a=actor(u.PointLight,name,k,xyz);c=a.point_light_component;c.set_mobility(u.ComponentMobility.MOVABLE);c.set_intensity(intensity*(.35 if xyz[2]<0 else 1));c.set_attenuation_radius(radius);c.set_cast_shadows(False);c.set_light_color(u.LinearColor(*color,1));return a
 def move(a,k,xyz,surface=False,yaw=0):
  p=position(k,xyz,surface);a.set_actor_location_and_rotation(p,rotation(k,yaw,p if surface else None),False,False)
 def readable(name,k,xyz,title,body,yaw=0,surface=False):
  d=u.load_asset(DATA+'/DA_Guide_'+name)
  if not d:
   factory=setp(u.DataAssetFactory(),data_asset_class=u.JTSReadableDocumentData);d=at.create_asset('DA_Guide_'+name,DATA,u.JTSReadableDocumentData,factory)
  setp(d,document_id='Guide_'+name,title=title,body=body);save(d)
  a=actor(u.JTSReadableDocument,'Guide_'+name,k,xyz,yaw,surface);setp(a,document=d);apply(a,'Data_Pad')
  if surface or name=='NoctisOperations':
   stand=prop('GuideStand_'+name,'Log_Pedestal',k,xyz,yaw,surface)
   stand.set_actor_location(a.get_actor_location()-a.get_actor_up_vector()*111,False,False)
  return a
 # Remove only the technical prototype geometry that is explicitly superseded.
 remove=['MC_DomeModule_0','MC_DomeModule_1']
 remove += [n for n in aa if n.startswith(('MC_Station_Lift_','MC_ElevatorBuilding_','MC_Noctis_Headframe_')) or n=='MC_Noctis_HeadframeTop']
 for name in remove:
  if name in aa:sub.destroy_actor(aa.pop(name))
 for i,(x,y) in enumerate(((-4900,3800),(0,6200),(4900,3800))):
  sector='ABC'[i]
  for j in range(12):
   a=aa['MC_Solar_'+str(i)+'_'+str(j)];apply(a,'Solar_Tracker')
   ground=position('Mars',(x+(j%4-1.5)*650,y+(j//4-1)*1200,0),True)
   up=planets['Mars'].get_radial_up_vector(ground);rot=u.MathLibrary.make_rot_from_zx(up,frames['Mars'].forward)
   forward=u.MathLibrary.get_forward_vector(rot);right=u.MathLibrary.get_right_vector(rot);feet=[]
   for sx in (-1,1):
    for sy in (-1,1):
     target=ground+forward*(sx*170)+right*(sy*360)
     hit=planets['Mars'].project_point_to_surface(target);assert hit.blocking_hit
     feet.append((sx,sy,hit.impact_point))
   height=max(0,max((p-ground).dot(up) for _,_,p in feet))+12
   deck=ground+up*height;a.set_actor_location_and_rotation(deck,rot,False,False)
   for sx,sy,p in feet:
    top=deck+forward*(sx*170)+right*(sy*360);rise=(top-p).dot(up)
    name='SolarFoot_'+str(i)+'_'+str(j)+'_'+str(sx)+'_'+str(sy)
    if rise<30:
     old=aa.pop('MC_R_'+name,None)
     if old:sub.destroy_actor(old)
     continue
    support=prop(name,'Foundation_Support','Mars',(0,0,0),scale=(.4,.4,rise/100))
    support.set_actor_location_and_rotation(p,rot,False,False)
  a=aa['MC_Sector_'+sector];apply(a,'Combiner');move(a,'Mars',(x,y-2200,0),True,180)
  setp(a,completed_prompt=sector+' 区已修复 · 等待风扫清尘后供电',inactive_material=amber,active_material=green)
  a.status_light.set_static_mesh(meshes['Status_Light']);a.status_light.set_relative_location(u.Vector(0,-35,120),False,False)
  readable('Sector'+sector,'Mars',(x+450,y-2200,112),'光伏 '+sector+' 区 · 检修规程','汇流箱接头已烧毁。长按 E 使用飞船仓库的金属 6、晶体 2 接通线路。\n修复后等候风扫清尘；A、B、C 三区全部修复且清尘后，站内电梯恢复供电。\n面板下方有穴虱，注意它们的翻滚冲撞。',surface=True)
  sign('Sector'+sector,'Mars',(x,y-2250,240),'PV / '+sector+'\n汇流箱 · 长按 E 检修',28,yaw=-90,surface=True)
  # Cables are independent surface attachments with radial frames at each section.
  start=(x,y-2200);end=(0,-1400);dx,dy=end[0]-start[0],end[1]-start[1];steps=max(1,int(math.hypot(dx,dy)/360))
  for n in range(steps):
   t=(n+.5)/steps;prop('Cable_'+sector+str(n),'Pipe_Run','Mars',(start[0]+dx*t,start[1]+dy*t,12),math.degrees(math.atan2(dy,dx)),True,False,scale=(math.hypot(dx,dy)/steps/400,.8,.8))
 dome_origin=position('Mars',(0,-3900,0),True)
 # All dome contents share the same foundation plane, leaving the original ground intact.
 f=frames['Mars'];samples=[]
 for x in range(-1600,1601,400):
  for y in range(-1600,1601,400):
   if x*x+y*y<1850**2:
    surface=position('Mars',(x,-3900+y,0),True)
    samples.append((surface-position('Mars',(x,-3900+y,0))).dot(f.up))
 dz=max(samples)+20
 for part in ('Glazing','Ribs','Footing'):prop('Dome_'+part,'Dome_'+part,'Mars',(0,-3900,dz),collision=part!='Glazing')
 for i in range(16):
  angle=i*math.tau/16;x=1700*math.cos(angle);y=-3900+1700*math.sin(angle)
  ground=position('Mars',(x,y,0),True);ground_z=(ground-position('Mars',(x,y,0))).dot(f.up)
  prop('Dome_Pier_'+str(i),'Foundation_Support','Mars',(x,y,0),i*22.5,True,True,scale=(1,1,max(.2,(dz+15-ground_z)/100)))
 for x in range(-1600,1601,400):
  for y in range(-1600,1601,400):
   if x*x+y*y<1650**2:prop('DomeDeck_'+str(x)+'_'+str(y),'Deck_Tile','Mars',(x,-3900+y,dz+35))
 # Low-clearance steps lead from the real terrain to the level foundation.
 for side in (-1,1):
  door_y=-3900+side*1800;ground=position('Mars',(0,door_y+side*300,0),True)
  ground_z=(ground-position('Mars',(0,door_y+side*300,0))).dot(f.up)
  rise=max(0,dz+35-ground_z);count=max(3,int(math.ceil(rise/28)))
  for i in range(count):
   y=door_y+side*(count-i)*55
   prop('Dome_Stair_'+str(side)+'_'+str(i),'Deck_Tile','Mars',(0,y,ground_z+(dz+35-ground_z)*(i+1)/count),collision=True,scale=(1,.14,1))
 # Two human doors, the east breach remains open as the soil-factory clue.
 prop('Dome_Airlock','Door_Frame','Mars',(0,-5700,dz+35))
 prop('Dome_Entry','Door_Frame','Mars',(0,-2100,dz+35),180)
 for row,x in enumerate((-1000,-600,600,1000)):
  for n,y in enumerate((-4700,-4200,-3700,-3200)):
   prop('Dome_Grow_'+str(row)+'_'+str(n),'Grow_Bed','Mars',(x,y,dz+35),90)
 for i,x in enumerate((-650,-200,250,700)):
  prop('Dome_Nursery_'+str(i),'Nursery_Rack','Mars',(x,-4950,dz+35))
 for i,y in enumerate((-4200,-3700)):
  prop('Dome_Recycler_'+str(i),'Water_Recycler','Mars',(-1350,y,dz+35),90)
 prop('Dome_Lab','Lab_Bench','Mars',(1150,-4400,dz+35),-90)
 for i,x in enumerate((-750,750)):prop('Dome_Seed_'+str(i),'Seed_Rack','Mars',(x,-2750,dz+35))
 for x in (-350,350):
  prop('Dome_Bench_'+str(x),'Bench','Mars',(x,-3500,dz+35),90)
  light('Dome_Light_'+str(x),'Mars',(x,-3900,dz+650),3500,1500)
 sign('Dome_Entrance','Mars',(0,-2050,dz+520),'赤原生态站\nRED PLAINS / BIOREGENERATION',42,yaw=-90)
 sign('Dome_Loop','Mars',(-1150,-3900,dz+230),'水循环 → 种植 → 氧气\nH2O / FOOD / O2',24)
 readable('DomeOperations','Mars',(1050,-4400,dz+145),'生态球 · 失压后的最后一班','这是食物、水和氧气循环的中枢。破口导致整个温室失压，剩余作物已冻死。\n地下 B1 菌丝工坊、B2 授粉温室、B3 鱼菜共生池、B4 土壤工厂各有一名主管。\n先恢复地表三区供电，再到地下寻找主管工牌。种子库只要三张工牌就能开。')
 for name,key,xyz,title,rewards in [('SeedReserve','Cargo_Crate',(-600,-2800,dz+35),'回收密封食物储备',{u.JTSResourceType.FOOD:8}),('WaterReserve','Water_Recycler',(-1350,-3100,dz+35),'抽取应急净水',{u.JTSResourceType.WATER:8}),('ServiceParts','Cargo_Crate',(1100,-3000,dz+35),'回收温室检修材料',{u.JTSResourceType.ORE:18,u.JTSResourceType.CRYSTAL:6})]:
  a=actor(u.JTSFacilityController,'Recovery_'+name,'Mars',xyz);apply(a,key)
  setp(a,completed_flag='Mars_Recovery_'+name,display_name=title,repair_seconds=2,completion_rewards=rewards,completed_prompt='储备已入飞船共享仓库 · 本次远征已领取',inactive_material=amber,active_material=green)
  a.status_light.set_static_mesh(meshes['Status_Light']);a.status_light.set_relative_location(u.Vector(0,-35,80),False,False)
 # East breach contains displaced trays, frozen remains and the direction back to B4.
 for i in range(3):prop('Dome_Breach_'+str(i),'Wreck_Salvage','Mars',(1500+i*120,-3900+(i-1)*230,dz+30),i*35,False,False)
 prop('Dome_Frozen_Woodlouse','Woodlouse','Mars',(1300,-3500,dz+50),40,False,False)
 prop('Survey_Rover','Survey_Rover','Mars',(-1550,-1300,0),35,True)
 prop('Weather_Mast','Weather_Mast','Mars',(-2400,-2100,0),0,True)
 prop('Rescue_Antenna','Rescue_Antenna','Mars',(1600,-2000,0),0,True)
 prop('Lift_Housing','Station_LiftShaft','Mars',(0,-1100,0),0,True)
 lift=aa['MC_Station_Elevator'];apply(lift,'Lift_Console');move(lift,'Mars',(-230,-1500,0),True,180)
 setp(lift,locked_prompt='电梯无电 · 修复 A/B/C 汇流箱，并等待风扫清尘')
 for a in list(aa.values()):
  if isinstance(a,u.JTSInteriorTransit):
   if a!=lift:
    old=a.get_actor_location();apply(a,'Lift_Console');a.set_actor_location(old,False,False)
   if str(a.get_editor_property('display_name')).find('货笼')>=0:setp(a,locked_prompt='货笼锁定 · 摧毁至少两座机械巢')
 for name,k,xyz,yaw in [('Hub_Elevator','Mars',(-300,-300,-3200),180),('B4_Shortcut','Mars',(600,-4200,-3200),180),('Noctis_Entrance','Noctis',(0,0,0),180),('Noctis_Return','Noctis',(0,-800,-2800),180),('Furnace_Return','Noctis',(-7600,500,-2800),180),('IceVein_Return','Noctis',(7600,500,-2800),180),('Camp_Return','Noctis',(500,7600,-2800),180),('Noctis_Cage','Noctis',(-150,800,-2800),180),('Fabricator_Return','Noctis',(6400,-3400,-4000),180),('Deimos_Entrance','MarII',(0,0,0),180),('Deimos_Return','MarII',(-500,-500,-2200),180)]:
  move(aa['MC_'+name],k,xyz,name in ('Noctis_Entrance','Deimos_Entrance'),yaw)
 for i,y in enumerate(range(-1700,1200,400)):prop('Entry_Walk_'+str(i),'Deck_Tile','Mars',(0,y,35),0,True)
 for x in (-600,600):
  for y in (-1600,-600,400):light('LandingBeacon'+str(x)+str(y),'Mars',(x,y,140),600,350,(1,.30,.05))
 readable('Arrival','Mars',(600,-1200,105),'赤原站 · 救援指引','01 太阳能板阵 A/B/C：每区汇流箱需金属 6、晶体 2，长按 E 维修。\n02 修复后等待风扫清尘，恢复电梯供电。\n03 地下四区寻找主管工牌，任意三张开启种子库。\n04 门内有大型生物。留意承重柱和它翻滚后的硬直。',surface=True)
 # Existing rooms and their true collision shells remain. Add human-scale fixtures
 # away from the central combat lanes, with distinctive department silhouettes.
 themed=[('B1',(-3000,0,-3200),'菌丝工坊','MYCELIUM / B1'),('B2',(3000,0,-3200),'授粉温室','POLLINATION / B2'),('B3',(0,3400,-3200),'鱼菜共生池','AQUAPONICS / B3'),('B4',(0,-3400,-3200),'土壤工厂','REGOLITH / B4')]
 for name,(cx,cy,z),title,en in themed:
  for x in range(-800,801,400):
   for y in range(-1200,1201,400):
    if name=='B3' and abs(x)<700 and abs(y)<700:continue
    prop(name+'_Deck_'+str(x)+'_'+str(y),'Deck_Tile','Mars',(cx+x,cy+y,z+4),collision=False)
  for sx in (-1,1):
   for y in range(-1000,1001,400):
    if abs(y)<210:continue
    prop(name+'_WallX_'+str(sx)+'_'+str(y),'Interior_Panel','Mars',(cx+sx*990,cy+y,z),-90 if sx<0 else 90,False,False)
  for sy in (-1,1):
   for x in (-800,-400,400,800):
    prop(name+'_WallY_'+str(sy)+'_'+str(x),'Interior_Panel','Mars',(cx+x,cy+sy*1390,z),0 if sy<0 else 180,False,False)
  for sx in (-1,1):
   for n in range(5):
    key={'B1':'Mycelium_Vat','B2':'Nursery_Rack','B3':'Grow_Bed','B4':'Soil_Processor'}[name]
    prop(name+'_Machine_'+str(sx)+str(n),key,'Mars',(cx+sx*700,cy-900+n*450,z+5),90 if sx<0 else -90)
   for n in range(3):prop(name+'_Pipe_'+str(sx)+str(n),'Pipe_Run','Mars',(cx+sx*920,cy-800+n*800,z+360),90,False,False)
  prop(name+'_Desk','Lab_Bench','Mars',(cx+300,cy+1050,z+5))
  prop(name+'_Storage','Seed_Rack','Mars',(cx-500,cy+1100,z+5))
  sign(name+'_Header','Mars',(cx,cy-1100,z+450),title+'\n'+en,42,yaw=-90)
  text={'B1':'孢子警报：装备腰灯后进入。菌丝已经爬进过滤器。主管留下的工牌在工作台附近。',
   'B2':'绒蛾追逐腰灯。关闭灯光再穿过授粉区，避免惊动整群。主管工牌在北侧。',
   'B3':'薄冰承载上限：两名乘员。第三人踏上会开裂坍塌。观察水下冰鳃鱼的预警。',
   'B4':'地面起尘时立即让开，蠕虫即将破土。塌方通道可回到太阳能板阵。'}[name]
  readable(name+'Safety','Mars',(cx+300,cy+1000,z+118),title+' · 值班提示',text)
  for n in range(3):light(name+'_WorkLight'+str(n),'Mars',(cx,cy-800+n*800,z+500),1800,850)
 for a in aa.values():
  if isinstance(a,u.JTSReadableDocument) and not isinstance(a,u.JTSMerchant) and not a.get_actor_label().startswith('MC_R_'):apply(a,'Data_Pad')
 # Main hub guides the player with four large direction labels and floor strips.
 for name,xyz,title,yaw in [('West',(-520,0,-2840),'B1 ← 菌丝工坊',0),('East',(520,0,-2840),'B2 → 授粉温室',180),('North',(0,520,-2840),'B3 / 鱼菜共生池',-90),('South',(0,-520,-2840),'B4 / 土壤工厂',90)]:
  sign('Hub'+name,'Mars',xyz,title,30,yaw)
 for x in (-350,350):prop('Hub_Bench'+str(x),'Bench','Mars',(x,-400,-3195))
 prop('Hub_Status','Lab_Bench','Mars',(0,400,-3195))
 readable('HubMap','Mars',(0,380,-3075),'地下设施导览','B1 西：菌丝工坊。需要腰灯过滤孢子。\nB2 东：授粉温室。绒蛾会被灯光吸引。\nB3 北：鱼菜共生池。薄冰仅承载两人。\nB4 南：土壤工厂。注意地面起尘，塌方通道可回地表。\n集齐任意三张主管工牌，可开启 B3 东侧种子库。')
 setp(aa['MC_SeedVault_Door'],locked_prompt='种子库封存 · 需要任意三张主管工牌',completed_prompt='种子库已开启')
 apply(aa['MC_SeedVault_Door'],'Vault_Door');move(aa['MC_SeedVault_Door'],'Mars',(1900,3400,-3200),False,90)
 prop('Vault_Frame','Vault_Frame','Mars',(1900,3400,-3200),90)
 for i,(x,y) in enumerate(((-3400,800),(3400,800),(500,4200),(-500,-4200)),1):
  prop('Supervisor_Desk'+str(i),'Lab_Bench','Mars',(x+150,y,-3200))
  move(aa['MC_Badge_B'+str(i)],'Mars',(x+150,y-15,-3090))
 # Corridors get repeated structural ribs and lights; no prop blocks a doorway.
 for name,k,cx,cy,z,length,axis in [('West','Mars',-1300,0,-3200,1400,0),('East','Mars',1300,0,-3200,1400,0),('North','Mars',0,1300,-3200,1400,1),('South','Mars',0,-1300,-3200,1400,1),('Furnace','Noctis',-4000,0,-2800,5400,0),('Ice','Noctis',4000,0,-2800,5400,0),('Camp','Noctis',0,4000,-2800,5400,1)]:
  for i in range(int(length/450)):
   d=-length/2+225+i*450;xx=cx+(d if axis==0 else 0);yy=cy+(d if axis==1 else 0)
   prop('Rib_'+name+str(i),'Corridor_Rib',k,(xx,yy,z),90 if axis==0 else 0)
   if k=='Noctis':prop('RockArch_'+name+str(i),'Rock_Arch',k,(xx,yy,z),90 if axis==0 else 0,False,False)
   if i%2==0:light('Corridor_'+name+str(i),k,(xx,yy,z+340),600,600,(.85,.57,.31) if name=='Furnace' else (.5,.72,.8))
 # Badge, collar and chip keep the original server-owned pickup classes/rewards.
 for name,key in [('Mars_Badge_'+b,'Supervisor_Badge') for b in ('B1','B2','B3','B4')]+[('Mars_TrackingCollar','Tracking_Collar'),('Mars_LogisticsChip','Logistics_Chip')]:
  b=u.load_asset(BP+'/BP_Key_'+name);c=u.get_default_object(b.generated_class());apply(c,key,False);u.BlueprintEditorLibrary.compile_blueprint(b);save(b)
 for a in aa.values():
  if isinstance(a,u.JTSExpeditionKeyPickup):
   key=str(a.get_editor_property('important_item_id'))
   apply(a,'Supervisor_Badge' if 'Badge' in key else 'Tracking_Collar' if 'Collar' in key else 'Logistics_Chip',False)
 for name,key,scale in [('Woodlouse','Woodlouse',1),('Larva','Woodlouse',.4),('Moth','Dust_Moth',1),('DuskMoth','Dust_Moth',1),('Icegill','Icegill',1),('Worm','Burrow_Worm',1),('HeavyMiner','Heavy_Miner',1),('FastMiner','Fast_Miner',1),('FeignMiner','Feign_Miner',1)]:
  b=u.load_asset(BP+'/BP_Mars_'+name);c=u.get_default_object(b.generated_class());t=c.get_component_by_class(u.JTSCreatureTacticsComponent)
  setp(t,body_mesh=meshes[key],body_material=None,body_size=u.Vector(scale,scale,scale),body_offset=u.Vector(0,0,0),stunned_body_offset=u.Vector(0,0,70*scale));u.BlueprintEditorLibrary.compile_blueprint(b);save(b)
 for a in aa.values():
  if a.get_actor_label().startswith('MC_Camera_'):apply(a,'Security_Camera')
 for name in ('Furnace','IceVein','Camp'):
  a=aa['MC_Nest_'+name] if 'MC_Nest_'+name in aa else None
  if a:
   c=a.refuge_mesh
   if c:c.set_static_mesh(meshes['Robot_Nest']);setp(c,override_materials=[]);c.set_relative_scale3d(u.Vector(1,1,1));c.set_relative_location(u.Vector(0,0,0),False,False)
 prop('Headframe','Noctis_Headframe','Noctis',(0,0,0),surface=True)
 prop('Freight_Cage','Freight_Cage','Noctis',(0,1000,-2800))
 prop('EmptyRoom_Sphere','Empty_Spherical_Liner','Noctis',(7200,2200,-2800),collision=False)
 prop('EmptyRoom_Suit','Empty_Suit','Noctis',(7250,2300,-2790),20,False,False)
 prop('Ice_Drill','Ice_Drill','Noctis',(7300,-500,-2800),90)
 prop('Habitat_Wreck','Dismantled_Habitat','Noctis',(-200,7200,-2800),45)
 for i in range(9):prop('Ice_Shards_'+str(i),'Ice_Shards','Noctis',(1900+i*550,190 if i%2 else -190,-2800),i*37,False,False)
 corebp=u.load_asset(BP+'/BP_FabricatorCore');core=u.get_default_object(corebp.generated_class());apply(core,'Core_Pod');u.BlueprintEditorLibrary.compile_blueprint(corebp);save(corebp)
 assemblybp=u.load_asset(BP+'/BP_FabricatorAssembly');assembly=u.get_default_object(assemblybp.generated_class());apply(assembly,'Assembly_Point');assembly.mesh.set_relative_location(u.Vector(),False,False);u.BlueprintEditorLibrary.compile_blueprint(assemblybp);save(assemblybp)
 for i in range(3):prop('Furnace'+str(i),'Furnace_Bank','Noctis',(-7200,-550+i*500,-2795),90)
 for i in range(4):prop('Camp_Wreck'+str(i),'Wreck_Salvage','Noctis',(-550+(i%2)*1100,6800+(i//2)*800,-2790),i*55)
 sign('NoctisDirections','Noctis',(0,0,-2350),'熔炉 ← 机械巢 / 冰脉 → 机械巢\n北 / 居住舱拆解场\n摧毁至少两巢，中央货笼解锁',34,yaw=-90)
 readable('NoctisOperations','Noctis',(-400,-500,-2680),'井下生产网 · 应急停机','三处机械巢仍在生产矿机。摧毁两座即可解除货笼联锁。\n熔炉巢会加强母机护甲；冰脉巢会追加快速单位；拆解场巢会追加伪装成残骸的矿机。\n母机组装点亮起时，及时击破可中断增援。')
 # The deliberately clean empty chamber stays almost empty, retaining only its shell/camera.
 prop('Phobos_Rover','Survey_Rover','MarI',(-600,800,0),surface=True)
 prop('Phobos_Telescope','Telescope','MarI',(900,-1000,0),surface=True)
 prop('Phobos_Tanks','Port_Tank_Farm','MarI',(1500,1100,0),surface=True)
 prop('Phobos_Emblem','Half_Sprout','MarI',(0,-700,12),surface=True,collision=False)
 for name in [n for n in aa if n.startswith('MC_Ph obos_Sprout_'.replace(' ',''))]:sub.destroy_actor(aa.pop(name))
 for n in ('MC_Ph obos_Rover'.replace(' ',''),'MC_Ph obos_Telescope'.replace(' ','')):
  if n in aa:sub.destroy_actor(aa.pop(n))
 if 'MC_Ph obos_Fuel'.replace(' ','') in aa:
  a=aa['MC_Ph obos_Fuel'.replace(' ','')];apply(a,'Fuel_Depot')
 for a in aa.values():
  if isinstance(a,u.JTSFuelDepot):apply(a,'Fuel_Depot')
  if a.get_actor_label().endswith('_Salvage'):apply(a,'Wreck_Salvage')
 for old,key in [('MC_Jummt_Sofa','Jummt_Sofa'),('MC_Jummt_TV','Jummt_TV'),('MC_Jummt_Shelf','Seed_Rack'),('MC_Jummt_Desk','Lab_Bench')]:
  if old in aa:
   a=aa[old];apply(a,key)
   move(a,'MarII',{'MC_Jummt_Sofa':(3100,-500,-2200),'MC_Jummt_TV':(2900,-650,-2200),'MC_Jummt_Shelf':(3100,550,-2200),'MC_Jummt_Desk':(2300,600,-2200)}[old],False,180)
 for a in aa.values():
  if isinstance(a,u.JTSMerchant):apply(a,'Jummt_Suit',False);move(a,'MarII',(2700,200,-2200),False,180)
  if a.get_actor_label()=='MC_Jummt_Visor':sub.destroy_actor(a)
 # The collector has repaired one small living pocket inside the rough cave.
 for x in range(1600,3201,400):
  for y in range(-800,801,400):prop('Jummt_Deck_'+str(x)+'_'+str(y),'Deck_Tile','MarII',(x,y,-2197),collision=False)
 for x in (1800,2200,2600,3000):
  prop('Jummt_Backwall_'+str(x),'Interior_Panel','MarII',(x,990,-2200),180,False,False)
 for i in range(8):
  paper=aa.get('MC_Jummt_Paper_'+str(i))
  if paper:move(paper,'MarII',(2250+(i%4)*25,580+(i//4)*25,-2090),False,180)
 for i in range(3):prop('Jummt_Cargo_'+str(i),'Cargo_Crate','MarII',(1700+i*160,700,-2200),i*12)
 prop('Jummt_SalvagedRover','Wreck_Salvage','MarII',(1850,-650,-2200),-25)
 sign('Jummt_Home','MarII',(2600,970,-1870),'旅人收藏室 / TRAVELLER COLLECTION',22,yaw=-90)
 for i in range(5):light('Jummt_String'+str(i),'MarII',(1800+i*300,650,-1600),250,400,(1,.55,.20))
 for a in list(aa.values()):
  if isinstance(a,u.JTSFuelDepot):move(a,'MarI',(700,600,0),True,180)
  if isinstance(a,u.JTSReadableDocument) and not isinstance(a,u.JTSMerchant) and not a.get_actor_label().startswith('MC_R_'):
   label='MC_R_LogStand_'+a.get_actor_label();stand=aa.get(label)
   if not stand:stand=sub.spawn_actor_from_class(u.StaticMeshActor,a.get_actor_location(),a.get_actor_rotation());stand.set_actor_label(label);aa[label]=stand
   apply(stand,'Log_Pedestal');stand.set_actor_location_and_rotation(a.get_actor_location()-a.get_actor_up_vector()*111,a.get_actor_rotation(),False,False);stand.set_folder_path('MarsChapter/Refined/Logs')
 if 'MC_R_LogStand_MC_Jummt' in aa:sub.destroy_actor(aa.pop('MC_R_LogStand_MC_Jummt'))
 # Space encounter archetypes retain the same server damage/splitting rules.
 spaceclasses=[]
 for i,key in enumerate(('Asteroid_Shard','Asteroid_Porous','Asteroid_Crescent')):
  name='BP_Space_Asteroid'+('' if i==0 else '_'+str(i+1));b=u.load_asset(BP+'/'+name)
  if not b:
   factory=setp(u.BlueprintFactory(),parent_class=u.JTSSpaceEncounterUnit);b=at.create_asset(name,BP,u.Blueprint,factory)
  c=u.get_default_object(b.generated_class());c.mesh.set_static_mesh(meshes[key]);c.mesh.set_relative_scale3d(u.Vector(1,1,1));setp(c.mesh,override_materials=[])
  setp(c,light_sail_ray=False,size_tier=2,salvage={u.JTSResourceType.ROCK:3,u.JTSResourceType.ORE:1});setp(c.health,max_health=65)
  u.BlueprintEditorLibrary.compile_blueprint(b);save(b);spaceclasses.append(b.generated_class())
 b=u.load_asset(BP+'/BP_Space_LightSailRay');c=u.get_default_object(b.generated_class());c.mesh.set_static_mesh(meshes['Light_Sail_Ray']);c.mesh.set_relative_scale3d(u.Vector(1,1,1));setp(c.mesh,override_materials=[]);setp(c,wing_root_half_width=60,warning_seconds=1.25)
 u.BlueprintEditorLibrary.compile_blueprint(b);save(b);spaceclasses.append(b.generated_class());setp(aa['MC_Mars_EncounterDirector'],unit_classes=spaceclasses)
 # Faceted seams belong to breakable actors, never celestial surfaces.
 for n in ('MC_Deimos_Wall','MC_EmptyRoom_Wall'):
  if n in aa:
   a=aa[n];apply(a,'Fractured_Seam');a.mesh.set_relative_location(u.Vector(0,0,-200),False,False)
   if n=='MC_Deimos_Wall':a.mesh.set_relative_rotation(u.Rotator(yaw=90),False,False)
 def haze(name,color,opacity):
  m=material(name,glass=True,color=color)
  if u.EditorAssetLibrary.get_metadata_tag(m,'MarsHazeRevision')=='4':return m
  mel=u.MaterialEditingLibrary;mel.delete_all_material_expressions(m)
  setp(m,blend_mode=u.BlendMode.BLEND_TRANSLUCENT,two_sided=True)
  tint=mel.create_material_expression(m,u.MaterialExpressionConstant3Vector);setp(tint,constant=u.LinearColor(*color,1))
  mel.connect_material_property(tint,'',u.MaterialProperty.MP_BASE_COLOR)
  world=mel.create_material_expression(m,u.MaterialExpressionWorldPosition);origin=mel.create_material_expression(m,u.MaterialExpressionActorPositionWS)
  p=mel.create_material_expression(m,u.MaterialExpressionSubtract);mel.connect_material_expressions(world,'',p,'A');mel.connect_material_expressions(origin,'',p,'B')
  t=mel.create_material_expression(m,u.MaterialExpressionTime)
  inputs=[]
  for key in ('P','T'):
   item=u.CustomInput();setp(item,input_name=key);inputs.append(item)
  alpha=mel.create_material_expression(m,u.MaterialExpressionCustom)
  setp(alpha,inputs=inputs,output_type=u.CustomMaterialOutputType.CMOT_FLOAT1,code='float3 q=P*.025;return '+str(opacity)+'*(.25+.75*saturate(sin(q.x+T*.7)*sin(q.y*1.7-T*.2)*sin(q.z*.5+T*.4)+.45));',description='Slowly drifting sparse haze')
  mel.connect_material_expressions(p,'',alpha,'P');mel.connect_material_expressions(t,'',alpha,'T');mel.connect_material_property(alpha,'',u.MaterialProperty.MP_OPACITY)
  mel.recompile_material(m);u.EditorAssetLibrary.set_metadata_tag(m,'MarsHazeRevision','4');return save(m)
 dust=haze('M_MP_Dust',(.50,.28,.13),.035);spore=haze('M_MP_Spores',(.53,.62,.29),.045)
 for a in list(aa.values()):
  if not isinstance(a,u.JTSFacilityHazard):continue
  kind=a.get_editor_property('kind');c=a.visual
  if kind==u.JTSFacilityHazardKind.DUST_SWEEP:
   deimos='Deimos' in a.get_actor_label()
   if deimos:move(a,'MarII',(0,0,0),True)
   else:
    i='ABC'.index(a.get_actor_label()[-1]);x,y=((-4900,3800),(0,6200),(4900,3800))[i];move(a,'Mars',(x,y,20),True)
   c.set_static_mesh(meshes['Dust_Plume']);c.set_relative_scale3d(u.Vector(1.5,1.5,4) if deimos else u.Vector(12,12,.35));setp(c,override_materials=[dust])
   c.set_visibility(False)
  elif kind==u.JTSFacilityHazardKind.FALLING_ICE:
   c.set_static_mesh(meshes['Ice_Shards']);c.set_relative_scale3d(u.Vector(.8,.8,.9));setp(c,override_materials=[])
  elif kind==u.JTSFacilityHazardKind.SPORE_FOG:
   c.set_static_mesh(meshes['Spore_Cloud']);c.set_relative_scale3d(u.Vector(1.2,1.2,1.2));c.set_relative_location(u.Vector(0,0,-260),False,False);setp(c,override_materials=[spore])
   for x in (-600,0,600):
    for y in (-850,0,850):
     if x==0 and y==0:continue
     fog=prop('B1_Spores_'+str(x)+'_'+str(y),'Spore_Cloud','Mars',(-3000+x,y,-3160),collision=False,scale=(1.2,1.2,1.2))
     fog.static_mesh_component.set_material(0,spore)
 for name,intensity in [('B1',900),('B2',1100),('B3',950),('B4',850),('Hub',900),('SeedVault',700),('NoctisHall',800),('FabricatorHall',1500),('Deimos_A',500),('Deimos_B',600),('EmptyRoom',550)]:
  a=aa.get('MC_'+name+'_Light')
  if a:a.point_light_component.set_intensity(intensity)
 # Keep the power rule, but retire its prototype mesh. Streaming may restore
 # actor visibility/collision, so hiding the actor alone is insufficient.
 if 'MC_Power' in aa:aa['MC_Power'].mesh.set_static_mesh(None)
 u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
 bosses=runpy.run_path(str(Path(__file__).with_name('refine_mars_bosses.py')))['refine']()
 motion=runpy.run_path(str(Path(__file__).with_name('refine_mars_motion.py')))['refine']()
 runpy.run_path(str(Path(__file__).with_name('refine_mars_access.py')))['refine']()
 return {'modules':len(meshes),'refined_actors':sum(n.startswith('MC_R_') for n in aa),'bosses':bosses,'palette_slots_max':3,'original_planets_untouched':True}

def review():
 path=Path(u.Paths.project_dir()).resolve()/'Saved/MarsReview.txt'
 view=path.read_text(encoding='utf-8-sig').strip() if path.exists() else 'station'
 if view.startswith('access_'):
  return runpy.run_path(str(Path(__file__).with_name('refine_mars_access.py')))[view.removeprefix('access_')]()
 if view.startswith('lift_'):
  return runpy.run_path(str(Path(__file__).with_name('refine_mars_lift_room.py')))[view.removeprefix('lift_')]()
 sub,aa,planets,frames=context()
 if view=='solar_audit':
  panels=[a for a in aa.values() if a.get_actor_label().startswith('MC_Solar_') and isinstance(a,u.StaticMeshActor)]
  return {'panel_count':len(panels),'minimum_radial_alignment':min(a.get_actor_up_vector().dot(planets['Mars'].get_radial_up_vector(a.get_actor_location())) for a in panels),'footing_max_height':max(a.get_actor_scale3d().z for a in aa.values() if a.get_actor_label().startswith('MC_R_SolarFoot_'))}
 if view=='assets':
  result={}
  for name in ('Tuantuan','Fabricator'):
   c=u.get_default_object(u.load_asset(BP+'/BP_Mars_'+name).generated_class());p=c.get_component_by_class(u.JTSBossPhaseComponent)
   result[name]={'phases':len(p.get_editor_property('phases')),'clips':len(c.get_editor_property('animations')),'flip_offset':str(p.get_editor_property('flipped_mesh_offset')),'mesh':c.mesh.get_skeletal_mesh_asset().get_name(),'hp':c.health.get_editor_property('max_health')}
  return result
 if view=='art_preview':
  # Temporary editor preview uses a dedicated folder and is excluded from saves.
  return {'motion_materials':{n:str(u.load_asset(ROOT+'/SM_MP_'+n).get_material(0)) for n in ('Dust_Moth','Light_Sail_Ray','Burrow_Worm')},'effects':{n:{'blend':str(u.load_asset(ROOT+'/'+n).get_editor_property('blend_mode')),'opacity':str(u.MaterialEditingLibrary.get_material_property_input_node(u.load_asset(ROOT+'/'+n),u.MaterialProperty.MP_OPACITY))} for n in ('M_MP_Dust','M_MP_Spores')}}
 views={'station':('Mars',(4500,-7500,2700),(0,-3000,500)), 'dome':('Mars',(100,-5200,230),(0,-3650,160)),
 'solar':('Mars',(-5600,-500,600),(-4900,3500,150)), 'B1':('Mars',(-3000,-1050,-2990),(-3000,300,-2990)),
 'B2':('Mars',(3000,-1050,-2990),(3000,300,-2990)),'B3':('Mars',(0,2350,-2990),(0,3700,-2990)),
 'B4':('Mars',(0,-4450,-2990),(0,-3200,-2990)),'noctis':('Noctis',(2500,-3500,1800),(0,0,700)),
 'merchant':('MarII',(2000,-600,-1940),(2700,200,-1940))}
 k,xyz,target=views[view];f=frames[k]
 if view=='dome':
  base=(aa['MC_R_Dome_Footing'].get_actor_location()-f.location).dot(f.up)
  xyz=(xyz[0],xyz[1],xyz[2]+base);target=(target[0],target[1],target[2]+base)
 def p(t):return f.location+f.forward*t[0]+f.right*t[1]+f.up*t[2]
 pos=p(xyz);rot=u.MathLibrary.make_rot_from_xz(p(target)-pos,f.up)
 u.EditorLevelLibrary.set_level_viewport_camera_info(pos,rot);u.EditorLevelLibrary.editor_set_game_view(True);u.EditorLevelLibrary.editor_invalidate_viewports()
 return {'view':view,'camera':str(pos),'actors':len(aa),'bounds':{n:str(u.load_asset(ROOT+'/SM_MP_'+n).get_bounds()) for n in ('Solar_Tracker','Grow_Bed','Combiner','Dome_Ribs')}}
