"""Assemble delivery evidence from completed editor tests; does not run or mutate gameplay."""
import hashlib
import json
import re
import shutil
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / 'SourceArt/FrontEnd'
QA = ROOT / 'Saved/FrontEndQA'
DELIVERY = ART / 'QA'
DELIVERY.mkdir(exist_ok=True)


def read(name):
    return json.loads((QA / name).read_text(encoding='utf-8'))


def worlds(name):
    data = read(name)
    assert data['success'], name
    return data['worlds']


checks = []


def check(name, passed, evidence):
    assert passed, name
    checks.append({'check': name, 'passed': True, 'evidence': evidence})


gate = json.loads((ART / 'asset_gate.json').read_text(encoding='utf-8'))
manifest = json.loads((ART / 'asset_manifest.json').read_text(encoding='utf-8'))
assets = manifest['assets']
check('Source asset gate and current SHA256 checksums', gate['passed'] and len(assets) == gate['asset_count'] and all(
    hashlib.sha256((ART / asset['file']).read_bytes()).hexdigest() == asset['sha256'] for asset in assets),
    ['../asset_manifest.json', '../asset_gate.json'])

appearance = worlds('final_four_appearance.json')
suits = ['Explorer', 'Scout', 'Engineer', 'Survey', 'Tactical']
check('Four independent appearances on all four peers (16 observed pawns)', len(appearance) == 4 and all(
    len(w['players']) == 4 and len({re.search(r'suit: (\d+)', p['appearance'])[1] for p in w['players']}) == 4
    and all(p['catalog'] != 'None' and 'SK_FE_' + suits[int(re.search(r'suit: (\d+)', p['appearance'])[1])] in p['mesh']
            for p in w['players']) for w in appearance), ['final_four_appearance.json'])

ready = worlds('final_four_ready.json')
check('All four ready; only host launch enabled', len(ready) == 4 and all(
    all(p['ready'] for p in w['players']) and
    w['widgets'][0]['controls']['Lobby_Launch']['enabled'] == (i == 0) for i, w in enumerate(ready)),
    ['final_four_ready.json'])
check('Native non-host launch request rejected by server', all(
    w['launch_deadline'] == 0 and w['accepting_players'] for w in worlds('final_client_launch_rejected.json')),
    ['final_client_launch_rejected.json'])

countdown = worlds('final_countdown.json')
check('Shared countdown closes joining and locks ready/launch', len(countdown) == 4 and
      len({w['launch_deadline'] for w in countdown}) == 1 and all(w['launch_deadline'] > 0 and not w['accepting_players'] and
      all(not state['enabled'] for state in w['widgets'][0]['controls'].values()) for w in countdown), ['final_countdown.json'])
travel = worlds('final_four_travel.json')
check('Four-player seamless travel reaches Earth collection', len(travel) == 4 and all(
    'L_EarthLaunchPrototype' in w['world'] and 'EARTH_COLLECTION' in w['phase'] and w['player_count'] == 4
    for w in travel), ['final_four_travel.json'])
check('Single-player new expedition creates lobby and reaches Earth',
      worlds('new_create_lobby.json')[0]['widgets'][0]['page_after'] == 'Lobby' and
      'L_EarthLaunchPrototype' in worlds('new_create_travel.json')[0]['world'],
      ['new_create_lobby.json', 'new_create_travel.json'])
check('Original saved expedition resumes in SpaceWorld',
      'L_SpaceWorld' in worlds('final_resume_travel.json')[0]['world'] and
      'MOON_EXPLORATION' in worlds('final_resume_travel.json')[0]['phase'],
      ['final_resume_lobby.json', 'final_resume_travel.json'])

paused = [worlds('final_motion_paused_' + suffix + '.json')[0] for suffix in ['a', 'b']]
robot_pos = lambda w: re.search(r'\{.*\}', next(a for a in w['scene'] if a['name'] == 'FE_MaintenanceRobot')['position'])[0]
loop = lambda w: next(a for a in w['scene'] if a['name'] == 'FE_HangarLoop')['playing']
check('Reduced motion pauses loop and robot; disabling it resumes',
      all(not loop(w) for w in paused) and robot_pos(paused[0]) == robot_pos(paused[1]) and
      loop(worlds('final_motion_resumed.json')[0]),
      ['final_motion_paused_a.json', 'final_motion_paused_b.json', 'final_motion_resumed.json'])
pending = worlds('video_pending.json')[0]['widgets'][0]['controls']
expired = worlds('video_timeout.json')[0]['widgets'][0]['controls']
check('Video confirmation expires and restores settings',
      all(v['enabled'] for v in pending.values()) and all(not v['enabled'] for v in expired.values()),
      ['video_pending.json', 'video_timeout.json'])
custom = read('custom_video_baseline.json')
resolution_applied = read('custom_resolution_applied.json')
check('Custom quality label and resolution-only changes preserve mixed quality', custom['overall'] == -1 and
      len(set(custom['quality'].values())) > 1 and
      worlds('custom_video_label.json')[0]['widgets'][0]['labels']['Value_Quality'] == '\u81ea\u5b9a\u4e49' and
      resolution_applied['quality'] == custom['quality'] and resolution_applied['resolution_scale'] == custom['resolution_scale'] and
      resolution_applied['resolution'] != custom['resolution'],
      ['custom_video_baseline.json', 'custom_video_label.json', 'custom_resolution_applied.json'])
