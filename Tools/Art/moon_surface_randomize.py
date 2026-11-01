"""Configure independent runtime attachments. Never edits the Moon actor or mesh."""
import json
import unreal
import moon_surface_unreal as s

def entry(meshes,count,**values):
    e=unreal.JTSPlanetSurfaceScatterEntry()
    e.set_editor_property('meshes',[unreal.load_asset(s.MESHES+'/'+n) for n in meshes])
    e.set_editor_property('count',count)
    for k,v in values.items():e.set_editor_property(k,v)
    return e

def configure():
    s.editor_world();state=s.actors();plan=s.recipe()
    planet=next(a for a in state.values() if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='Moon')
    controller=next(a for a in state.values() if isinstance(a,unreal.JTSMoonSurfaceController) and str(a.get_planet_id())=='Moon')
    protected=[a for a in state.values() if a.get_actor_label().startswith('Moon_Nest_')]+[state['Moon_Corpse_SkeletonAstronaut']]
    before={a.get_path_name():a.get_actor_transform() for a in protected+[state['MoonPlanet'],planet]}
    rows=json.loads((s.ART/'portable_modules.json').read_text(encoding='utf-8'))
    s.import_tasks([s.fbx_task(s.ART/'FBX'/(r['name']+'.fbx'),s.MESHES,r['name']) for r in rows])
    mats=s.surface_materials()
    for r in rows:
        mesh=unreal.load_asset(s.MESHES+'/'+r['name']);mesh.set_editor_property('allow_cpu_access',True)
        slots=list(mesh.static_materials)
        for slot in slots:slot.material_interface=mats[str(slot.material_slot_name)]
        mesh.set_editor_property('static_materials',slots);s.save(mesh)
    for i in range(4):
        mesh=unreal.load_asset(s.MESHES+f'/SM_MS3_MirrorRefuge_{i}')
        mesh.set_editor_property('allow_cpu_access',True);s.save(mesh)
    feeding,cdo=s.blueprint('BP_MoonMirrorFeedingArea',unreal.JTSPlanetEcologyHabitat)
    values=dict(enemy_class=unreal.load_asset(s.BPS+'/BP_MirrorSkimmer').generated_class(),enemy_count=2,
        core_radius=500,spawn_radius=800,minimum_spacing=80,minor_axis_ratio=1,
        light_preference=unreal.JTSHabitatLight.SUNLIT,maintain_population=False,destructible_entrance=False,require_refuge=True)
    for k,v in values.items():cdo.set_editor_property(k,v)
    cdo.get_editor_property('surface_scatter').set_editor_property('entries',[
        entry([f'SM_MS3_MirrorRefuge_{i}' for i in range(4)],1,refuge=True,conform_to_surface=True,cast_shadow=True,minimum_spacing=80)])
    cdo.get_editor_property('surface_scatter').set_editor_property('owner_clearance',300)
    unreal.BlueprintEditorLibrary.compile_blueprint(feeding);s.save(feeding)
    removed=[]
    for a in list(state.values()):
        if not a.actor_has_tag(s.TAG):continue
        if isinstance(a,unreal.StaticMeshActor) or (isinstance(a,unreal.JTSPlanetEcologyHabitat) and a.get_actor_label().startswith('MoonV3_Feed_')):
            removed.append(a.get_actor_label())
    obsolete=set(removed)
    active=[a for a in controller.get_editor_property('placed_ecology_habitats') if a.get_actor_label() not in obsolete]
    for a in state.values():
        if isinstance(a,unreal.JTSPlanetEcologyHabitat):
            a.modify()
            for k in ('refuges','food_points'):
                a.set_editor_property(k,[r for r in a.get_editor_property(k) if r and r.get_actor_label() not in obsolete])
            a.get_editor_property('surface_scatter').set_editor_property('exclusion_actors',protected)
    for region_index,row in enumerate(plan['regions']):
        core=state['MoonV3_'+row['id']+'_Rift_Core']
        core.set_editor_property('enemy_count',1+region_index%2)
        entries=[entry([f'SM_MS3_RiftRim_{i}' for i in range(4)],1,refuge=True,conform_to_surface=True,cast_shadow=True,destructible=True,minimum_spacing=500,
            cut_volumes=[unreal.load_asset(s.MESHES+f'/SM_MS3_RiftCutVolume_{i}') for i in range(4)])]
        if row['food_ids']:
            entries.append(entry([f'SM_MS3_PortableFeed_{i}' for i in range(4)],len(row['food_ids']),
                conform_to_surface=True,food=True,minimum_sun_dot=.08,minimum_spacing=650,resident_habitat_class=feeding.generated_class()))
        core.get_editor_property('surface_scatter').set_editor_property('entries',entries)
        for suffix in ('_Rift_Fringe','_Mirror_Fringe'):
            fringe=state.get('MoonV3_'+row['id']+suffix)
            if fringe:fringe.set_editor_property('refuge_providers',[core])
    for tower in plan['towers']:
        nest=state['MoonV3_'+tower['id']]
        nest.set_editor_property('height_range',unreal.Vector2D(95,132))
        workers=state['MoonV3_'+tower['id']+'_Workers'];guards=state['MoonV3_'+tower['id']+'_Guards']
        candidates=[state['MoonV3_'+r['id']+'_Rift_Core'] for r in plan['regions']]
        region=min(candidates,key=lambda r:(r.get_actor_location()-nest.get_actor_location()).length())
        scatter=workers.get_editor_property('surface_scatter')
        scatter.set_editor_property('placement_region',region);scatter.set_editor_property('relocate_with_owner',[nest,guards])
        nest.get_editor_property('nest_mesh').set_mobility(unreal.ComponentMobility.MOVABLE)
    controller.modify();controller.set_editor_property('placed_ecology_habitats',active)
    planet.get_editor_property('surface_cuts').set_editor_property('cut_source_mesh',unreal.load_asset(s.MESHES+'/SM_MoonCutSource'))
    for label in removed:assert s.editor().destroy_actor(state[label])
    for a in protected+[state['MoonPlanet'],planet]:assert a.get_actor_transform()==before[a.get_path_name()]
    return json.dumps(dict(removed_fixed_props=len(removed),region_generators=len(plan['regions']),fixed_entrances_preserved=len(protected)-1,
        portable_meshes=len(rows),moon_edited=False))
