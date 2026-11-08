"""Moon placement, feeding and lighting adjustments, invoked through Unreal MCP."""
import json
import math
import unreal
import moon_surface_unreal as s
import moon_surface_refinement as r


def game_world():
    worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
    world=next((w for w in worlds if unreal.GameplayStatics.get_game_mode(w)),None)
    assert world,'No authoritative PIE world'
    return world


def accessible_nests(apply=False):
    s.editor_world();r.check_protected();state=s.actors()
    planet=next(a for a in state.values() if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='Moon')
    rows=[]
    def slope(hit):
        return math.degrees(math.acos(max(-1,min(1,unreal.MathLibrary.dot_vector_vector(hit.impact_normal,planet.get_radial_up_vector(hit.impact_point))))))
    for name in ['Moon_Nest_%02d'%i for i in range(1,10)]:
        a=state[name];origin=a.get_actor_location();radial=planet.get_radial_up_vector(origin)
        f=(a.get_actor_forward_vector()-radial*unreal.MathLibrary.dot_vector_vector(a.get_actor_forward_vector(),radial)).normal()
        right=unreal.MathLibrary.cross_vector_vector(radial,f).normal();best=None
        for radius in [0,45,75,110,160,230,320,440,600]:
            for angle_i in range(1 if radius==0 else 24):
                angle=2*math.pi*angle_i/24
                hit=planet.project_point_to_surface(origin+(f*math.cos(angle)+right*math.sin(angle))*radius)
                if not hit or slope(hit)>35:continue
                pos=hit.impact_point+hit.impact_normal*.8
                stable=planet.project_point_to_surface(pos)
                if not stable or slope(stable)>35:continue
                up=stable.impact_normal
                tangent=(f-up*unreal.MathLibrary.dot_vector_vector(f,up)).normal()
                side=unreal.MathLibrary.cross_vector_vector(up,tangent).normal()
                for heading in range(12):
                    h=2*math.pi*heading/12;forward=tangent*math.cos(h)+side*math.sin(h)
                    approach=[planet.project_point_to_surface(pos-forward*d) for d in (65,120,180)]
                    if not all(x and slope(x)<=35 for x in approach):continue
                    best=dict(pos=pos,rotation=unreal.MathLibrary.make_rot_from_xz(forward,up),slope=slope(stable),approach=slope(approach[-1]));break
                if best:break
            if best:break
        assert best,'No accessible slope foot for '+name
        if apply:
            a.modify();a.set_editor_property('can_block_entrance',True);a.set_editor_property('destructible_entrance',False)
            a.set_editor_property('block_hold_duration',2.5);a.set_editor_property('entrance_interaction_offset',unreal.Vector(-20,0,28))
            c=a.get_editor_property('refuge_mesh');c.set_world_location(best['pos'],False,True);c.set_world_rotation(best['rotation'],False,True)
        rows.append(dict(nest=name,offset_cm=round((best['pos']-origin).length(),1),slope=round(best['slope'],2),approach=round(best['approach'],2),position=s.xyz(best['pos'])))
    r.check_protected()
    return rows


def author_blocker():
    path=s.MESHES+'/SM_MS3_BlockedRefuge'
    asset=unreal.load_asset(path)
    if asset:return asset
    dm=unreal.DynamicMesh();vertices=[];triangles=[];normals=[];uv=[];steps=24
    rings=[]
    for z,rx,ry in [(.3,57,42),(23,48,37),(43,26,23)]:
        ring=[]
        for i in range(steps):
            a=2*math.pi*i/steps;w=1+.09*math.sin(3*a)+.04*math.cos(7*a)
            ring.append(unreal.Vector(-4+rx*math.sin(a)*w,ry*math.cos(a)*w,z+math.sin(5*a)*2))
        rings.append(ring)
    def triangle(a,b,c):
        base=len(vertices);vertices.extend([a,b,c]);triangles.append(unreal.IntVector(base,base+1,base+2))
        n=unreal.MathLibrary.cross_vector_vector(b-a,c-a).normal();normals.extend([n]*3)
        uv.extend([unreal.Vector2D(p.x/100,p.y/100) for p in (a,b,c)])
    for ring_i in range(2):
        a,b=rings[ring_i: ring_i+2]
        for i in range(steps):
            j=(i+1)%steps;triangle(a[i],b[j],a[j]);triangle(a[i],b[i],b[j])
    for i in range(steps):triangle(rings[-1][i],unreal.Vector(-4,0,50),rings[-1][(i+1)%steps])
    dm.append_buffers_to_mesh(unreal.GeometryScriptSimpleMeshBuffers(vertices=vertices,triangles=triangles,normals=normals,uv0=uv),0)
    asset,_=unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm,path,
        unreal.GeometryScriptCreateNewStaticMeshAssetOptions(enable_collision=False,enable_recompute_normals=True,enable_recompute_tangents=True))
    assert asset
    mat=unreal.load_asset(s.MATERIALS+'/TunnelRegolith')
    asset.set_editor_property('static_materials',[unreal.StaticMaterial(material_interface=mat,material_slot_name='Regolith')])
    s.save(asset);return asset


