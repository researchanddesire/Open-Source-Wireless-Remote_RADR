"""Record the rig camera that sees the devices under test.

Video is evidence only: a missing camera, busy device or encoder failure is
written to the manifest and never changes a hardware result. Camera identity
and per-device crop/rotation presets live in the host-only C:\\rad-hil\\cameras.json,
next to fixture.json; PR artifacts never choose a camera or a command.
"""
from __future__ import annotations

from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import threading
import time
import urllib.request

ROTATIONS = {0: '', 90: ',transpose=1', 180: ',hflip,vflip', 270: ',transpose=2'}
CAMERA_SERVICE = 'http://127.0.0.1:8765'
MAX_SECONDS = 1800
HEIGHT = 540
FONT = 'C\\:/Windows/Fonts/consola.ttf'


def utc():
    return datetime.now(timezone.utc).isoformat(timespec='milliseconds')


def view_filter(view, clock=True):
    x, y, w, h = (float(v) for v in view.get('crop', (0, 0, 1, 1)))
    if not (0 <= x < 1 and 0 <= y < 1 and 0 < w <= 1 - x + 1e-9 and 0 < h <= 1 - y + 1e-9):
        raise ValueError('crop must be a normalized [x, y, width, height] inside the frame')
    rotate = int(view.get('rotate', 0))
    if rotate not in ROTATIONS:
        raise ValueError('rotate must be 0, 90, 180 or 270')
    text = (f",drawtext=fontfile='{FONT}':text='%{{pts\\:hms}}':x=8:y=8:fontsize=26"
            ':fontcolor=white:box=1:boxcolor=black@0.6:boxborderw=6') if clock and Path('C:/Windows/Fonts/consola.ttf').exists() else ''
    return (f'crop=trunc(iw*{w}/2)*2:trunc(ih*{h}/2)*2:trunc(iw*{x}):trunc(ih*{y})'
            f'{ROTATIONS[rotate]},scale=-2:{HEIGHT}{text}')


def camera_service_up():
    try:
        with urllib.request.urlopen(CAMERA_SERVICE + '/api/status', timeout=2) as response:
            return response.status == 200
    except OSError:
        return False


