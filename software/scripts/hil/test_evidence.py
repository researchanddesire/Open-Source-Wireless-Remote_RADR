"""Video evidence never decides a result; the OSSM guard interlock always can."""
from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ossm_guard import GuardWatch, unsafe_reason  # noqa: E402
from video import Recorder, view_filter  # noqa: E402


def guard_status(**changes):
    status = dict(time=datetime.now(timezone.utc).isoformat(), streamConnected=True, rateHz=60.0,
                  sampleAgeMs=4.0, activeFault=None, faultCount=0, safe=True, alerts={},
                  stopResults=[dict(stopVerified=True)], rail=dict(enabled=True, critical=False),
                  mode='collision-only', handback=dict(verified=True), contacts=[])
    status.update(changes)
    return status


class GuardTests(unittest.TestCase):
    def reason(self, **changes):
        status = guard_status(**changes)
        status['status_age_s'] = (datetime.now(timezone.utc) - datetime.fromisoformat(status['time'])).total_seconds()
        return unsafe_reason(status)

    def test_handed_back_guard_permits_the_run(self):
        self.assertIsNone(self.reason())

    def test_every_unprotected_state_blocks_the_run(self):
        old = (datetime.now(timezone.utc) - timedelta(seconds=30)).isoformat()
        for changes in (dict(time=old), dict(streamConnected=False), dict(rateHz=12.0), dict(sampleAgeMs=None),
                        dict(sampleAgeMs=900.0), dict(activeFault='stall'), dict(stopResults=[]),
                        dict(rail=dict(critical=dict(reason='trip'))), dict(mode='strict'),
                        dict(handback=None), dict(safe=False, alerts=dict(supply_out_of_range={}))):
            with self.subTest(changes=changes):
                self.assertIsNotNone(self.reason(**changes))

    def test_missing_guard_refuses_before_any_write(self):
        with tempfile.TemporaryDirectory() as root:
            watch = GuardWatch(Path(root) / 'missing.json', root)
            with self.assertRaises(RuntimeError):
                watch.require()
            watch.close()
            self.assertIn('unavailable', json.loads((Path(root) / 'rs485-guard.json').read_text())['preflight'])

    def test_fault_during_the_run_fails_it(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / 'status.json'
            path.write_text(json.dumps(guard_status()))
            watch = GuardWatch(path, root)
            watch.require()
            path.write_text(json.dumps(guard_status(faultCount=1, activeFault='Hard-stop contact exceeds 200 ms')))
            for _ in range(30):
                if watch.failure:
                    break
                watch._stop.wait(0.1)
            with self.assertRaises(RuntimeError):
                watch.check()
            watch.close()


class VideoTests(unittest.TestCase):
    def test_presets_are_validated(self):
        self.assertIn('hflip,vflip', view_filter(dict(crop=[0.1, 0.1, 0.5, 0.5], rotate=180), clock=False))
        for view in (dict(crop=[0.8, 0, 0.5, 1]), dict(crop=[0, 0, 1, 1], rotate=45), dict(crop=[0, 0, 0, 1])):
            with self.subTest(view=view), self.assertRaises(ValueError):
                view_filter(view)

    def test_missing_camera_configuration_records_why_without_raising(self):
        with tempfile.TemporaryDirectory() as root:
            recorder = Recorder(Path(root) / 'cameras.json', 'dtt', ['v1', 'v2'], root)
            recorder.start()
            recorder.event('v1', 'flashing')
            recorder.stop()
            manifest = json.loads((Path(root) / 'video' / 'manifest.json').read_text())
            self.assertIn('camera configuration unavailable', manifest['error'])
            self.assertEqual(manifest['views'], [])

    def test_camera_that_cannot_start_is_reported(self):
        with tempfile.TemporaryDirectory() as root:
            ffmpeg = Path(root) / 'ffmpeg.exe'
            ffmpeg.write_bytes(b'')
            config = dict(schema=1, ffmpeg=str(ffmpeg), cameras={'1': dict(device='cam', serial='S')},
                          products=dict(dtt=dict(camera='1', views=dict(v1=dict(crop=[0, 0, 1, 1])))))
            (Path(root) / 'cameras.json').write_text(json.dumps(config))
            recorder = Recorder(Path(root) / 'cameras.json', 'dtt', ['v1'], root)
            with patch('video.camera_service_up', return_value=False), \
                 patch('video.subprocess.Popen', side_effect=OSError('busy')):
                recorder.start()
            recorder.stop()
            manifest = json.loads((Path(root) / 'video' / 'manifest.json').read_text())
            self.assertIn('recording did not start', manifest['error'])


if __name__ == '__main__':
    unittest.main()
