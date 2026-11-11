"""Save one live frame of the RADR view from the bench camera service (camera 4 must already be live)."""
import json, subprocess, sys, time
from pathlib import Path
sys.path.insert(0, 'C:/rad-hil/harness')
import video
config = json.loads(Path('C:/rad-hil/cameras.json').read_text(encoding='utf-8-sig'))
preset = config['products']['radr']
out = Path('C:/rad-hil/manual-pr-evidence/lovense-emulator/session/snaps'); out.mkdir(parents=True, exist_ok=True)
target = out / (sys.argv[1] + '.jpg')
r = subprocess.run([config['ffmpeg'], '-hide_banner', '-loglevel', 'error', '-nostdin', '-f', 'mpjpeg',
                    '-i', f"{video.CAMERA_SERVICE}/stream/{preset['camera']}.mjpg", '-vf',
                    video.view_filter(preset['views']['r8'], clock=False), '-frames:v', '1', '-update', '1', '-q:v', '3',
                    '-y', str(target)], capture_output=True, text=True, timeout=30)
print(time.strftime('%H:%M:%S'), target if target.exists() else r.stderr[-300:])