class Recorder:
    """One camera stream per run; each device view is cropped and rotated from it."""

    def __init__(self, config_path, product, variants, output):
        self.directory = Path(output) / 'video'
        self.manifest = dict(schema=1, product=product, camera=None, source=None, started_at=None,
                             stopped_at=None, views=[], context=[], events=[], error=None)
        self.process = None
        self.started = None
        self.lock = threading.Lock()
        self.views, self.context = {}, []
        try:
            config = json.loads(Path(config_path).read_text(encoding='utf-8-sig'))
            if config.get('schema') != 1:
                raise ValueError('unsupported camera configuration schema')
            self.ffmpeg = config['ffmpeg']
            self.cameras = config['cameras']
            preset = config['products'][product]
            self.camera = str(preset['camera'])
            self.views = {variant: preset['views'][variant] for variant in variants
                          if variant in preset.get('views', {})}
            self.context = preset.get('context', [])
            for view in [*self.views.values(), *self.context]:
                view_filter(view)
            if not Path(self.ffmpeg).is_file() or self.camera not in self.cameras:
                raise ValueError('ffmpeg or the product camera is not installed')
        except (OSError, ValueError, KeyError, TypeError) as error:
            self.views, self.context = {}, []
            self.manifest['error'] = f'camera configuration unavailable: {error}'

    def event(self, variant, name):
        if self.started is not None:
            with self.lock:
                self.manifest['events'].append(
                    dict(t=round(time.monotonic() - self.started, 1), variant=variant, event=name))

    def _input(self, camera):
        """Prefer the bench camera service when it owns the cameras; else read DirectShow directly."""
        if camera_service_up():
            with urllib.request.urlopen(f'{CAMERA_SERVICE}/camera/{camera}', timeout=10) as response:
                response.read()
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                with urllib.request.urlopen(CAMERA_SERVICE + '/api/status', timeout=2) as response:
                    status = json.load(response)
                if any(str(c['id']) == camera and c.get('live') for c in status.get('cameras', [])):
                    break
                time.sleep(0.5)
            return 'camera-service', ['-f', 'mpjpeg', '-use_wallclock_as_timestamps', '1',
                                      '-i', f'{CAMERA_SERVICE}/stream/{camera}.mjpg']
        device = self.cameras[camera]['device']
        return 'dshow', ['-f', 'dshow', '-rtbufsize', '64M', '-video_size', '1280x720', '-framerate', '15',
                         '-vcodec', 'mjpeg', '-i', 'video=' + device]

    def snapshot(self, when):
        for index, item in enumerate(self.context):
            name = f"{item.get('name', f'context-{index}')}-{when}.jpg"
            try:
                _, source = self._input(str(item['camera']))
                subprocess.run([self.ffmpeg, '-hide_banner', '-loglevel', 'error', '-nostdin', *source,
                                '-vf', view_filter(item, clock=False), '-frames:v', '1', '-update', '1', '-y',
                                str(self.directory / name)], timeout=30, check=True,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                self.manifest['context'].append(dict(label=f"{item.get('label', name)} ({when})", file=name))
            except (OSError, subprocess.SubprocessError, KeyError) as error:
                self.manifest['error'] = f'{name}: {error}'

    def start(self):
        if not self.views:
            return
        try:
            self.directory.mkdir(parents=True, exist_ok=True)
            self.snapshot('before')
            source, arguments = self._input(self.camera)
            names = list(self.views)
            graph = f"[0:v]fps=10,split={len(names)}" + ''.join(f'[s{i}]' for i in range(len(names)))
            graph += ''.join(f";[s{i}]{view_filter(self.views[v])}[o{i}]" for i, v in enumerate(names))
            command = [self.ffmpeg, '-hide_banner', '-loglevel', 'error', *arguments, '-t', str(MAX_SECONDS),
                       '-filter_complex', graph]
            for i, variant in enumerate(names):
                command += ['-map', f'[o{i}]', '-c:v', 'libx264', '-preset', 'veryfast', '-crf', '26',
                            '-pix_fmt', 'yuv420p', '-g', '50', '-y', str(self.directory / f'{variant}.mkv')]
            with (self.directory / 'ffmpeg.log').open('wb') as log:
                self.process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                                                stderr=log)
            time.sleep(3)
            if self.process.poll() is not None:
                raise RuntimeError(f'camera {self.camera} did not start (see video/ffmpeg.log)')
            self.started = time.monotonic()
            camera = self.cameras[self.camera]
            self.manifest.update(camera=dict(id=self.camera, serial=camera.get('serial')), source=source,
                                 started_at=utc())
        except (OSError, RuntimeError, subprocess.SubprocessError, KeyError) as error:
            self.manifest['error'] = f'recording did not start: {error}'
            self.process = None

    def stop(self):
        try:
            if self.process:
                try:
                    self.process.stdin.write(b'q')
                    self.process.stdin.close()
                    self.process.wait(timeout=30)
                except (OSError, subprocess.TimeoutExpired):
                    self.process.kill()
                    self.process.wait(timeout=10)
                self.manifest['stopped_at'] = utc()
                for variant, view in self.views.items():
                    recorded = self.directory / f'{variant}.mkv'
                    final = self.directory / f'{variant}.mp4'
                    result = subprocess.run([self.ffmpeg, '-hide_banner', '-loglevel', 'error', '-nostdin', '-i',
                                             str(recorded), '-c', 'copy', '-movflags', '+faststart', '-y', str(final)],
                                            timeout=120, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                    if result.returncode == 0 and final.stat().st_size:
                        recorded.unlink()
                        self.manifest['views'].append(dict(variant=variant, label=view.get('label', variant),
                                                           file=final.name, crop=view.get('crop'),
                                                           rotate=view.get('rotate', 0)))
                    else:
                        self.manifest['error'] = f'{variant} recording could not be finalized'
            if self.views:
                self.snapshot('after')
        except (OSError, subprocess.SubprocessError) as error:
            self.manifest['error'] = f'recording did not finish: {error}'
        finally:
            if self.views or self.manifest['error']:
                self.directory.mkdir(parents=True, exist_ok=True)
                (self.directory / 'manifest.json').write_text(json.dumps(self.manifest, indent=2) + '\n',
                                                              encoding='utf-8')
