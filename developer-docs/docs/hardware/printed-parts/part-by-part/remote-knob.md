# Remote - Encoder Knob

import { STLViewer } from '/snippets/ossm/stl-render.jsx';

<STLViewer url="https://raw.githubusercontent.com/researchanddesire/radr-wireless-remote/main/Hardware/Print%20Files/Remote%20-%20Knob.stl" />

## Function

The Encoder Knobs are press-fit caps for the RADR wireless remote's rotary encoders. This component:

- Provides an ergonomic grip surface for encoder control
- Press-fits onto the encoder shafts
- Features optional translucent sections for LED backlighting
- Two knobs required per remote (left and right encoders)

## Print Settings

| Setting | Value |
|---------|-------|
| Layer Height | 0.12mm |
| Walls | 4 |
| Infill Type | Gyroid |
| Infill Percentage | 25% |
| Supports | Yes - Auto |

!!! tip
    Print with the shaft cavity facing upward for best surface quality inside the bore.

## Multi-Color Printing (Optional)

The knobs include a separate body for the backlight ring area:

| Area | Purpose | Recommended Material |
|------|---------|---------------------|
| Backlight ring | Shows encoder LED colors | Transparent/translucent |
| Main body | Grip surface | Any opaque color |

## Installation

### Step 1: Prepare the part

  Remove supports and lightly deburr the bore with a hobby knife or small round file.

  !!! success
The bore is clean and smooth; no support material remains inside.
  

### Step 2: Test fit on the encoder shaft

  Align the knob with the encoder shaft. Do not force it—ensure alignment first.

  !!! warning
Excessive force can damage the encoder. Apply steady, straight pressure only.
  

### Step 3: Press-fit the knob

  Press straight down until fully seated. Repeat for the second encoder.

  !!! success
Knobs rotate smoothly and the remote registers encoder turns and clicks.
  

## Troubleshooting
!!! note "Fit is too tight"
    - Lightly ream the bore with a small round file or deburring tool.
    - Re-slice with −0.5% to −1% XY scale or use a negative horizontal expansion value.
    - Warm the knob slightly (hair dryer) and press-fit while warm.

!!! note "Fit is too loose"
    - Re-slice with +0.5% to +1% XY scale.
    - Add a thin wrap of PTFE/plumber's tape to the shaft as a temporary shim.

!!! note "Support marks inside the bore"
    - Increase support Z-distance/interface layers.
    - Switch to tree supports or reduce support density.
    - Orient with the bore up to minimize internal supports.

## Download Files
**[STEP File](https://github.com/researchanddesire/radr-wireless-remote/blob/main/Hardware/Print%20Files/Remote%20-%20Knob.step)**

Download for CAD editing

**[3MF Project](https://github.com/researchanddesire/radr-wireless-remote/blob/main/Hardware/Print%20Files/Bambu%20A1%20Remote_V1.01.3mf)**

Bambu Studio project with print settings
