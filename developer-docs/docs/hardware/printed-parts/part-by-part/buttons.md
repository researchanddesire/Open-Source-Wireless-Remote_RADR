# Buttons

import { STLViewer } from '/snippets/ossm/stl-render.jsx';

<STLViewer url="https://raw.githubusercontent.com/researchanddesire/radr-wireless-remote/main/Hardware/Print%20Files/Buttons.stl" />

## Function

The Buttons are small caps that sit over the tactile switches beneath the display. This component:

- Provides a comfortable press surface for the three under-screen buttons
- Transfers actuation force to the underlying tactile switches
- Features optional translucent sections for LED status indicators
- Three buttons required per remote

## Print Settings

| Setting | Value |
|---------|-------|
| Layer Height | 0.12mm |
| Walls | 4 |
| Infill Type | Gyroid |
| Infill Percentage | 30% |
| Supports | Minimal or none |

!!! tip
    These are small parts. Use a brim for bed adhesion if needed, and print multiple at once for efficiency.

## Multi-Color Printing (Optional)

The buttons include a separate body for the LED indicator area:

| Area | Purpose | Recommended Material |
|------|---------|---------------------|
| Indicator window | Shows button status LED | Transparent/translucent |
| Button face | Press surface | Any opaque color |

!!! info
    Multi-color printing is optional. Single-color buttons work perfectly—you just won't see the status LEDs through the button caps.

## Installation

### Step 1: Print three buttons

  Print all three button caps using the settings above.

  !!! success
Buttons are free of stringing and have clean surfaces.
  

### Step 2: Test fit

  Place each button into its housing slot in the top enclosure. They should move freely up and down.

  !!! warning
If buttons bind or stick, lightly sand the sides. Do not force them into place.
  

### Step 3: Verify actuation

  With the PCB installed, press each button to confirm it properly actuates the underlying switch.

  !!! success
Each button clicks cleanly and springs back when released.
  

## Download Files
**[STEP File](https://github.com/researchanddesire/radr-wireless-remote/blob/main/Hardware/Print%20Files/Buttons.step)**

Download for CAD editing

**[3MF Project](https://github.com/researchanddesire/radr-wireless-remote/blob/main/Hardware/Print%20Files/Bambu%20A1%20Remote_V1.01.3mf)**

Bambu Studio project with print settings
