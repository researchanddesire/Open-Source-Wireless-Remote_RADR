# Lovense emulator

A BLE peripheral that pretends to be a Lovense toy, for checking what RADR
really sends without a toy in hand. It runs on a Lockbox ESP32-S3 board,
because the shared Test Rig already has two of them next to the RADR.

The emulator advertises as one Lovense model at a time, answers the queries a
real toy answers (`DeviceType;`, `Battery;`, `Status:1;`) and prints every
write it receives as one JSON line on USB serial:

```text
LVSEMU {"event":"write","t":1822540,"profile":"lush","ascii":"DeviceType;","hex":"446576696365547970653b",...}
LVSEMU {"event":"notify","t":1822555,"profile":"lush","ascii":"S:11:0082059AD3BD;","sent":true}
LVSEMU {"event":"write","t":1822870,"profile":"lush","ascii":"Vibrate:0;",...}
```

## Profiles

| Id | Emulates | Advertised name | Notes |
| --- | --- | --- | --- |
| `lush` | Lush (`S`) | `LVS-Lush` | Single vibrator |
| `edge` | Edge (`P`) | `LVS-Edge` | Two vibrators |
| `nora` | Nora (`A`) | `LVS-Nora` | Vibrator and rotation |
| `domi` | Domi (`W`) | `LVS-Domi` | Single vibrator |
| `max` | Max (`B`) | `LVS-Max` | Vibrator and air pump |
| `hush1` | Hush, first generation (`Z`) | `LVS-Hush` | Uses the Nordic UART service |
| `gush-hidden` | Gush (`ED`) | `LVS-Gush` | Advertises its name only, no service UUID |
| `mute` | A toy that never answers `DeviceType;` | `LVS-Mute` | Checks RADR's timeout |

Service UUIDs and model letters come from RADR's bundled Buttplug
`lovense.json`, so RADR treats each profile as that model.

Serial commands, one per line: `list`, `status`, `profile <id>`,
`battery <percent>` and `stop`.

The emulator advertises from its own random static address
(`f2:4c:56:53:00:01`), never the board's. A computer or phone that already
knows the board would otherwise reconnect to it by itself and block the test.

## Build

```sh
cd software/dev/lovense-emulator
pio run -e lockbox-r2    # or lockbox-r8
```

The project deliberately copies the Lockbox's platform, partition table and
sdkconfig. That lets it replace only the Lockbox application slot.

## Flash a Lockbox and put it back

A Lockbox holds a device key and settings that must survive. Write the
emulator into the application slot (`app0`, `0x10000`) and nothing else.
Never erase the flash, and never write the bootloader, partition table,
otadata, NVS or eFuses.

1. Back up the application slot and record the running build
   (`rig/backup_app_slot.py <variant> before`).
2. Write `.pio/build/lockbox-<variant>/firmware.bin` with
   `rig/flash_app_slot.py <variant> <firmware.bin> emulator`. It checks the
   chip, factory MAC, flash size, PSRAM type, partition table and boot slot,
   and fails if NVS changed during the write.
3. Run the test.
4. Write the backup (`app0.bin`) back with the same script and compare the
   boot record (`RAD_HEALTH`: build and image SHA-256) with step 1.

Both scripts expect the Test Rig layout (`C:\rad-hil\harness`, the fixture
file and the rig locks) and hold the Lockbox rig locks while they run.

## Drive RADR on the Test Rig

`rig/session.py` holds the Lockbox and RADR rig locks, logs both serial
ports, records the RADR camera view and drives RADR over RAD BLE (it needs
`bleak`, so run it from its own virtual environment, not the harness one).
Commands are JSON files dropped into its `cmd` folder:

```json
{"op": "batch", "ops": [
  {"op": "emu", "line": "profile lush"},
  {"op": "ble", "req": {"op": "target.set", "path": "state.deviceSearch"}},
  {"op": "wait", "seconds": 8},
  {"op": "ble", "req": {"op": "target.set", "path": "state.connectSelected", "args": {"index": 0}}},
  {"op": "ble", "req": {"op": "encoder.set", "path": "encoder.left", "args": {"value": 8}}},
  {"op": "ble", "req": {"op": "target.set", "path": "state.disconnect"}}
]}
```

The order of RADR's device list changes from scan to scan, and the rig's OSSM
is in it. Look at the screen (`rig/snapshot.py <name>` saves a live camera
frame) before choosing an index, so that a test never connects to the OSSM.
