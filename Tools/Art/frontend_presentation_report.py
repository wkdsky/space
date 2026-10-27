"""Collect measured native presentation evidence and preserve the earlier flow suite."""
import hashlib
import json
import math
import re
import shutil
from datetime import datetime,timezone
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
ART=ROOT/'SourceArt/FrontEnd'
QA=ROOT/'Saved/FrontEndQA'
DELIVERY=ART/'QA'
read=lambda name:json.loads((QA/name).read_text(encoding='utf-8'))
vector=lambda text:tuple(float(re.search(axis+r':\s*([-\d.]+)',text)[1]) for axis in ['x','y','z'])
stage=lambda data:next(a for a in data['worlds'][0]['presentation'] if a['name']=='FE_MainPresentation')
checks=[]
def check(name,passed,evidence,**measured):
    assert passed,name
    checks.append(dict(check=name,passed=True,evidence=evidence,**measured))

phases=[stage(read('polished_pose_'+str(t)+'.json')) for t in [0,3,6,9,12]]
foot_error=max(math.dist(vector(a['bones'][bone]),vector(phases[0]['bones'][bone])) for a in phases for bone in ['Foot_L','Foot_R'])
body_span=math.dist(vector(phases[1]['bones']['Body']),vector(phases[3]['bones']['Body']))
check('Planted feet while native compressed idle shifts weight',foot_error<.01 and body_span>1.5,
      ['polished_pose_'+str(t)+'.json' for t in [0,3,6,9,12]],foot_deviation_cm=foot_error,body_span_cm=body_span)
check('Native loop endpoints match',all(math.dist(vector(phases[0]['bones'][b]),vector(phases[-1]['bones'][b]))<.01 for b in phases[0]['bones']),
      ['polished_pose_0.json','polished_pose_12.json'])
root_scale=re.search(r'scale3d: \{([^}]+)',phases[0]['bone_transforms']['Root'])[1]
check('Shared skeleton unit basis retained',all(abs(v-100)<.01 for v in vector(root_scale)),['polished_pose_0.json'])
check('Five suits retain playable idle and visible standing proportions',all(stage(read('polished_suit_'+str(i)+'.json'))['playing'] and
      vector(stage(read('polished_suit_'+str(i)+'.json'))['bones']['Head'])[2]>150 for i in range(5)),
      ['polished_suit_'+str(i)+'.json' for i in range(5)])

bounds=read('polished_scene_bounds_entry.json')['actors']
ship=next(a for a in bounds if a['name']=='FE_TwinforkDisplay')
walls=[a for a in bounds if any(s in a['name'] for s in ['Wall','Door','Frame'])]
ship_rear=ship['center'][1]+ship['extent'][1]
wall_gap=min(w['center'][1]-w['extent'][1]-ship_rear for w in walls)
floor_clearance=ship['center'][2]-ship['extent'][2]-.4
check('Ship grounded with no wall or door overlap',wall_gap>290 and abs(floor_clearance-1)<.02,
      ['polished_scene_bounds_entry.json'],minimum_wall_gap_cm=wall_gap,floor_clearance_cm=floor_clearance)
entry=stage(read('polished_entry_restart.json'))
pad=next(a for a in read('polished_entry_restart.json')['worlds'][0]['presentation'] if a['name']=='FE_CrewPad_1')
check('Hero root and visible pad aligned',vector(entry['mesh_transform'].split('translation:')[1])[0:2]==vector(pad['location'])[0:2],['polished_entry_restart.json'])
threat=next(a for a in bounds if a['name']=='FE_AlienThreat_Core')
check('Distant laser front remains suspended above buildings',threat['location'][2]>max(a['center'][2]+a['extent'][2] for a in walls),
      ['polished_scene_bounds_entry.json'],beam_origin_height_cm=threat['location'][2])

restart=read('polished_four_restart.json')['worlds']
check('Four appearances and native lobby idle survive editor restart (16 observed pawns)',len(restart)==4 and all(
    len(w['players'])==4 and len({re.search(r'suit: (\d+)',p['appearance'])[1] for p in w['players']})==4 and
    all(p['playing'] and 'AnimSingleNodeInstance' in p['anim_instance'] and abs(vector(p['bones']['Foot_L'])[2]-7.275)<.02 for p in w['players']) for w in restart),
    ['polished_four_restart.json'])
travel=read('polished_four_travel.json')['worlds']
check('Four-player travel restores gameplay pawns and animation',len(travel)==4 and all('L_EarthLaunchPrototype' in w['world'] and
    all('ABP_JTSPlayer_Casual_2' in p['anim_instance'] for p in w['players']) for w in travel),['polished_four_ready.json','polished_four_travel.json'])
shipping=read('polished_shipping_defaults.json')
check('Shipping version check enabled; entry idle with no dirty packages',shipping['require_matching_build_version'] and
    not shipping['dirty_maps'] and not shipping['dirty_content'] and 'L_Entry' in shipping['world'],['polished_shipping_defaults.json'])
restored=read('polished_save_restoration.json')
check('Original five saves restored byte-for-byte; QA profile removed',restored['success'] and all(
    hashlib.sha256((ROOT/'Saved/SaveGames'/a['file']).read_bytes()).hexdigest()==a['sha256'] for a in restored['files']) and
    not (ROOT/'Saved/SaveGames/JTSLocalProfile.sav').exists(),['polished_save_restoration.json'])
build='Build-PresentationFix-spaceEditor-Win64-Development.log'
log_bytes=(QA/build).read_bytes()
log=log_bytes.decode('utf-16' if log_bytes.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig')
check('Normal Development Editor build succeeded', 'Result: Succeeded' in log,[build],up_to_date='Target is up to date' in log)

for name in sorted({n for c in checks for n in c['evidence']}):shutil.copy2(QA/name,DELIVERY/name)
for name in ['PolishedPose3.png','PolishedPose9.png','PolishedHead.png']+['PolishedSuit'+str(i)+'.png' for i in range(5)]:
    shutil.copy2(QA/name,DELIVERY/name)
for item in checks:item['evidence']=['QA/'+p for p in item['evidence']]
report={'generated_utc':datetime.now(timezone.utc).isoformat(),'result':'passed','checks':checks,
    'source_assets':223,'cpp_changed_this_revision':False,'build_seconds':float(re.search(r'Total execution time: ([\d.]+) seconds',log)[1]),
    'source_manifest':'asset_manifest.json','sources':'sources.json','editor_steps':'PRESENTATION_FIX.md',
    'screenshots':['QA/MainMenu.png','QA/Astronaut.png','QA/FourPlayerLobby.png','QA/PolishedPose3.png','QA/PolishedPose9.png'],
    'visual_review':'Native home, five full-body outfits, face close-up, pose phases and four-player lobby inspected. Blue daytime sky, white-hot beam and red-orange heat cloud; no ground impact.',
    'scope':'Presentation assets, editor authoring scripts and Blueprint/level configuration. No gameplay animation or skeleton changes.',
    'multiplayer_environment':'One-process PIE listen server plus three clients using OnlineSubsystem NULL.',
    'not_verified':['Packaged build','Cross-machine networking and platform services'],
    'save_preservation':restored}
(ART/'presentation_fix_validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
baseline=json.loads((ART/'validation_report.json').read_text(encoding='utf-8'))
baseline['presentation_revision']={'report':'presentation_fix_validation.json','checks_passed':len(checks),'build_seconds':report['build_seconds']}
(ART/'validation_report.json').write_text(json.dumps(baseline,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'presentation_checks_passed':len(checks),'body_span_cm':body_span,'foot_deviation_cm':foot_error}))
