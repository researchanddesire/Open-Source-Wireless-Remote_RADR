"""Write ONE image into a rig Lockbox's app0 slot (0x10000) and nothing else.

Checks chip, factory MAC, flash size, PSRAM type, the installed partition table
and that otadata boots app0; snapshots NVS before and after the write and fails
if it changed. Never touches bootloader, partition table, otadata, NVS, spiffs,
coredump or eFuses. Usage: flash_app_slot.py <variant> <image.bin> <label> [boot_seconds]
"""
from pathlib import Path
import hashlib, sys, time
import serial
import esptool
from esptool.reset import HardReset
sys.path.insert(0, 'C:/rad-hil/harness')
from run import rig_lock, find_port, load_fixture
from bundle import partition_rows
from catalog import policy

variant, image, label = sys.argv[1], Path(sys.argv[2]), sys.argv[3]
boot_seconds = float(sys.argv[4]) if len(sys.argv) > 4 else 30
repository = 'researchanddesire/Open-Source-Chastity-Lockbox_LKBX'
spec = policy(repository, variant)
device = load_fixture(Path('C:/rad-hil/fixture.json'), repository)[variant]
root = Path('C:/rad-hil/private-backups/lovense-emulator') / f'{variant}-{label}'
root.mkdir(parents=True, exist_ok=True)
data = image.read_bytes()
app0 = next(r for r in spec['partitions'] if r[0] == 'app0')
assert data[0] == 0xE9 and len(data) <= app0[4], 'not an app image or too large for app0'
print('image', image.name, len(data), 'sha256', hashlib.sha256(data).hexdigest(), flush=True)

with rig_lock(Path('C:/rad-hil/locks/lockbox.lock')), rig_lock(Path('C:/lkbx-hil/fixture.lock')):
    port = find_port(device)
    esp = esptool.detect_chip(port=port, baud=115200, connect_attempts=3)
    try:
        esp = esp.run_stub()
        esp.change_baud(460800)
        assert esp.CHIP_NAME.lower().replace('-', '') == 'esp32s3'
        assert bytes(esp.read_mac()).hex().upper() == device['device_id'], 'MAC changed'
        assert 1 << ((esp.flash_id() >> 16) & 0xff) == spec['flash_bytes'], 'flash size changed'
        repeat, word4, word5 = (esp.read_reg(0x6000703c + n) for n in (0, 24, 28))
        capacity = (((word5 >> 19) & 1) << 2) | ((word4 >> 3) & 3)
        power = (repeat >> 8) & 1
        psram = 'r2' if power == 0 and capacity in (1, 2) else 'r8' if power == 1 and capacity == 1 else '?'
        assert psram == spec['psram'], 'PSRAM identity changed'
        assert partition_rows(esp.read_flash(0x8000, 0x1000)) == spec['partitions'], 'partition table differs'
        ota = esp.read_flash(0xe000, 0x2000)
        seqs = [int.from_bytes(ota[i*0x1000:i*0x1000+4], 'little') for i in range(2)]
        valid = [s for s in seqs if s not in (0, 0xffffffff)]
        assert not valid or (max(valid) - 1) % 2 == 0, f'otadata boots app1 {seqs}; refusing'
        print('identity ok', port, device['device_id'], psram, 'otadata', [hex(s) for s in seqs], flush=True)
        nvs = esp.read_flash(0x9000, 0x5000)
        (root / 'nvs-before-write.bin').write_bytes(nvs)
        print('nvs before', hashlib.sha256(nvs).hexdigest(), flush=True)
        esp.WRITE_FLASH_ATTEMPTS = 1
        esptool.main(['--chip', 'esp32s3', '--port', port, '--baud', '460800', '--no-stub', '--after', 'no_reset_stub',
                      'write_flash', '--compress', '--flash_mode', 'keep', '--flash_freq', 'keep', '--flash_size', 'keep',
                      hex(app0[3]), str(image)], esp=esp)
        after = esp.read_flash(0x9000, 0x5000)
        print('nvs after ', hashlib.sha256(after).hexdigest(), flush=True)
        if after != nvs:
            raise RuntimeError('NVS changed while flashing')
        print('NVS preserved; app0 written', flush=True)
    finally:
        esp._port.close()
    p = serial.Serial(port=None, baudrate=115200, timeout=.25); p.dtr = False; p.rts = False
    p.port = port; p.open()
    try:
        p.reset_input_buffer(); HardReset(p, uses_usb=True)()
        log = b''; end = time.monotonic() + boot_seconds
        while time.monotonic() < end:
            log += p.read(p.in_waiting or 1)
    finally:
        p.close()
(root / 'boot.log').write_bytes(log)
for line in log.decode('utf-8', 'replace').splitlines():
    for key in ('RAD_HEALTH {"event":"boot"', 'RAD_ID ', 'LVSEMU ', 'Guru', 'panic', 'abort'):
        if key in line:
            print(line[line.find(key):][:300]); break
print('ready heartbeats', log.count(b'"ready":true'), 'MQTT Connected', log.count(b'MQTT Connected'), 'raw', root)