def configure_blocking():
    s.editor_world();r.check_protected();blocker=author_blocker()
    for a in s.actors().values():
        if not isinstance(a,unreal.JTSPlanetEcologyHabitat):continue
        a.modify();a.set_editor_property('can_block_entrance',False);a.set_editor_property('destructible_entrance',False)
        a.get_editor_property('surface_scatter').set_editor_property('slope_foot_search_radius',75)
        if a.get_actor_label().startswith('Moon_Nest_'):
            a.get_editor_property('entrance_blocker').set_static_mesh(blocker)
    bp=unreal.load_asset(s.BPS+'/BP_MoonMirrorFeedingArea')
    unreal.get_default_object(bp.generated_class()).get_editor_property('surface_scatter').set_editor_property('slope_foot_search_radius',75)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp);s.save(bp)
    rows=accessible_nests(True)
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return dict(nests=rows,only_nine_blockable=True,hold_seconds=2.5,protected=r.check_protected())


def audit_blocking():
    world=game_world();actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
    planet=next(a for a in actors if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='Moon')
    pawn=unreal.GameplayStatics.get_player_pawn(world,0)
    marked=[a for a in actors if isinstance(a,unreal.JTSPlanetEcologyHabitat) and a.get_editor_property('can_block_entrance')]
    assert len(marked)==9 and all(a.get_actor_label().startswith('Moon_Nest_') for a in marked)
    rows=[]
    for a in sorted(marked,key=lambda a:a.get_actor_label()):
        mesh=a.get_editor_property('refuge_mesh');hit=planet.project_point_to_surface(mesh.get_world_location())
        normal=unreal.MathLibrary.dot_vector_vector(hit.impact_normal,planet.get_radial_up_vector(hit.impact_point))
        row=dict(nest=a.get_actor_label(),slope=round(math.degrees(math.acos(max(-1,min(1,normal)))),2),
            can_interact=a.call_method('CanInteract',(pawn,)),blocked=a.is_entrance_blocked(),
            plug_visible=a.get_editor_property('entrance_blocker').is_visible())
        assert row['slope']<=35 and (row['can_interact'] or row['blocked']),row
        rows.append(row)
    return dict(nests=rows,blockable_total=len(marked),runtime_cuts=planet.get_editor_property('surface_cuts').get_cut_count(),protected=r.check_protected({a.get_actor_label():a for a in actors}))


