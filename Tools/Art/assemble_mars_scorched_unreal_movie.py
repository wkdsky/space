"""Encode the fixed-time MRQ sequence, without resampling viewport captures."""
import hashlib
import json
import shutil
import subprocess
from pathlib import Path
from PIL import Image, ImageStat

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / 'SourceArt/MarsDestruction/ScorchedFractureV3'
manifest = json.loads((ART / 'manifest.json').read_text())
FRAMES = ROOT / manifest.get('cinematic_frames_directory', 'Saved/MarsDestruction/offline_r8')
report = json.loads((FRAMES / 'render_report.json').read_text())
assert report['revision'] == manifest['revision'] and report['result'] == 'PASS', report
fps=manifest['video_fps']
assert report['output_frame_step'] == 1 and report['fps'] == fps == 60
assert report['first_frame'] == 0 and report['frames'] == round(manifest['duration_seconds']*fps)
assert report['resolution'] == [1920, 1080]
assert len(list(FRAMES.glob('Mars_*.png'))) == report['frames']

# Check rendered cadence both before release and throughout the reported late
# stutter. The separate pose audit detects holds hidden by material/TAA changes.
cadence=[]
for start,end in ((4,9),(20,21),(25,26),(31,32)):
    hashes = [hashlib.sha256((FRAMES / f'Mars_{i:04d}.png').read_bytes()).hexdigest()
              for i in range(start*fps,end*fps)]
    assert len(set(hashes)) == len(hashes), (start,end,'Duplicated rendered frames')
    cadence.append(dict(seconds=[start,end],frames=len(hashes),unique_frames=len(set(hashes))))
sampling=json.loads((ART/'validation_animation_sampling.json').read_text())
assert sampling['revision']==manifest['revision'] and sampling['result']=='PASS',sampling
for i in (round(33.1*fps),round(33.5*fps),round(33.9*fps)):
    with Image.open(FRAMES / f'Mars_{i:04d}.png') as picture:
        assert max(ImageStat.Stat(picture.convert('RGB')).mean) == 0, i

output = ART / 'Mars_Scorched_Fracture_V3_Unreal.mp4'
subprocess.run([shutil.which('ffmpeg'), '-y', '-hide_banner', '-loglevel', 'error',
                '-framerate', str(report['fps']), '-start_number', '0',
                '-i', str(FRAMES / 'Mars_%04d.png'), '-frames:v', str(report['frames']),
                '-vf', 'format=yuv420p', '-c:v', 'libx264', '-crf', '18',
                '-preset', 'medium', '-g', str(fps), '-movflags', '+faststart', str(output)], check=True)
probe = json.loads(subprocess.check_output([shutil.which('ffprobe'), '-v', 'error',
                  '-select_streams', 'v:0', '-show_entries',
                  'stream=width,height,r_frame_rate,nb_frames,duration',
                  '-of', 'json', str(output)], text=True))['streams'][0]
assert (probe['width'], probe['height'], probe['r_frame_rate'], probe['nb_frames']) == (
    1920, 1080, f'{fps}/1', str(report['frames'])), probe
assert abs(float(probe['duration']) - 40.) < .001, probe
black = subprocess.check_output([shutil.which('ffmpeg'), '-v', 'error', '-ss', '33.5',
                               '-i', str(output), '-frames:v', '1', '-f', 'rawvideo',
                               '-pix_fmt', 'rgb24', '-'])
assert len(black) == 1920 * 1080 * 3 and max(black) == 0

decoded_cadence=[]
for start in (20,25,31):
    decoded=subprocess.check_output([shutil.which('ffmpeg'),'-v','error','-ss',str(start),
        '-i',str(output),'-t','1','-an','-f','framemd5','-'],text=True)
    hashes=[line.rsplit(',',1)[-1].strip() for line in decoded.splitlines()
            if line.strip() and not line.startswith('#')]
    assert len(hashes)==fps and len(set(hashes))==fps,(start,len(hashes),len(set(hashes)))
    decoded_cadence.append(dict(seconds=[start,start+1],frames=len(hashes),unique_frames=len(set(hashes))))

for name,start,duration in [('Mars_Leakage_4to12_Review.mp4',4,8),('Mars_LateMotion_20to32_Review.mp4',20,12)]:
    subprocess.run([shutil.which('ffmpeg'),'-y','-hide_banner','-loglevel','error',
        '-ss',str(start),'-i',str(output),'-t',str(duration),'-an','-c:v','libx264',
        '-crf','18','-preset','medium','-g',str(fps),'-pix_fmt','yuv420p',
        '-movflags','+faststart',str(ART/name)],check=True)

movies = ROOT / 'Content/Movies'
movies.mkdir(parents=True, exist_ok=True)
shutil.copy2(output, movies / 'MarsDestruction.mp4')
report.update(video=probe, frame_cadence=cadence, animation_sampling_report='validation_animation_sampling.json',
              decoded_video_cadence=decoded_cadence,
              source_blackout_rgb_zero=True, encoded_blackout_rgb_zero=True,
              game_movie='Content/Movies/MarsDestruction.mp4', audio=False)
(ART / 'unreal_movie_capture.json').write_text(json.dumps(report, indent=2) + '\n')
shutil.copy2(FRAMES / 'render_report.json', ART / 'validation_unreal_render.json')
print('UNREAL_MOVIE_OK', json.dumps(report))
