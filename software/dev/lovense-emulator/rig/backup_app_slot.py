"""Read-only snapshot of one rig Lockbox: identity, partitions, otadata, NVS hash,
full app0 backup and the boot RAD_HEALTH/RAD_ID records. Writes nothing to the device.
Usage: backup_app_slot.py <variant> <label> [nobackup]"""
from pathlib import Path
import hashlib, json, re, subprocess, sys, time
import serial
from esptool.reset import HardReset
sys.path.insert(0, 'C:/rad-hil/harness')
from run import rig_lock, find_port, load_fixture
from bundle import partition_rows

variant, label = sys.argv[1], sys.argv[2]
backup = 'nobackup' not in sys.argv
root = Path('C:/rad-hil/private-backups/lovense-emulator') / f'{variant}-{label}'
root.mkdir(parents=True, exist_ok=True)
device = load_fixture(Path('C:/rad-hil/fixture.json'), 'researchanddesire/Open-Source-Chastity-Lockbox_LKBX')[variant]

def esp(port, *args, timeout=400):
    r = subprocess.run([sys.executable, '-m', 'esptool', '--chip', 'esp32s3', '--port', port, '--baud', '460800',
                        '--after', 'no_reset', *args], capture_output=True, text=True, timeout=timeout)
    if r.returncode:
        raise RuntimeError(r.stdout[-800:] + r.stderr[-800:])
    return r.stdout

with rig_lock(Path('C:/rad-hil/locks/lockbox.lock')), rig_lock(Path('C:/lkbx-hil/fixture.lock')):
    port = find_port(device)
    out = esp(port, 'flash_id')
    mac = re.findall(r'MAC:\s*([0-9a-fA-F:]{17})', out)[-1].replace(':', '').upper()
    assert mac == device['device_id'], ('MAC mismatch', mac)
    print('port', port, 'mac', mac, 'flash', re.findall(r'Detected flash size:\s*(\S+)', out))
    reads = [('partitions', 0x8000, 0x1000), ('otadata', 0xe000, 0x2000), ('nvs', 0x9000, 0x5000)]
    if backup:
        reads.append(('app0', 0x10000, 0x640000))
    for name, offset, size in reads:
        path = root / f'{name}.bin'
        esp(port, 'read_flash', hex(offset), hex(size), str(path))
        data = path.read_bytes()
        print(name, hex(offset), len(data), 'sha256', hashlib.sha256(data).hexdigest())
        if name == 'partitions':
            print('rows', partition_rows(data))
        if name == 'otadata':
            for i in range(2):
                seq = int.from_bytes(data[i*0x1000:i*0x1000+4], 'little')
                state = int.from_bytes(data[i*0x1000+24:i*0x1000+28], 'little')
                print(f'otadata[{i}] seq=0x{seq:08x} state=0x{state:08x}')
        if name == 'app0':
            # Image length from the ESP image header: segments then checksum + optional sha256.
            n, pos = data[1], 24
            for _ in range(n):
                pos += 8 + int.from_bytes(data[pos+4:pos+8], 'little')
            pos = (pos + 1 + 15) // 16 * 16
            if data[23] == 1:
                pos += 32
            image = data[:pos]
            (root / 'app0-image.bin').write_bytes(image)
            print('app0 image bytes', pos, 'sha256', hashlib.sha256(image).hexdigest())
    p = serial.Serial(port=None, baudrate=115200, timeout=.25); p.dtr = False; p.rts = False
    p.port = port; p.open()
    try:
        p.reset_input_buffer(); HardReset(p, uses_usb=True)()
        data = b''; end = time.monotonic() + float(sys.argv[3] if len(sys.argv) > 3 and sys.argv[3].isdigit() else 30)
        while time.monotonic() < end:
            data += p.read(p.in_waiting or 1)
    finally:
        p.close()
(root / 'boot.log').write_bytes(data)
for line in data.decode('utf-8', 'replace').splitlines():
    for key in ('RAD_HEALTH {"event":"boot"', 'RAD_ID ', 'LVSEMU '):
        if key in line:
            print(line[line.find(key):][:400])
            break
print('ready heartbeats', data.count(b'"ready":true'), 'MQTT Connected', data.count(b'MQTT Connected'))
print('raw kept at', root)
