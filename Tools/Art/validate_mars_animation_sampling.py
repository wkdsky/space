"""Compare the actual cooked-pose evaluator with raw authoring at 60 Hz.

Run with UnrealEditor-Cmd -run=pythonscript. Image hashes alone cannot detect
frozen skeletal poses when TAA, lighting or a material keeps changing.
"""
import json
import math
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
ART=ROOT/'SourceArt/MarsDestruction/ScorchedFractureV3'
data=json.loads((ART/'manifest.json').read_text())
motion=json.loads((ART/'fragment_motion.json').read_text())
folder='/Game/Space/Environment/MarsDestructionPreview/ScorchedFractureV3'
animation=unreal.load_asset(folder+'/A_MarsScorchedFracture')
mesh=unreal.load_asset(folder+'/SK_MarsScorchedFracture')
options={}
for name in ('RAW','COMPRESSED'):
    option=unreal.AnimPoseEvaluationOptions()
    option.set_editor_property('evaluation_type',getattr(unreal.AnimDataEvalType,name))
    option.set_editor_property('optional_skeletal_mesh',mesh)
    options[name]=option

def sample(t,mode):
    pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(animation,t,options[mode])
    return [unreal.AnimPoseExtensions.get_bone_pose(pose,r['name'],unreal.AnimPoseSpaces.WORLD).translation.to_tuple()
            for r in motion]

assert not animation.get_editor_property('allow_frame_stripping')
assert animation.get_editor_property('interpolation')==unreal.AnimInterpolationType.LINEAR
first=sample(0.,'COMPRESSED')
for t in (5.,7.,7.5,8.):
    before=sample(t,'COMPRESSED')
    for i,r in enumerate(motion):
        if not r.get('fault_shard'):
            assert math.dist(first[i],before[i])<.0001,(t,r['name'],'early outward release')
after=sample(8.+1./60,'COMPRESSED')
for i,r in enumerate(motion):
    if not r.get('fault_shard'):
        assert math.dist(first[i],after[i])>.0001,(r['name'],'missed common release')

windows=[]
for start,end in ((20.,21.),(25.,26.),(31.,33.)):
    maximum_error=0.
    steps=[]
    ratios=[]
    previous=None
    for frame in range(round(start*60),round(end*60)+1):
        raw=sample(frame/60,'RAW')
        compressed=sample(frame/60,'COMPRESSED')
        maximum_error=max(maximum_error,max(math.dist(a,b) for a,b in zip(raw,compressed)))
        if previous:
            for i,r in enumerate(motion):
                d=math.dist(previous[1][i],compressed[i])
                reference=math.dist(previous[0][i],raw[i])
                assert d>1e-6,(frame,r['name'],'compressed-pose hold')
                assert reference>1e-6
                steps.append(d)
                ratios.append(d/reference)
        previous=(raw,compressed)
    assert maximum_error<.001,maximum_error
    assert min(ratios)>.9 and max(ratios)<1.1,(min(ratios),max(ratios))
    windows.append(dict(seconds=[start,end],bone_steps=len(steps),zero_steps=0,
                        minimum_step_local_cm=min(steps),maximum_step_local_cm=max(steps),
                        maximum_raw_compressed_error_local_cm=maximum_error,
                        compressed_raw_step_ratio=[min(ratios),max(ratios)]))
report=dict(revision=data['revision'],result='PASS',sampling_fps=60,bones=len(motion),
            frame_stripping=False,compression=animation.get_editor_property('bone_compression_settings').get_path_name(),
            synchronized_release_seconds=8.,main_solids_stationary_until_release=True,
            windows=windows)
(ART/'validation_animation_sampling.json').write_text(json.dumps(report,indent=2)+'\n')
unreal.log('MARS_ANIMATION_SAMPLING '+json.dumps(report))
