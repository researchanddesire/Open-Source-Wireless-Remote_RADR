"""Lovense emulator session: holds the Lockbox and RADR rig locks, logs both
serial ports, records the RADR camera view and drives the RADR over RAD BLE.

Commands are JSON files dropped into <work>/cmd/ (processed in name order);
results are appended to <work>/results.jsonl. The session ends on {"op":"quit"}
or after --minutes, always disconnecting the RADR from the emulator and
stopping the emulator's advertising first.

Never flashes, resets or writes flash on any device.
"""
import argparse
import asyncio
import json
import subprocess
import sys
import threading
import time
from pathlib import Path

import serial
from bleak import BleakClient, BleakScanner

sys.path.insert(0, 'C:/rad-hil/harness')
from run import rig_lock, find_port, load_fixture  # noqa: E402
import video  # noqa: E402

RADR_SERVICE = '522b443a-5241-4452-0001-420badbabe69'
REQUEST = RADR_SERVICE.replace('-0001-', '-1000-')
RESPONSE = RADR_SERVICE.replace('-0001-', '-1100-')
INFO = RADR_SERVICE.replace('-0001-', '-0002-')
KEEP = ('[RAD BLE]', 'RAD_HEALTH', 'RAD_ID', 'LOVENSE', 'Lovense', 'BUTTPLUGIO', 'COMS', 'REGISTRY',
        'DEVICE', 'Guru', 'panic', 'abort', 'assert', 'E (', 'W (', 'Connect', 'connect')

parser = argparse.ArgumentParser()
parser.add_argument('--work', default='C:/rad-hil/manual-pr-evidence/lovense-emulator/session')
parser.add_argument('--minutes', type=float, default=25)
parser.add_argument('--variant', default='r2')
options = parser.parse_args()
work = Path(options.work)
(work / 'cmd').mkdir(parents=True, exist_ok=True)
private = Path('C:/rad-hil/private-backups/lovense-emulator/session-' + time.strftime('%Y%m%d-%H%M%S'))
private.mkdir(parents=True, exist_ok=True)
started = time.monotonic()
results = (work / 'results.jsonl').open('a', encoding='utf-8')
lock = threading.Lock()


def now():
    return round(time.monotonic() - started, 3)


def record(kind, **fields):
    fields = dict(t=now(), wall=time.strftime('%H:%M:%S'), kind=kind, **fields)
    with lock:
        results.write(json.dumps(fields) + '\n')
        results.flush()
    print(json.dumps(fields)[:400], flush=True)


class Port(threading.Thread):
    def __init__(self, name, device_name, keep_all):
        super().__init__(daemon=True)
        self.name, self.keep_all = name, keep_all
        self.port = serial.Serial(port=None, baudrate=115200, timeout=.2)
        self.port.dtr = False
        self.port.rts = False
        self.port.port = device_name
        self.port.open()
        self.raw = (private / f'{name}.raw.log').open('ab')
        self.evidence = (work / f'{name}.log').open('a', encoding='utf-8')
        self.running = True
        self.buffer = b''

    def run(self):
        while self.running:
            try:
                chunk = self.port.read(self.port.in_waiting or 1)
            except serial.SerialException as error:
                record('serial_error', port=self.name, error=str(error))
                time.sleep(1)
                continue
            if not chunk:
                continue
            self.raw.write(chunk)
            self.raw.flush()
            self.buffer += chunk
            while b'\n' in self.buffer:
                line, self.buffer = self.buffer.split(b'\n', 1)
                text = line.decode('utf-8', 'replace').rstrip('\r')
                if self.keep_all or any(k in text for k in KEEP):
                    self.evidence.write(f'{now():9.3f} {text}\n')
                    self.evidence.flush()

    def write(self, text):
        self.port.write(text.encode() + b'\n')
        self.port.flush()


class Radr:
    def __init__(self):
        self.client = None
        self.lease = None
        self.next_id = 1000
        self.pending = {}

    def on_response(self, _char, data):
        try:
            response = json.loads(bytes(data))
        except ValueError:
            record('ble_bad_response', data=bytes(data).hex())
            return
        future = self.pending.get(int(response.get('id', 0)))
        if future and not future.done() and response.get('stage') in ('completed', 'failed'):
            future.get_loop().call_soon_threadsafe(future.set_result, response)

    async def connect(self):
        device = await BleakScanner.find_device_by_filter(
            lambda d, ad: RADR_SERVICE in [u.lower() for u in ad.service_uuids], timeout=20)
        if device is None:
            raise RuntimeError('RADR RAD BLE service not found')
        self.client = BleakClient(device, timeout=20)
        await self.client.connect()
        info = json.loads(await self.client.read_gatt_char(INFO))
        await self.client.start_notify(RESPONSE, self.on_response)
        record('ble_connected', address=device.address, name=device.name,
               info={k: info.get(k) for k in ('deviceType', 'version', 'firmware', 'capabilityHash')})
        acquired = await self.request('control.acquire', args={'ttl': 30}, lease=False)
        self.lease = int(acquired['result']['lease'])

    async def request(self, op, path=None, args=None, lease=True):
        self.next_id += 1
        message = {'v': 1, 'id': self.next_id, 'op': op}
        if path:
            message['path'] = path
        if args is not None:
            message['args'] = args
        if lease and self.lease is not None:
            message['lease'] = self.lease
        future = asyncio.get_running_loop().create_future()
        self.pending[self.next_id] = future
        await self.client.write_gatt_char(REQUEST, json.dumps(message, separators=(',', ':')).encode(),
                                          response=True)
        try:
            response = await asyncio.wait_for(future, 12)
        finally:
            self.pending.pop(self.next_id, None)
        record('ble', request=message, response=response)
        return response

    async def renew(self):
        while True:
            await asyncio.sleep(10)
            if self.lease is not None and self.client and self.client.is_connected:
                try:
                    message = {'v': 1, 'id': 1, 'op': 'control.renew', 'args': {'ttl': 30},
                               'lease': self.lease}
                    self.next_id += 1
                    message['id'] = self.next_id
                    await self.client.write_gatt_char(REQUEST, json.dumps(message).encode(), response=True)
                except Exception as error:  # noqa: BLE001 - diagnostic only
                    record('renew_error', error=repr(error))


