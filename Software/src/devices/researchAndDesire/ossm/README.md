# OSSM BLE Control Library - Basics

## Service & Device Info

- **Service UUID**: `522b443a-4f53-534d-0001-420badbabe69`
- **Device Name**: "OSSM"
- **Manufacturer**: "Research And Desire"

## Active Characteristics

### 1. Command Characteristic (Write)

- **UUID**: `522b443a-4f53-534d-0002-420badbabe69`
- **Purpose**: Send control commands

**Commands**:

```
set:speed:<0-100>        # Stroke speed percentage
set:stroke:<0-100>       # Stroke length percentage
set:depth:<0-100>        # Penetration depth percentage
set:sensation:<0-100>    # Sensation intensity percentage
set:pattern:<0-6>        # Stroke pattern (0-6)
go:simplePenetration     # Switch to simple penetration mode
go:strokeEngine          # Switch to stroke engine mode
go:menu                  # Return to main menu
```

**Responses**:

- Success: `ok:<original_command>`
- Failure: `fail:<original_command>`

### 2. State Characteristic (Read/Notify)

- **UUID**: `522b443a-4f53-534d-1000-420badbabe69`
- **Purpose**: Monitor device state and settings

**State JSON Format**:

```json
{
  "state": "<state_name>",
  "speed": <0-100>,
  "stroke": <0-100>,
  "sensation": <0-100>,
  "depth": <0-100>,
  "pattern": <0-6>
}
```

**Key States**:

- `idle` - Initializing
- `menu` - Main menu
- `simplePenetration` - Simple penetration mode
- `strokeEngine` - Stroke engine mode
- `error` - Error state

### 3. Speed Knob Config Characteristic (Write)

- **UUID**: `522b443a-4f53-534d-0010-420badbabe69`
- **Purpose**: Configure speed knob behavior

**Values**:

- `true` - Speed knob acts as upper limit for BLE commands (default)
- `false` - Speed knob and BLE speed are independent

### 4. Pattern List Characteristic (Read)

- **UUID**: `522b443a-4f53-534d-2000-420badbabe69`
- **Purpose**: Get available stroke patterns

**Pattern JSON Format**:

```json
[
  { "name": "Simple Stroke", "idx": 0 },
  { "name": "Teasing Pounding", "idx": 1 },
  { "name": "Robo Stroke", "idx": 2 },
  { "name": "Half'n'Half", "idx": 3 },
  { "name": "Deeper", "idx": 4 },
  { "name": "Stop'n'Go", "idx": 5 },
  { "name": "Insist", "idx": 6 }
]
```

## Connection Flow

1. Scan for "OSSM" device
2. Connect and discover service
3. Subscribe to state notifications
4. Read initial state and patterns
5. Send commands as needed

## RAD BLE Requests (Machine Settings)

The remote's OSSM settings pages use RAD BLE v1 JSON on top of the command
characteristic. Client: `ossm_rad_ble.{h,cpp}`; request building, reply
parsing and geometry validation: `lib/OssmSettingsLogic/src/ossm_rad_ble_logic.h`
(native test `test/test_ossm_rad_ble_logic`).

- **Request**: write JSON to the command characteristic
  (`522b443a-4f53-534d-1000-420badbabe69`).
- **Response characteristic** (Indicate): `522b443a-4f53-534d-1100-420badbabe69`.
  Each reply carries the request `id` and a `stage`. Skip `accepted` and wait
  for `completed` or `failed` (2 s timeout).

```json
{"v":1,"id":1,"op":"setting.read","path":"setting.machine.maxStrokeMm"}
{"v":1,"id":1,"stage":"completed","ok":true,"code":"ok","result":{"value":300,"active":500,"restartRequired":true}}
```

`value` is stored in NVS, `active` is in use since boot. Writes need a
control lease, which the remote releases straight away:

```json
{"v":1,"id":2,"op":"control.acquire","args":{"ttl":5}}
{"v":1,"id":3,"op":"setting.write","path":"setting.machine.maxStrokeMm","args":{"value":300},"lease":<token>}
{"v":1,"id":4,"op":"control.release","lease":<token>}
```

### Machine geometry paths

These apply only after the OSSM restarts.

| Path | Units | Rule |
|------|-------|------|
| `setting.machine.stepsPerRev` | steps | > 0, whole number |
| `setting.machine.pulleyToothCount` | teeth | > 0, whole number |
| `setting.machine.beltPitchMm` | mm | > 0 |
| `setting.machine.maxRpm` | rpm | > 0 |
| `setting.machine.maxStrokeMm` | mm | > 50 |

The whole set must also give at least 2 steps/mm and at most 100000 steps/s.
The OSSM checks each write against its stored values with only that field
changed, so the remote orders multi-field writes so every step is valid.
An OSSM without the response characteristic, or one that replies
`unknown_path` or times out, shows "Update OSSM to change settings".

### Remote UI

On the control screens, Menu is lit when the OSSM is paused or at speed 0 and
opens the settings list (its first row opens the main menu). Pressing it
while it is grey shows "Pause First" for 1.5 s. Kept edits stay on the remote
until the confirm screen's Restart writes them and restarts the OSSM; Cancel
drops them and Edit returns to the list.
