"""Author the separate Moon refuges exclusively through registered Unreal MCP tools."""
import argparse
import hashlib
import json
from pathlib import Path
from frontend_mcp_client import UnrealMCP

ROOT=Path(__file__).resolve().parents[2]
QA=ROOT/'Saved/MoonRefuges'
MESHES='/Game/Space/Environment/MoonV3/Meshes'
MATERIALS='/Game/Space/Environment/MoonV3/Materials'
BPS='/Game/Space/Blueprints/Planets/Moon/EcologyV3'
SCENE='editor_toolset.toolsets.scene.SceneTools'
ACTOR='editor_toolset.toolsets.actor.ActorTools'
OBJECT='editor_toolset.toolsets.object.ObjectTools'
MESH='editor_toolset.toolsets.static_mesh.StaticMeshTools'
ASSET='editor_toolset.toolsets.asset.AssetTools'


class Refuges:
    def __init__(self):self.client=UnrealMCP()

    def call(self,toolset,tool_name,**arguments):
        result=self.client.call(tool_name,arguments,toolset)
        if result.get('isError'):raise RuntimeError(result)
        value=json.loads(result['content'][0]['text']).get('returnValue')
        return value

    def find(self,name):return self.call(SCENE,'find_actors',name=name,tag='',collision_channels=[])

    def get(self,instance,*properties):
        return json.loads(self.call(OBJECT,'get_properties',instance=instance,properties=list(properties)))

    def set(self,instance,**values):
        assert self.call(OBJECT,'set_properties',instance=instance,values=json.dumps(values)),(instance,values)

    def label(self,actor):return self.call(ACTOR,'get_label',actor=actor)

    def snapshot(self):
        actors=self.find('Moon_Nest_')+self.find('Moon_Corpse_')+self.find('MoonPlanet')
        state={self.label(a):dict(actor=a,transform=self.call(ACTOR,'get_actor_transform',actor=a)) for a in actors}
        moon=self.find('MoonPlanet')[0]
        meshcomp=self.call(ACTOR,'get_components',actor=moon,component_type={'refPath':'/Script/Engine.StaticMeshComponent'})[0]
        mesh=self.get(meshcomp,'StaticMesh')['StaticMesh']
        file=ROOT/'Content'/(mesh['refPath'].split('.')[0].removeprefix('/Game/')+'.uasset')
        return dict(actors=state,moon_mesh=mesh,moon_mesh_sha256=hashlib.sha256(file.read_bytes()).hexdigest())

    def check_protected(self,before):
        after=self.snapshot()
        assert before==after,'Protected Moon, corpse or nine entrance geometry/transforms changed'
        return True

    def inspect(self):
        nests={self.label(a):self.get(a,'EnemyClass','EnemyCount','Refuges','LightPreference') for a in self.find('Moon_Nest_')}
        cores={}
        for a in self.find('_Rift_Core')[:1]:
            p=self.get(a,'EnemyClass','EnemyCount','SurfaceScatter','RefugeProviders')
            p['scatter']=self.get(p['SurfaceScatter'],'Entries');cores[self.label(a)]=p
        return dict(nests=nests,cores=cores,feeding=self.get({'refPath':BPS+'/BP_MoonMirrorFeedingArea.BP_MoonMirrorFeedingArea'},'EnemyClass','SurfaceScatter'))

    def import_models(self):
        return json.loads(self.call('Game.Tools.Art.moon_surface_mcp.MoonSurfaceV3Tools','import_surface',batch=3))

    def configure(self):
        baseline=self.snapshot()
        QA.mkdir(parents=True,exist_ok=True)
        (QA/'protected_before.json').write_text(json.dumps(baseline,indent=2),encoding='utf-8')
        models=self.import_models()
        mirror={'refPath':BPS+'/BP_MirrorSkimmer.BP_MirrorSkimmer_C'}
        for a in self.find('Moon_Nest_'):
            self.set(a,EnemyClass=mirror,EnemyCount=1,LightPreference='Any',Refuges=[a],RefugeProviders=[])
            label=self.label(a);mesh=self.get(a,'RefugeMesh')['RefugeMesh']
            self.set(mesh,StaticMesh=models['SM_MS3_FixedNest_'+label.rsplit('_',1)[-1]])
        cores=self.find('_Rift_Core')
        for i,a in enumerate(cores):
            p=self.get(a,'SurfaceScatter')
            scatter=self.get(p['SurfaceScatter'],'Entries')['Entries']
            for entry in scatter:
                if entry['bRefuge']:
                    entry['bConformToSurface']=True;entry['bCastShadow']=True;entry['bDestructible']=True
                    entry['meshes']=[models[f'SM_MS3_RiftRefuge_{j}'] for j in range(4)]
            self.set(p['SurfaceScatter'],Entries=scatter)
            self.set(a,EnemyCount=1+i%2,Refuges=[])
        # The core's one fissure owns its one or two residents. Sparse Mirror fringes
        # can still query same-species feeding residents through the provider graph.
        for a in self.find('_Rift_Fringe'):
            self.set(a,EnemyCount=0,ExpeditionChance=0,Refuges=[])
        for a in self.find('_Mirror_Fringe'):
            self.set(a,Refuges=[])
        bp={'refPath':BPS+'/BP_MoonMirrorFeedingArea.BP_MoonMirrorFeedingArea'}
        feeding=self.get(bp,'SurfaceScatter')['SurfaceScatter']
        entries=self.get(feeding,'Entries')['Entries']
        for entry in entries:
            if entry['bRefuge']:entry['bConformToSurface']=True;entry['bCastShadow']=True
        self.set(feeding,Entries=entries)
        for i in range(4):
            name=f'SM_MS3_MirrorRefuge_{i}'
            self.set({'refPath':MESHES+'/'+name+'.'+name},bAllowCPUAccess=True)
        self.call('editor_toolset.toolsets.blueprint.BlueprintTools','compile_blueprint',blueprint=bp,warnings_as_errors=False)
        self.call(ASSET,'save_assets',asset_paths=[BPS+'/BP_MoonMirrorFeedingArea','/Game/Space/Maps/L_SpaceWorld']+[MESHES+f'/SM_MS3_MirrorRefuge_{i}' for i in range(4)])
        assert self.check_protected(baseline)
        self.require_refuges()
        self.call('Game.Tools.Art.moon_surface_mcp.MoonSurfaceV3Tools','validate_play',stage='repair_configure')
        return dict(fixed_mirror_mouths=9,rift_fissure_regions=len(cores),rift_residents_per_fissure=[1,2],protected_unchanged=True)

    def require_refuges(self):
        targets=self.find('Moon_Nest_')+self.find('_Rift_Core')+self.find('_Rift_Fringe')+self.find('_Mirror_Fringe')
        for a in targets:self.set(a,bRequireRefuge=True)
        bp={'refPath':BPS+'/BP_MoonMirrorFeedingArea.BP_MoonMirrorFeedingArea'}
        self.set(bp,bRequireRefuge=True)
        self.call('editor_toolset.toolsets.blueprint.BlueprintTools','compile_blueprint',blueprint=bp,warnings_as_errors=False)
        self.call(ASSET,'save_assets',asset_paths=[BPS+'/BP_MoonMirrorFeedingArea','/Game/Space/Maps/L_SpaceWorld'])
        self.check_protected(json.loads((QA/'protected_before.json').read_text()))
        return dict(habitats_requiring_actual_refuge=len(targets),protected_unchanged=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('stage',choices=['inspect','configure','snapshot','require_refuges'])
    args=parser.parse_args();print(json.dumps(getattr(Refuges(),args.stage)(),indent=2))