def grab_frame(recorder, name):
    source = recorder.directory / 'r8.mkv'
    target = work / 'frames' / f'{name}.jpg'
    target.parent.mkdir(exist_ok=True)
    completed = subprocess.run([recorder.ffmpeg, '-hide_banner', '-loglevel', 'error', '-i', str(source),
                                '-update', '1', '-q:v', '3', '-y', str(target)],
                               capture_output=True, text=True, timeout=90)
    return str(target) if target.exists() else completed.stderr[-300:]


async def execute(command, emulator, radr, recorder):
    op = command.get('op')
    try:
        if op == 'quit':
            record('quit')
            return True
        if op == 'emu':
            emulator.write(command['line'])
            record('emu', line=command['line'])
        elif op == 'ble':
            if not radr.client.is_connected:
                await radr.connect()
            await radr.request(command['req']['op'], command['req'].get('path'),
                               command['req'].get('args'))
        elif op == 'wait':
            await asyncio.sleep(float(command['seconds']))
            record('waited', seconds=command['seconds'])
        elif op == 'mark':
            recorder.event('r8', command['label'])
            record('mark', label=command['label'])
        elif op == 'frame':
            record('frame', name=command['name'], result=grab_frame(recorder, command['name']))
        else:
            record('unknown_op', op=op)
    except Exception as error:  # noqa: BLE001 - keep the session alive
        record('command_error', command=command, error=repr(error))
    return False


async def main():
    lockbox = load_fixture(Path('C:/rad-hil/fixture.json'),
                           'researchanddesire/Open-Source-Chastity-Lockbox_LKBX')[options.variant]
    radr_device = load_fixture(Path('C:/rad-hil/fixture.json'),
                               'researchanddesire/Open-Source-Wireless-Remote_RADR')['r8']
    emulator = Port('emulator', find_port(lockbox), keep_all=True)
    radr_port = Port('radr', find_port(radr_device), keep_all=False)
    emulator.start()
    radr_port.start()
    video.MAX_SECONDS = int(options.minutes * 60) + 120
    recorder = video.Recorder('C:/rad-hil/cameras.json', 'radr', ['r8'], work)
    recorder.start()
    record('session_started', camera=recorder.manifest.get('error') or 'recording', private=str(private))
    radr = Radr()
    renew = None
    deadline = time.monotonic() + options.minutes * 60
    try:
        await radr.connect()
        renew = asyncio.create_task(radr.renew())
        while time.monotonic() < deadline:
            files = sorted((work / 'cmd').glob('*.json'))
            if not files:
                await asyncio.sleep(0.2)
                continue
            path = files[0]
            try:
                command = json.loads(path.read_text(encoding='utf-8-sig'))
                path.unlink()
            except (OSError, ValueError) as error:
                await asyncio.sleep(0.3)
                if path.exists() and time.time() - path.stat().st_mtime > 10:
                    record('bad_command', file=path.name, error=str(error))
                    path.rename(path.with_suffix('.bad'))
                continue
            commands = command['ops'] if command.get('op') == 'batch' else [command]
            stop = False
            for item in commands:
                if await execute(item, emulator, radr, recorder):
                    stop = True
                    break
            if stop:
                break
    finally:
        record('cleanup_begin')
        try:
            if radr.client and radr.client.is_connected:
                await radr.request('target.set', 'state.disconnect')
                await radr.request('target.set', 'state.mainMenu')
                await radr.request('control.release')
                await radr.client.disconnect()
        except Exception as error:  # noqa: BLE001
            record('cleanup_error', error=repr(error))
        if renew:
            renew.cancel()
        emulator.write('stop')
        time.sleep(1)
        recorder.stop()
        emulator.running = radr_port.running = False
        time.sleep(0.5)
        record('session_ended', video=str(recorder.directory), manifest_error=recorder.manifest.get('error'))


with rig_lock(Path('C:/rad-hil/locks/lockbox.lock')), rig_lock(Path('C:/lkbx-hil/fixture.lock')), \
        rig_lock(Path('C:/rad-hil/locks/radr.lock')):
    asyncio.run(main())
