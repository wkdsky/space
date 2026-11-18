"""Configure chapter-specific boss art on existing generic combat/phase rules."""
from pathlib import Path
import json,runpy
import unreal as u

def refine():
 root=Path(u.Paths.project_dir()).resolve();source=root/'SourceArt/MarsBosses/Refined'
 helper=runpy.run_path(str(root/'Tools/Art/moon_surface_unreal.py'))
 task=helper['fbx_task'];imports=helper['import_tasks'];save=helper['save'];results={}
 palette='/Game/Space/Environment/MarsSystem/Chapter/Refined/'
 for name,data in json.loads((source/'manifest.json').read_text()).items():
  folder='/Game/Space/Creatures/'+name;meshname=data['mesh'];m=u.load_asset(folder+'/'+meshname)
  reimport=not m or u.EditorAssetLibrary.get_metadata_tag(m,'MarsBossRevision')!='2'
  if reimport:imports([task(source/(meshname+'.fbx'),folder,meshname,True)])
  m=u.load_asset(folder+'/'+meshname);assert m
  slots=list(m.materials)
  for slot in slots:slot.material_interface=u.load_asset(palette+('M_MP_Luminous' if str(slot.material_slot_name)=='MP_Luminous' else 'M_MP_Palette'))
  m.set_editor_property('materials',slots);save(m);save(m.skeleton)
  pending=[]
  for clip in data['clips']:
   asset='A_MP_'+name+'_'+clip
   if reimport or not u.load_asset(folder+'/Animations/'+asset):pending.append(task(source/(asset+'.fbx'),folder+'/Animations',asset,True,True,m.skeleton))
  if pending:imports(pending)
  forms=[]
  if name=='Tuantuan':
   for variant in ('ShedRear','Soft'):
    formname='SK_MP_Tuantuan_'+variant;form=u.load_asset(folder+'/'+formname)
    if not form:imports([task(source/(formname+'.fbx'),folder,formname,True,False,m.skeleton)])
    form=u.load_asset(folder+'/'+formname);form.set_editor_property('materials',slots);save(form);forms.append(form)
  bp=u.load_asset('/Game/Space/Blueprints/Planets/Mars/Chapter/BP_Mars_'+name);c=u.get_default_object(bp.generated_class());c.mesh.set_skeletal_mesh_asset(m);c.mesh.set_relative_scale3d(u.Vector(1,1,1));c.mesh.set_relative_location(u.Vector(),False,False);c.mesh.set_relative_rotation(u.Rotator(),False,False);c.mesh.set_editor_property('override_materials',[])
  actions={}
  for clip in data['clips']:
   a=u.load_asset(folder+'/Animations/A_MP_'+name+'_'+clip);assert a,clip
   actions[getattr(u.JTSBossAction,{'TurnLeft':'TURN_LEFT','TurnRight':'TURN_RIGHT'}.get(clip,clip.upper()))]=a;save(a)
  c.set_editor_property('animations',actions)
  zones=[]
  rows=[('Body',(0,0,110),(235,150,85),1,True),('Head',(255,0,110),(55,90,65),1,False),('Body',(0,0,55),(160,110,30),1,False)] if name=='Tuantuan' else [('Body',(0,0,430),(235,235,270),1,True)]+[('Arm'+str(i),(0,0,740),(70,70,70),.8,False) for i in range(4)]
  for bone,center,extent,mult,block in rows:
   z=u.JTSBossHitZone();z.set_editor_property('bone',bone);z.set_editor_property('center',u.Vector(*center));z.set_editor_property('half_extent',u.Vector(*extent));z.set_editor_property('damage_multiplier',mult);z.set_editor_property('blocks_movement',block);zones.append(z)
  c.set_editor_property('hit_zones',zones)
  steering=c.get_component_by_class(u.JTSPlanetSurfaceSteeringComponent);steering.set_editor_property('body_radius',165 if name=='Tuantuan' else 260);steering.set_editor_property('body_ground_clearance',15)
  phase=c.get_component_by_class(u.JTSBossPhaseComponent)
  phase.set_editor_property('flipped_mesh_offset',u.Vector(0,0,280) if name=='Tuantuan' else u.Vector())
  def attack(windup,active,recovery,damage,reach=500,radius=250):
   a=u.JTSBossAttack()
   for k,v in dict(windup=windup,active=active,recovery=recovery,damage=damage,reach=reach,radius=radius,cooldown=5).items():a.set_editor_property(k,v)
   return a
  def stage(threshold,walk,charge,attacks,summon=None,count=0):
   p=u.JTSBossPhase()
   for k,v in dict(enter_below_health=threshold,walk_speed=walk,charge_speed=charge,attacks=attacks,summon_class=summon,summon_count=count).items():p.set_editor_property(k,v)
   return p
  enum=u.JTSBossAction
  if name=='Tuantuan':
   larva=u.load_class(None,'/Game/Space/Blueprints/Planets/Mars/Chapter/BP_Mars_Larva.BP_Mars_Larva_C')
   phases=[stage(1,160,1000,{enum.CHARGE:attack(1.3,2,1.5,18,900)}),stage(.66,190,1000,{enum.SLAM:attack(1.2,.4,1.4,20,500,350)},larva,4),stage(.33,230,1250,{enum.CHARGE:attack(1,1.6,1.2,22,1000),enum.SWEEP:attack(1,.5,1.2,16,650,350)}),stage(.18,230,1250,{enum.CHARGE:attack(1,1.6,1.2,22,1000),enum.SWEEP:attack(1,.5,1.2,16,650,350)})]
   for p,form in zip(phases[2:],forms):p.set_editor_property('form',form);p.set_editor_property('damage_multiplier',1.5)
   phase.set_editor_property('cooperative_flip',True);phase.set_editor_property('phases',phases)
  else:
   fast=u.load_class(None,'/Game/Space/Blueprints/Planets/Mars/Chapter/BP_Mars_FastMiner.BP_Mars_FastMiner_C')
   phases=[stage(1,0,0,{enum.SLAM:attack(1.2,.5,1.2,25,1500,600),enum.SWEEP:attack(1.3,.8,1.5,20,1800,400)},fast,2),stage(.66,170,650,{enum.CHARGE:attack(1.3,1.8,1.3,26,500),enum.SWEEP:attack(1,.5,1.3,20,800)}),stage(.33,0,0,{enum.SLAM:attack(1.5,.4,2,15,1000,350)})]
   phases[0].set_editor_property('assembly_point_class',u.load_class(None,'/Game/Space/Blueprints/Planets/Mars/Chapter/BP_FabricatorAssembly.BP_FabricatorAssembly_C'))
   phases[1].set_editor_property('trail_hazard_class',u.load_class(None,'/Game/Space/Blueprints/Planets/Mars/Chapter/BP_FabricatorSlag.BP_FabricatorSlag_C'))
   phases[2].set_editor_property('core_copies',True);phases[2].set_editor_property('core_class',u.load_class(None,'/Game/Space/Blueprints/Planets/Mars/Chapter/BP_FabricatorCore.BP_FabricatorCore_C'));phase.set_editor_property('phases',phases)
  u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
  u.EditorAssetLibrary.set_metadata_tag(m,'MarsBossRevision','2');save(m)
  results[name]={'bounds':str(m.get_bounds()),'clips':len(actions),'hit_zones':len(zones)}
 return results