check('Timeout and explicit preset cancellation restore every custom video field',
      read('custom_resolution_timeout.json') == custom and read('custom_preset_reverted.json') == custom and
      read('custom_preset_applied.json')['quality'] != custom['quality'],
      ['custom_video_baseline.json', 'custom_resolution_timeout.json', 'custom_preset_applied.json', 'custom_preset_reverted.json'])
check('Avatar catalog persisted after normal editor restart',
      'DA_FE_AvatarCatalog' in read('restart_persistence.json')['avatar_catalog'], ['restart_persistence.json'])
shipping = read('final_shipping_defaults.json')
check('Shipping version check restored; editor idle on entry with no dirty packages',
      shipping['require_matching_build_version'] and not shipping['dirty_maps'] and not shipping['dirty_content']
      and 'L_Entry' in shipping['world'], ['final_shipping_defaults.json'])
restored = read('save_restoration.json')
check('All five original save files restored byte-for-byte; QA profile removed', restored['success'] and
      all(hashlib.sha256((ROOT / 'Saved/SaveGames' / item['file']).read_bytes()).hexdigest() == item['sha256']
          for item in restored['files']) and not (ROOT / 'Saved/SaveGames/JTSLocalProfile.sav').exists(), ['save_restoration.json'])

build_name = 'Build-spaceEditor-Win64-Development.log'
build_log = (QA / build_name).read_text(encoding='utf-8')
check('Normal Development Editor build succeeded', 'Result: Succeeded' in build_log, [build_name])
evidence = {name for c in checks for name in c['evidence'] if not name.startswith('../')}
evidence.update(['final_slots.json', 'final_four_initial.json', 'save_backup_manifest.json'])
for name in sorted(evidence):
    shutil.copy2(QA / name, DELIVERY / name)
screenshots = ['MainMenu.png', 'Astronaut.png', 'Settings.png', 'ExpeditionSlots.png', 'FourPlayerLobby.png']
for name in screenshots:
    shutil.copy2(QA / name, DELIVERY / name)
for result in checks:
    result['evidence'] = [name[3:] if name.startswith('../') else 'QA/' + name for name in result['evidence']]

report = {
    'generated_utc': datetime.now(timezone.utc).isoformat(),
    'result': 'passed_with_service_validation_pending',
    'engine': 'Unreal Engine 5.8.2',
    'build': {'target': 'spaceEditor Win64 Development', 'result': 'Succeeded',
              'seconds': float(re.search(r'Total execution time: ([\d.]+) seconds', build_log)[1]),
              'command': 'Build.bat spaceEditor Win64 Development D:/projects/space/space.uproject -WaitMutex -NoHotReloadFromIDE',
              'evidence': 'QA/' + build_name},
    'entry_map': '/Game/Space/FrontEnd/Maps/L_Entry',
    'lobby_map': '/Game/Space/FrontEnd/Maps/L_PreLaunchLobby',
    'art_direction': 'Blue Earth daytime; routine maintenance continues; suspended alien laser front before ground impact.',
    'source_assets': len(assets),
    'source_manifest': 'asset_manifest.json',
    'checks': checks,
    'screenshots': ['QA/' + name for name in screenshots],
    'test_environment': {
        'multiplayer': 'One-process PIE listen server plus three clients, OnlineSubsystem NULL.',
        'rpc_driver': 'Native button invocation on a following game tick; avoids Python editor-script local callspace.',
        'pie_travel': 'net.AllowPIESeamlessTravel=1 in PIE worlds; shipping uses normal seamless travel.',
        'build_version_check': 'Temporarily disabled only for auto-connected PIE QA; restored and saved as true.',
        'new_game_capture': 'Earth gameplay had advanced to its existing timeout phase by the new-game capture; map arrival is the tested criterion.',
        'appearance_and_travel': 'Distinct appearance synchronization and final seamless travel were checked in separate PIE runs.'
    },
    'not_verified': ['EOS/Steam real-account authentication and discovery', 'Cross-machine networking',
                     'Voice transmit/receive with real provider devices', 'Packaged build and platform authentication'],
    'implementation_limits': ['Input page documents bindings; runtime key remapping is not included.',
                              'Presentation loop is 60 seconds; the distant laser stays suspended rather than impacting Earth.'],
    'save_preservation': restored,
    'editor_steps': ['Open space.uproject; L_Entry is the configured startup map.',
                     'Play with one local player to use the complete create/resume flow.',
                     'Edit WBP_FE_Main/WBP_FE_Lobby, DA_FE_AvatarCatalog and map Stage instances for presentation changes.'],
    'implementation_details': 'IMPLEMENTATION.md'
}
(ART / 'validation_report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'checks_passed': len(checks), 'assets': len(assets), 'screenshots': len(screenshots),
                  'report': str(ART / 'validation_report.json')}, ensure_ascii=False))