def blocking_play():
    world=game_world();pawn=unreal.GameplayStatics.get_player_pawn(world,0)
    pc=unreal.GameplayStatics.get_player_controller(world,0)
    component=pawn.get_component_by_class(unreal.InteractionComponent)
    nests=sorted([a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat)
        if a.get_editor_property('can_block_entrance')],key=lambda a:a.get_actor_label())
    saved=pawn.get_actor_transform();previous=pc.get_view_target();movement=pawn.character_movement
    mode=movement.get_editor_property('movement_mode');movement.disable_movement()
    lib=unreal.get_default_object(unreal.GameplayStatics.static_class())
    def temporary(cls):
        pose=unreal.Transform()
        a=lib.call_method('BeginDeferredActorSpawnFromClass',(world,cls.static_class(),pose,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        return lib.call_method('FinishSpawningActor',(a,pose,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    camera=temporary(unreal.CameraActor);pc.set_view_target_with_blend(camera,0)
    capture=temporary(unreal.SceneCapture2D);lamp=temporary(unreal.PointLight)
    light=lamp.point_light_component;light.set_mobility(unreal.ComponentMobility.MOVABLE)
    light.set_editor_property('intensity_units',unreal.LightUnits.CANDELAS);light.set_intensity(40);light.set_attenuation_radius(500)
    c=capture.capture_component2d;tex=unreal.RenderingLibrary.create_render_target2d(world,1280,800,unreal.TextureRenderTargetFormat.RTF_RGBA8)
    c.set_editor_property('texture_target',tex);c.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    c.set_editor_property('fov_angle',52);c.set_editor_property('capture_every_frame',False)
    c.set_editor_property('always_persist_rendering_state',True);c.set_editor_property('hidden_actors',[pawn])
    p=c.get_editor_property('post_process_settings')
    p.set_editor_property('override_auto_exposure_method',True);p.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
    p.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True);p.set_editor_property('auto_exposure_apply_physical_camera_exposure',False)
    c.set_editor_property('post_process_settings',p);c.set_editor_property('post_process_blend_weight',1)
    result=dict(pending=True,targets=[]);unreal._moon_blocking_play_result=result
    class Pending:
        def __init__(self):
            self.index=0;self.frames=0;self.phase='probe';self.place(nests[0]);self.handle=unreal.register_slate_post_tick_callback(self.tick)
        def place(self,a):
            mesh=a.get_editor_property('refuge_mesh');up=mesh.get_up_vector();forward=mesh.get_forward_vector()
            target=a.get_interaction_anchor_world_location();pos=mesh.get_world_location()-forward*165+up*100
            rot=unreal.MathLibrary.make_rot_from_xz(target-pos,up)
            pawn.set_actor_location(pos,False,True);camera.set_actor_location(pos,False,True);camera.set_actor_rotation(rot,True)
            capture.set_actor_location(pos,False,True);capture.set_actor_rotation(rot,True);lamp.set_actor_location(pos,False,True)
            self.frames=0
        def finish(self):
            component.cancel_interaction();pc.set_view_target_with_blend(previous,0);pawn.set_actor_transform(saved,False,True)
            movement.set_movement_mode(mode)
            for a in (camera,capture,lamp):a.destroy_actor()
            unreal.unregister_slate_post_tick_callback(self.handle);result['pending']=False
        def image(self,name):
            c.capture_scene();unreal.RenderingLibrary.export_render_target(world,tex,str(s.QA),name)
            result[name]=str(s.QA/name)
        def tick(self,dt):
            try:
                self.frames+=1;c.capture_scene();now=unreal.GameplayStatics.get_time_seconds(world)
                if self.phase=='probe':
                    if self.frames<4:return
                    a=nests[self.index];component.refresh_interactable()
                    row=dict(nest=a.get_actor_label(),in_view=component.is_interactable_in_view(a),selected=component.get_current_interactable()==a)
                    result['targets'].append(row);assert row['in_view'] and row['selected'],row
                    self.index+=1
                    if self.index<len(nests):self.place(nests[self.index]);return
                    self.place(nests[0]);self.phase='start';return
                if self.phase=='start':
                    if self.frames<18:return
                    self.image('nest_before_blocking.png');assert component.try_interact()
                    self.started=now;self.phase='partial';return
                if self.phase=='partial':
                    if now-self.started<.65:return
                    result['partial_progress']=component.get_interaction_hold_progress()
                    assert 0<result['partial_progress']<1
                    component.cancel_interaction();self.started=now;self.phase='cancelled';return
                if self.phase=='cancelled':
                    if now-self.started<2.8:return
                    result['cancelled_stays_open']=not nests[0].is_entrance_blocked();assert result['cancelled_stays_open']
                    assert component.try_interact();self.started=now;self.phase='complete';return
                if self.phase=='complete':
                    if now-self.started<2.8:return
                    a=nests[0];result['blocked']=a.is_entrance_blocked();result['plug_visible']=a.get_editor_property('entrance_blocker').is_visible()
                    assert result['blocked'] and result['plug_visible'] and not a.call_method('CanInteract',(pawn,))
                    self.phase='capture';self.frames=0;return
                if self.phase=='capture' and self.frames>=18:
                    self.image('nest_after_blocking.png');self.finish()
            except Exception:
                import traceback
                result['error']=traceback.format_exc();self.finish()
    unreal._moon_blocking_play=Pending()
    return result


def author_entrances(rebuild=False):
    s.editor_world()
    # All geometry stays above the unchanged ground; the inner tube slopes down into a sealed dark end.
    palette=[('TunnelRegolith',(.39,.40,.42)),('TunnelLip',(.22,.23,.25)),
             ('TunnelInner',(.12,.13,.15)),('TunnelDeep',(.018,.022,.028))]
    materials=[]
    for name,color in palette:
        mat=s.material(name,color)
        node=unreal.MaterialEditingLibrary.get_material_property_input_node(mat,unreal.MaterialProperty.MP_BASE_COLOR)
        node.set_editor_property('constant',unreal.LinearColor(*color,1))
        unreal.MaterialEditingLibrary.recompile_material(mat);s.save(mat);materials.append(mat)
    result = []
    for variant in range(4):
        path = s.MESHES + '/SM_MS3_SafeSlopingRefuge_' + str(variant)
        existing = unreal.load_asset(path)
        if existing and not rebuild:
            result.append(existing)
            continue
        dm = unreal.DynamicMesh()
        steps = 20
        phase = variant * 1.71
        def ring(cx, cz, ry, rz, rim=False):
            points=[]
            for i in range(steps):
                a=2*math.pi*i/steps
                w=1+.055*math.sin(3*a+phase)+.025*math.cos(7*a-phase)
                points.append((cx+.45*rz*math.sin(a)*w, ry*math.cos(a)*w, cz+rz*math.sin(a)*w))
            return points
        rings=[ring(-20,26,23,15),ring(0,17,18,12),ring(22,8,9,5),ring(34,5,4,2)]
        outer=[(8+57*math.sin(2*math.pi*i/steps), 42*math.cos(2*math.pi*i/steps), .2) for i in range(steps)]
        lip=ring(-20,26,27,19)
        def strip(a,b,material,inward=False):
            vertices=[];triangles=[];uv=[];normals=[]
            for i in range(steps):
                j=(i+1)%steps
                q=[a[i],a[j],b[j],b[i]]
                if inward:q.reverse()
                base=len(vertices);vertices.extend(unreal.Vector(*v) for v in q)
                normal=unreal.MathLibrary.cross_vector_vector(vertices[base+1]-vertices[base],vertices[base+2]-vertices[base]).normal()
                normals.extend([normal]*4)
                uv.extend([unreal.Vector2D(0,0),unreal.Vector2D(1,0),unreal.Vector2D(1,1),unreal.Vector2D(0,1)])
                triangles.extend([unreal.IntVector(base,base+1,base+2),unreal.IntVector(base,base+2,base+3)])
            buffers=unreal.GeometryScriptSimpleMeshBuffers(vertices=vertices,triangles=triangles,uv0=uv,normals=normals)
            dm.append_buffers_to_mesh(buffers,material)
        strip(outer,lip,0,True)
        strip(lip,rings[0],1,True)
        for i in range(3):strip(rings[i],rings[i+1],2 if i==0 else 3,True)
        end=[unreal.Vector(*p) for p in rings[-1]]+[unreal.Vector(34,0,5)]
        dm.append_buffers_to_mesh(unreal.GeometryScriptSimpleMeshBuffers(vertices=end,triangles=[unreal.IntVector((i+1)%steps,i,steps) for i in range(steps)],uv0=[unreal.Vector2D(p.y/10,p.z/10) for p in end],normals=[unreal.Vector(-1,0,0)]*len(end)),3)
        options=unreal.GeometryScriptCreateNewStaticMeshAssetOptions(enable_collision=False,enable_recompute_normals=True,enable_recompute_tangents=True)
        if existing:
            copy=unreal.GeometryScriptCopyMeshToAssetOptions(enable_recompute_normals=True,enable_recompute_tangents=True,replace_materials=True,new_materials=materials)
            _,outcome=unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dm,existing,copy,unreal.GeometryScriptMeshWriteLOD())
            asset=existing
        else:asset,outcome=unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm,path,options)
        assert asset,(path,outcome)
        asset.set_editor_property('allow_cpu_access',True)
        asset.set_editor_property('static_materials',[unreal.StaticMaterial(material_interface=m,material_slot_name=m.get_name()) for m in materials])
        s.save(asset);result.append(asset)
    return result


def inspect():
    state = s.actors()
    result = {'lights': [], 'entrances': [], 'postprocess': []}
    for a in state.values():
        if isinstance(a, unreal.Light):
            c = a.get_component_by_class(unreal.LightComponent)
            props = {}
            for p in ('intensity', 'light_color', 'indirect_lighting_intensity', 'source_angle', 'cast_shadows', 'specular_scale'):
                try: props[p] = str(c.get_editor_property(p))
                except Exception: pass
            result['lights'].append(dict(label=a.get_actor_label(), cls=a.get_class().get_name(), props=props))
        if isinstance(a, unreal.SkyLight):
            c = a.get_component_by_class(unreal.SkyLightComponent)
            result['lights'].append(dict(label=a.get_actor_label(), props={p: str(c.get_editor_property(p)) for p in ('intensity', 'source_type', 'cubemap', 'lower_hemisphere_is_black')}))
        if isinstance(a, unreal.PostProcessVolume):
            p = a.get_editor_property('settings')
            result['postprocess'].append(dict(label=a.get_actor_label(), unbound=a.get_editor_property('unbound'), props={k: str(p.get_editor_property(k)) for k in ('auto_exposure_method', 'auto_exposure_bias', 'auto_exposure_min_brightness', 'auto_exposure_max_brightness', 'ambient_occlusion_intensity', 'dynamic_global_illumination_method', 'reflection_method')}))
        if a.get_actor_label().startswith('Moon_Nest_'):
            c = a.get_editor_property('refuge_mesh')
            result['entrances'].append(dict(label=a.get_actor_label(), mesh=c.static_mesh.get_name(), size=s.xyz(c.get_local_bounds()[1]-c.get_local_bounds()[0]), scale=s.xyz(c.get_editor_property('relative_scale3d')), cut=str(a.get_editor_property('refuge_cut_volume'))))
    moon = state['MoonPlanet'].static_mesh_component
    result['moon'] = dict(visible=moon.is_visible(), collision=str(moon.get_collision_enabled()), materials=[str(x) for x in moon.get_materials()])
    return result


def configure():
    s.editor_world();r.check_protected()
    state=s.actors()
    planet=next(a for a in state.values() if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='Moon')
    planet.modify();planet.set_editor_property('maximum_placement_slope_degrees',50)
    # Removing all Moon cutter references preserves both source mesh and original runtime collision.
    planet.get_editor_property('surface_cuts').set_editor_property('cut_source_mesh',None)
    mouths=author_entrances()
    residents={}
    for kind,enemy in [('Mirror','MirrorSkimmer'),('Rift','Riftlip')]:
        path=s.BPS+'/BP_MoonResourceForagers_'+kind
        bp=unreal.load_asset(path) or s.EA.duplicate_asset(s.BPS+'/BP_MoonHabitat',path)
        defaults=unreal.get_default_object(bp.generated_class())
        for k,v in dict(enemy_class=unreal.load_asset(s.BPS+'/BP_'+enemy).generated_class(),enemy_count=2,
                spawn_radius=75,core_radius=75,minimum_spacing=60,stationary_feeding=True,require_refuge=True,
                maintain_population=False,destructible_entrance=False,expedition_chance=1,fringe_only=False,
                light_preference=unreal.JTSHabitatLight.ANY,refuge_cut_volume=None).items():defaults.set_editor_property(k,v)
        defaults.get_editor_property('refuge_mesh').set_static_mesh(None)
        defaults.get_editor_property('surface_scatter').set_editor_property('entries',[])
        unreal.BlueprintEditorLibrary.compile_blueprint(bp);s.save(bp)
        residents[kind]=bp.generated_class()
    changed=0
    def entries(sc,kind):
        nonlocal changed
        values=list(sc.get_editor_property('entries'))
        for e in values:
            e.set_editor_property('cut_volumes',[])
            e.set_editor_property('destructible',False)
            if e.get_editor_property('refuge') and any('Mirror' in m.get_name() for m in e.get_editor_property('meshes')):
                e.set_editor_property('meshes',mouths)
                e.set_editor_property('scale_range',unreal.Vector2D(.8,1.05))
                e.set_editor_property('conform_to_surface',False)
            if e.get_editor_property('resource_item'):
                e.set_editor_property('near_refuge',True)
                e.set_editor_property('at_refuge_edge',True)
                e.set_editor_property('refuge_distance',unreal.Vector2D(18,65))
                e.set_editor_property('minimum_spacing',65)
                e.set_editor_property('food',False)
                e.set_editor_property('resident_habitat_class',residents[kind])
                e.set_editor_property('resident_count_range',unreal.IntPoint(0,2));changed+=1
        sc.set_editor_property('entries',values)
    for a in state.values():
        if isinstance(a,unreal.JTSPlanetEcologyHabitat):
            a.modify();a.set_editor_property('refuge_cut_volume',None)
            if a.get_actor_label().startswith('Moon_Nest_'):
                c=a.get_editor_property('refuge_mesh');c.set_static_mesh(mouths[(int(a.get_actor_label()[-2:])-1)%4])
                c.set_editor_property('relative_scale3d',unreal.Vector(1,1,1))
                hit=planet.project_point_to_surface(a.get_actor_location());assert hit
                # Keep the authored marker position: a second millimetre-scale projection near a
                # triangle edge can switch the collision face used for the placement slope test.
                c.set_world_location(a.get_actor_location(),False,True)
                c.set_world_rotation(unreal.MathLibrary.make_rot_from_xz(a.get_actor_forward_vector(),hit.impact_normal),False,True)
            if a.get_actor_label().endswith('_Workers'):
                a.set_editor_property('core_radius',700);a.set_editor_property('spawn_radius',900)
            entries(a.get_editor_property('surface_scatter'),'Mirror' if 'Mirror' in a.get_actor_label() else 'Rift')
    bp=unreal.load_asset(s.BPS+'/BP_MoonMirrorFeedingArea')
    entries(unreal.get_default_object(bp.generated_class()).get_editor_property('surface_scatter'),'Mirror')
    unreal.BlueprintEditorLibrary.compile_blueprint(bp);s.save(bp)
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    r.check_protected()
    blocking=configure_blocking()
    return dict(slope_limit=50,resource_entries=changed,residents='0-2 per deposit',cut_source=None,fixed_nests=9,blocking=blocking,protected=True)


def materials():
    lib=unreal.MaterialEditingLibrary
    roles={'Metal':(.22,.82,.5),'Crystal':(.19,.04,.65),'CarbonNodule':(.44,0,.5),'CarbonCore':(.63,0,.5)}
    for role,(roughness,metallic,specular) in roles.items():
        mat=unreal.load_asset(r.MATERIALS+'/M_MR_'+role);assert mat
        for prop,value,y in [(unreal.MaterialProperty.MP_ROUGHNESS,roughness,300),(unreal.MaterialProperty.MP_METALLIC,metallic,450),(unreal.MaterialProperty.MP_SPECULAR,specular,600)]:
            node=lib.create_material_expression(mat,unreal.MaterialExpressionConstant,200,y);node.set_editor_property('r',value)
            assert lib.connect_material_property(node,'',prop)
        if role!='Metal':
            custom=lib.create_material_expression(mat,unreal.MaterialExpressionCustom,500,-500)
            custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
            inputs=[]
            for name,cls in [('P',unreal.MaterialExpressionWorldPosition),('N',unreal.MaterialExpressionPixelNormalWS),('V',unreal.MaterialExpressionCameraVectorWS),('T',unreal.MaterialExpressionTime)]:
                value=unreal.CustomInput();value.set_editor_property('input_name',name);inputs.append(value)
            custom.set_editor_property('inputs',inputs)
            strength=.3 if role=='CarbonCore' else .65
            custom.set_editor_property('code',f'''float seed=frac(sin(dot(floor(P*0.075),float3(12.9898,78.233,37.719)))*43758.5453);
float sparse=smoothstep(0.96,0.995,seed);
float pulse=pow(saturate(sin(T*0.85+seed*6.28318)),14.0);
float edge=pow(1.0-saturate(dot(normalize(N),normalize(V))),2.0);
return float3(0.68,0.84,0.72)*{strength}*sparse*pulse*(0.25+0.75*edge);''')
            for i,(name,cls) in enumerate([('P',unreal.MaterialExpressionWorldPosition),('N',unreal.MaterialExpressionPixelNormalWS),('V',unreal.MaterialExpressionCameraVectorWS),('T',unreal.MaterialExpressionTime)]):
                node=lib.create_material_expression(mat,cls,100,-850+i*100)
                assert lib.connect_material_expressions(node,'',custom,name)
            assert lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        lib.recompile_material(mat);s.save(mat)
    return dict(reflective_roles=list(roles),carbon_shimmer='sparse, slow, view dependent')


def lighting():
    s.editor_world();state=s.actors()
    sun=state['Sun_Main'];sun.modify();c=sun.get_component_by_class(unreal.DirectionalLightComponent)
    c.set_editor_property('indirect_lighting_intensity',1)
    c.set_editor_property('specular_scale',1)
    c.set_editor_property('light_source_angle',.5357)
    # Keep the project's calibrated intensity/exposure pair and the single solar direction.
    # Local exposure compresses excessive display contrast while GI supplies real bounced sunlight.
    volume=state['SpaceExposure'];volume.modify();p=volume.get_editor_property('settings')
    for key,value in dict(local_exposure_shadow_contrast_scale=.72,local_exposure_highlight_contrast_scale=.85,
            local_exposure_detail_strength=.85,ambient_occlusion_intensity=.25,lumen_scene_lighting_quality=2,
            lumen_final_gather_quality=2).items():
        p.set_editor_property('override_'+key,True);p.set_editor_property(key,value)
    volume.set_editor_property('settings',p)
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return dict(single_sun=True,solar_angle_degrees=.5357,indirect=1,specular=1,skylight=0,local_shadow_contrast=.72)


def runtime():
    world=game_world()
    actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
    state={a.get_actor_label():a for a in actors}
    planet=next(a for a in actors if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='Moon')
    carbon=[];invalid=[];mouths=[];counts={};feeding=[];slopes=[]
    for a in actors:
        selected=isinstance(a,(unreal.JTSPlanetSurfaceAttachment,unreal.JTSMoonResourceActor,unreal.JTSPlanetSurfaceCreature,unreal.JTSPlanetColonyNest,unreal.JTSWorldPickupActor,unreal.JTSMoonCorpseActor)) and (a.get_actor_location()-planet.get_planet_center()).length()<45000
        if not selected or a.get_editor_property('hidden'):continue
        hit=planet.project_point_to_surface(a.get_actor_location())
        if not hit:invalid.append(a.get_name()+':no surface');continue
        dot=unreal.MathLibrary.dot_vector_vector(hit.impact_normal,planet.get_radial_up_vector(hit.impact_point))
        slope=math.degrees(math.acos(max(-1,min(1,dot))));slopes.append(slope)
        if slope>50.01:invalid.append(dict(actor=a.get_name(),slope=slope))
        if isinstance(a,unreal.JTSPlanetSurfaceAttachment):
            mesh=a.get_component_by_class(unreal.StaticMeshComponent).static_mesh
            if mesh and mesh.get_name().startswith('SM_MR_Carbon_'):carbon.append(a)
            if a.get_attach_parent_actor() and a.get_attach_parent_actor().get_actor_label().startswith('Moon_Nest_'):mouths.append(a)
        if isinstance(a,unreal.JTSPlanetSurfaceCreature):
            target=a.get_editor_property('feeding_target')
            if target:
                key=target.get_name();counts[key]=counts.get(key,0)+1
                feeding.append(dict(actor=a.get_name(),target=key,state=str(a.get_editor_property('state')),distance=round(planet.approximate_surface_arc_distance(a.get_actor_location(),target.get_actor_location()))))
    cuts=planet.get_editor_property('surface_cuts')
    summary=dict(carbon=len(carbon),resource_spectators=len(feeding),assigned_per_deposit=[counts.get(a.get_name(),0) for a in carbon],max_slope=max(slopes or [0]),slope_violations=invalid,fixed_mouths=len(mouths),runtime_cuts=cuts.get_cut_count(),moon_collision=str(state['MoonPlanet'].static_mesh_component.get_collision_enabled()),protected=r.check_protected(state),feeding=feeding)
    (s.ROOT/'Saved/MoonResources/scene_polish_validation.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    assert not invalid,invalid
    assert cuts.get_cut_count()==0
    assert carbon and all(counts.get(a.get_name(),0)<=2 for a in carbon)
    summary['per_deposit_histogram']={str(i):summary['assigned_per_deposit'].count(i) for i in range(3)}
    del summary['assigned_per_deposit']
    summary['feeding']=feeding[:3]
    summary['fixed_slopes']={a.get_actor_label():math.degrees(math.acos(max(-1,min(1,unreal.MathLibrary.dot_vector_vector(planet.project_point_to_surface(a.get_editor_property('refuge_mesh').get_world_location()).impact_normal,planet.get_radial_up_vector(a.get_editor_property('refuge_mesh').get_world_location())))))) for a in actors if a.get_actor_label().startswith('Moon_Nest_')}
    return summary


def capture(lamp=False):
    world=game_world()
    actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
    sun=next(a for a in actors if a.get_actor_label()=='Sun_Main')
    choices=[a for a in actors if isinstance(a,unreal.JTSPlanetSurfaceAttachment)
        and a.get_component_by_class(unreal.StaticMeshComponent).static_mesh.get_name().startswith('SM_MS3_SafeSlopingRefuge_')
        and not a.get_attach_parent_actor()]
    a=max(choices,key=lambda a:unreal.MathLibrary.dot_vector_vector(a.get_actor_up_vector(),-sun.get_actor_forward_vector()))
    planet=a.get_owner();up=planet.project_point_to_surface(a.get_actor_location()).impact_normal
    f=a.get_actor_forward_vector();forward=(f-up*unreal.MathLibrary.dot_vector_vector(f,up)).normal()
    target=a.get_actor_location()+up*23;location=target-forward*170+up*95
    rot=unreal.MathLibrary.make_rot_from_xz(target-location,up)
    def temporary(cls,pos,rot):
        pose=unreal.Transform(location=pos,rotation=rot,scale=unreal.Vector(1,1,1))
        lib=unreal.get_default_object(unreal.GameplayStatics.static_class())
        value=lib.call_method('BeginDeferredActorSpawnFromClass',(world,cls.static_class(),pose,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        return lib.call_method('FinishSpawningActor',(value,pose,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    camera=temporary(unreal.SceneCapture2D,location,rot);light=None
    if lamp:
        light=temporary(unreal.PointLight,location,unreal.Rotator())
        c=light.point_light_component;c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_editor_property('intensity_units',unreal.LightUnits.CANDELAS);c.set_intensity(40);c.set_attenuation_radius(500)
    c=camera.capture_component2d
    tex=unreal.RenderingLibrary.create_render_target2d(world,1280,800,unreal.TextureRenderTargetFormat.RTF_RGBA8)
    c.set_editor_property('texture_target',tex);c.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    c.set_editor_property('fov_angle',52);c.set_editor_property('always_persist_rendering_state',True)
    c.set_editor_property('hidden_actors',[unreal.GameplayStatics.get_player_pawn(world,0)]+[value for value in actors if isinstance(value,unreal.JTSPlanetSurfaceCreature)])
    c.set_editor_property('capture_every_frame',False)
    p=c.get_editor_property('post_process_settings')
    p.set_editor_property('override_auto_exposure_method',True);p.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
    p.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True);p.set_editor_property('auto_exposure_apply_physical_camera_exposure',False)
    c.set_editor_property('post_process_settings',p);c.set_editor_property('post_process_blend_weight',1)
    name='safe_tunnel_lamp.png' if lamp else 'safe_tunnel_sun.png'
    class Pending:
        def __init__(self):self.frames=0;self.handle=unreal.register_slate_post_tick_callback(self.tick)
        def tick(self,dt):
            self.frames+=1;c.capture_scene()
            if self.frames<18:return
            try:unreal.RenderingLibrary.export_render_target(world,tex,str(s.QA),name)
            finally:
                camera.destroy_actor()
                if light:light.destroy_actor()
                unreal.unregister_slate_post_tick_callback(self.handle)
    unreal._moon_polish_capture=Pending()
    return dict(file=str(s.QA/name),attachment=a.get_name())


def collect_return(prepare):
    world=game_world()
    if prepare:
        animals=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceCreature)
        eligible=[a for a in animals if a.get_editor_property('feeding_target') and a.get_editor_property('refuge_actor')]
        assert eligible,'No resource observers with real refuges'
        creature=max(eligible,key=lambda a:(a.get_actor_location()-a.get_editor_property('refuge_actor').get_actor_location()).length())
        target=creature.get_editor_property('feeding_target')
        cohort=[a for a in eligible if a.get_editor_property('feeding_target')==target]
        assert 1<=len(cohort)<=2
        planet=target.get_owner();pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        inventory=pawn.get_component_by_class(unreal.JTSInventoryComponent)
        if not inventory.can_add_item(unreal.JTSItemId.INORGANIC_CARBON,1):assert inventory.drop_item_at_slot(0)
        previous=inventory.get_item_count(unreal.JTSItemId.INORGANIC_CARBON)
        initial=[(a.get_actor_location()-a.get_editor_property('refuge_actor').get_actor_location()).length() for a in cohort]
        up=planet.get_radial_up_vector(target.get_actor_location())
        saved=pawn.get_actor_transform()
        pawn.set_actor_location(target.get_actor_location()+up*95,False,True)
        unreal._moon_polish_return_probe=(cohort,initial)
        unreal._moon_polish_pickup_result=dict(pending=True)
        class PendingPickup:
            def __init__(self):self.frames=0;self.handle=unreal.register_slate_post_tick_callback(self.tick)
            def tick(self,dt):
                self.frames+=1
                if self.frames<3:return
                try:
                    target.call_method('Interact',(pawn,))
                    unreal._moon_polish_pickup_result=dict(pending=False,picked_up=inventory.get_item_count(unreal.JTSItemId.INORGANIC_CARBON)==previous+1)
                finally:
                    pawn.set_actor_transform(saved,False,True)
                    unreal.unregister_slate_post_tick_callback(self.handle)
        unreal._moon_polish_pending_pickup=PendingPickup()
        return dict(pending=True,observers=len(cohort),initial_distances_cm=initial)
    cohort,initial=unreal._moon_polish_return_probe
    rows=[dict(name=a.get_name(),initial_cm=initial[i],current_cm=(a.get_actor_location()-a.get_editor_property('refuge_actor').get_actor_location()).length(),state=str(a.get_editor_property('state')),visible=a.get_editor_property('mesh').is_visible()) for i,a in enumerate(cohort)]
    return dict(pickup=unreal._moon_polish_pickup_result,observers=rows,all_home=all(not a.get_editor_property('mesh').is_visible() for a in cohort))


def run(stage):
    if stage == 'blocking_play':return json.dumps(blocking_play())
    if stage == 'blocking_play_check':return json.dumps(unreal._moon_blocking_play_result)
    if stage == 'blocking_probe':return json.dumps(accessible_nests())
    if stage == 'blocking_configure':return json.dumps(configure_blocking())
    if stage == 'blocking_audit':return json.dumps(audit_blocking())
    if stage in ('network_prepare','network_restore'):
        s.editor_world()
        settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
        values=dict(PlayNumberOfClients=2,RunUnderOneProcess=True)
        if stage=='network_prepare':
            unreal._moon_polish_play_settings={k:settings.get_editor_property(k) for k in values}
        else:values=unreal._moon_polish_play_settings
        for k,v in values.items():settings.set_editor_property(k,v)
        return json.dumps({k:str(v) for k,v in values.items()})
    if stage == 'network_check':
        rows=[]
        for world in unreal.EditorLevelLibrary.get_pie_worlds(False):
            actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat)
            nests=[a for a in actors if a.get_actor_label().startswith('Moon_Nest_')]
            rows.append(dict(world=world.get_name(),server=bool(unreal.GameplayStatics.get_game_mode(world)),fixed_nests=len(nests),
                visible_source_meshes=sum(a.get_editor_property('refuge_mesh').is_visible() for a in nests)))
        assert len(rows)==2 and all(x['fixed_nests']==9 and x['visible_source_meshes']==0 for x in rows),rows
        return json.dumps(rows)
    if stage in ('return_prepare','return_check'):return json.dumps(collect_return(stage=='return_prepare'))
    if stage == 'entrances_rebuild':return json.dumps([a.get_name() for a in author_entrances(True)])
    if stage in ('capture_sun','capture_lamp'):return json.dumps(capture(stage=='capture_lamp'))
    if stage == 'runtime':return json.dumps(runtime())
    if stage == 'configure':return json.dumps(configure())
    if stage == 'materials':
        try:return json.dumps(materials())
        except Exception:
            import traceback
            return traceback.format_exc()
    if stage == 'lighting':return json.dumps(lighting())
    if stage == 'entrances':return json.dumps([a.get_path_name() for a in author_entrances()])
    if stage == 'inspect': return json.dumps(inspect())
    raise ValueError(stage)



