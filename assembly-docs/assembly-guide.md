# RADR Wireless Remote Assembly Guide

## Purpose

This work instruction documents the current RADR wireless remote assembly process from prepared PCB and printed parts through final device test.

Use these instructions with the active PCB, printed parts, and BOM release for the build.

## Required Parts and Consumables

1. Gather the required parts before starting assembly.

2. Confirm the assembly set includes:

    - `RADR-PCB-assembly`
    - `screen`
    - `battery`
    - `front-housing`
    - `back-housing`
    - `battery-flap`
    - 2x `side-button`
    - `middle-button`
    - `power-slider-button`
    - `knob`
    - `weather-stripping`
    - 5x `BC-0408LTFB` tri-lobe screws
    - `hot-glue`

3. If all required parts are present, continue onward.

## Tools

- Rasp
- T8 bit for tri-lobe screws
- Electric screwdriver or hand driver
- Isopropyl alcohol
- Microfiber cloth
- Razor blade
- Hot glue gun
- Small screwdriver

## Prepare the PCBA

1. Deburr the edges of the `RADR-PCB-assembly` with a rasp until the board edge is smooth.

    <figure class="photo-frame" style="max-width: 520px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/pcba-deburr-edges.webp" alt="RADR PCB assembly with edge areas marked for deburring" style="width: 100%; height: auto;">
      <figcaption>Deburr the PCBA edges until smooth.</figcaption>
    </figure>

2. Wipe the marked weather-strip area on the PCBA with `isopropyl-alcohol` and a `microfiber-cloth`.

    <figure class="photo-frame" style="max-width: 520px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/pcba-clean-weather-strip-area.webp" alt="Marked weather-strip area on the RADR PCB assembly" style="width: 100%; height: auto;">
      <figcaption>Clean the marked weather-strip area.</figcaption>
    </figure>

3. Attach a 2.5 cm piece of `weather-stripping` to the center of the cleaned area.

    <figure class="photo-frame" style="max-width: 520px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/weather-strip-installed.webp" alt="Weather stripping installed on the RADR PCB assembly" style="width: 100%; height: auto;">
      <figcaption>Weather stripping installed on the PCBA.</figcaption>
    </figure>

4. Fit the printed `power-slider-button` over the PCBA on/off switch.

    <figure class="photo-frame" style="max-width: 520px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/power-switch-cover-installed.webp" alt="Printed power switch cover installed over the PCBA on off switch" style="width: 100%; height: auto;">
      <figcaption>Power switch cover installed.</figcaption>
    </figure>

5. Connect the 800 mAh LiPo `battery` to the PCBA.

    Keep the PCBA switched off after connecting the battery. The PCBA produces a green light when powered on and does not produce that light when switched off.

    <figure class="photo-frame" style="max-width: 520px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/battery-connected-to-pcba.webp" alt="LiPo battery connected to the RADR PCB assembly" style="width: 100%; height: auto;">
      <figcaption>Battery connected to the PCBA.</figcaption>
    </figure>

6. Confirm the PCBA is deburred, padded, fitted with the power switch cover, connected to the battery, and switched off before continuing.

## Prepare the Front Housing

1. Use a `razor-blade` to cut a small triangle off both bumpers on the `front-housing`.

    These cuts stop the bumpers from catching on the PCBA during installation.

    <div class="photo-row" style="display: flex; flex-wrap: wrap; gap: 1rem; margin: 0.75rem 0 1.5rem 1.5rem;">
      <figure class="photo-frame" style="max-width: 360px; margin: 0;">
        <img src="assets/front-housing-bumper-cut-lines.webp" alt="Front housing bumper features marked for trimming" style="width: 100%; height: auto;">
        <figcaption>Trim both bumper features.</figcaption>
      </figure>
      <figure class="photo-frame" style="max-width: 360px; margin: 0;">
        <img src="assets/front-housing-bumper-trimmed.webp" alt="Front housing bumper after trimming" style="width: 100%; height: auto;">
        <figcaption>Trimmed interior bumper reference.</figcaption>
      </figure>
    </div>

2. Remove the protective film from the `screen`.

    <figure class="photo-frame" style="max-width: 520px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/screen-protective-film-removal.webp" alt="Protective film being removed from the RADR display" style="width: 100%; height: auto;">
      <figcaption>Remove the display film.</figcaption>
    </figure>

3. Install the `screen` into the `front-housing`.

    Insert the screen into the bottom lip of the interior front housing first. Gently flex the housing outward while pressing the screen into the upper screen lip. Confirm the screen is flush against the housing.

    <div class="photo-row" style="display: flex; flex-wrap: wrap; gap: 1rem; margin: 0.75rem 0 1.5rem 1.5rem;">
      <figure class="photo-frame" style="max-width: 360px; margin: 0;">
        <img src="assets/screen-bottom-lip-insertion.webp" alt="Display being inserted into the lower lip of the RADR front housing" style="width: 100%; height: auto;">
        <figcaption>Insert the screen into the bottom lip first.</figcaption>
      </figure>
      <figure class="photo-frame" style="max-width: 360px; margin: 0;">
        <img src="assets/screen-seated-flush.webp" alt="Display seated flush in the RADR front housing" style="width: 100%; height: auto;">
        <figcaption>Screen seated flush in the housing.</figcaption>
      </figure>
    </div>

4. Run a thin, low-profile bead of `hot-glue` along the screen edges.

    Keep the glue bead in contact with both the screen edge and the plastic housing. The housing plastic should melt slightly into the glue.

    <figure class="photo-frame" style="max-width: 520px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/screen-hot-glue-bead.webp" alt="Hot glue bead around the RADR display edge" style="width: 100%; height: auto;">
      <figcaption>Low-profile glue bead around the screen edge.</figcaption>
    </figure>

5. Drop the printed buttons into the `front-housing`.

    <figure class="photo-frame" style="max-width: 520px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/buttons-installed-in-front-housing.webp" alt="Printed RADR buttons installed in the front housing" style="width: 100%; height: auto;">
      <figcaption>Buttons installed in the front housing.</figcaption>
    </figure>

6. Confirm the bumpers are trimmed, the screen is glued in place, and the buttons move freely before continuing.

## Install the PCBA

1. Feed the screen ribbon through the cutout in the PCBA, then press the PCBA flush into the `front-housing`.

    <figure class="photo-frame" style="max-width: 520px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/pcba-seated-in-front-housing.webp" alt="RADR PCB assembly seated in the front housing with screen ribbon routed through the PCB cutout" style="width: 100%; height: auto;">
      <figcaption>PCBA seated in the front housing.</figcaption>
    </figure>

2. Open the screen ribbon connector on the PCBA with a small screwdriver.

    <figure class="photo-frame" style="max-width: 520px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/screen-connector-open.webp" alt="Open screen ribbon connector on the RADR PCB assembly" style="width: 100%; height: auto;">
      <figcaption>Open the screen connector.</figcaption>
    </figure>

3. Insert the screen ribbon into the connector and close the connector.

    <figure class="photo-frame" style="max-width: 520px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/screen-ribbon-connected.webp" alt="Screen ribbon inserted into the RADR PCB connector" style="width: 100%; height: auto;">
      <figcaption>Screen ribbon connected.</figcaption>
    </figure>

4. Tuck the screen ribbon into the housing.

    <figure class="photo-frame" style="max-width: 520px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/screen-ribbon-tucked.webp" alt="Screen ribbon tucked into the RADR housing" style="width: 100%; height: auto;">
      <figcaption>Screen ribbon tucked into the housing.</figcaption>
    </figure>

5. Confirm the PCBA sits flush, the ribbon connector is closed, and the ribbon is tucked away from pinch points before continuing.

## Close the Housing

1. Place the `back-housing` over the `front-housing` by inserting the on/off switch into its cutout.

    <figure class="photo-frame" style="max-width: 420px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/bottom-housing-power-switch-alignment.webp" alt="RADR back housing aligned over the front housing with the power switch entering its cutout" style="width: 100%; height: auto;">
      <figcaption>Align the power switch with the back-housing cutout.</figcaption>
    </figure>

2. Pull the `battery` through the open space in the back housing that will become the battery compartment.

    <div class="photo-row" style="display: flex; flex-wrap: wrap; gap: 1rem; margin: 0.75rem 0 1.5rem 1.5rem;">
      <figure class="photo-frame" style="max-width: 360px; margin: 0;">
        <img src="assets/battery-pulled-through-bottom-housing.webp" alt="Battery pulled through the RADR back housing battery compartment opening" style="width: 100%; height: auto;">
        <figcaption>Pull the battery through the battery compartment opening.</figcaption>
      </figure>
      <figure class="photo-frame" style="max-width: 360px; margin: 0;">
        <img src="assets/battery-routing-reference.webp" alt="Battery routing reference with RADR front and back housings partially closed" style="width: 100%; height: auto;">
        <figcaption>Battery routing reference.</figcaption>
      </figure>
    </div>

3. Using a medium torque setting, fasten the front and back housings together with tri-lobe screws.

    The screw is deep enough when the screwdriver ratchets.

    <figure class="photo-frame" style="max-width: 520px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/rear-screws-fastened.webp" alt="RADR rear housing screw locations fastened with tri-lobe screws" style="width: 100%; height: auto;">
      <figcaption>Fasten the rear housing screws.</figcaption>
    </figure>

4. Fasten the front-housing screw holes with the same tri-lobe screws.

    <figure class="photo-frame" style="max-width: 420px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/front-screws-fastened.webp" alt="RADR front housing screw locations fastened with tri-lobe screws" style="width: 100%; height: auto;">
      <figcaption>Fasten the front housing screws.</figcaption>
    </figure>

5. Place the `battery` inside the battery compartment.

    <figure class="photo-frame" style="max-width: 420px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/battery-in-compartment.webp" alt="RADR battery placed inside the battery compartment" style="width: 100%; height: auto;">
      <figcaption>Battery placed in the compartment.</figcaption>
    </figure>

6. Place the `battery-flap` over the battery and screw it shut with tri-lobe screws.

    <figure class="photo-frame" style="max-width: 520px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/battery-cover-installed.webp" alt="RADR battery cover installed over the battery compartment" style="width: 100%; height: auto;">
      <figcaption>Battery cover installed.</figcaption>
    </figure>

7. Confirm the housing is fully closed, all screws are seated, and the battery cover is secured before continuing.

## Install Knob and Test

1. Use a small screwdriver to clean any excess filament from the interior knob circle.

    <figure class="photo-frame" style="max-width: 520px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/knob-interior-cleaned.webp" alt="Interior bore of the RADR knob after cleaning excess filament" style="width: 100%; height: auto;">
      <figcaption>Clean the knob bore before installation.</figcaption>
    </figure>

2. Align the encoder shape with the interior knob circle shape and press the `knob` onto the encoder until it softly clicks.

    <figure class="photo-frame" style="max-width: 520px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/knob-installed-on-encoder.webp" alt="RADR knob being pressed onto the rotary encoder" style="width: 100%; height: auto;">
      <figcaption>Press the knob onto the encoder.</figcaption>
    </figure>

3. Turn on the device and test as per protocol.

    <figure class="photo-frame" style="max-width: 520px; margin: 0.75rem 0 1.5rem 1.5rem;">
      <img src="assets/device-test-on.webp" alt="RADR wireless remote powered on for final test" style="width: 100%; height: auto;">
      <figcaption>Device powered on for final test.</figcaption>
    </figure>

## Final Assembly

1. Confirm the PCBA edges are smooth and do not catch on the housing.
2. Confirm the weather stripping is centered and adhered to the PCBA.
3. Confirm the power switch cover moves the on/off switch cleanly.
4. Confirm the front-housing bumpers are trimmed.
5. Confirm the screen is flush and glued with a low-profile bead.
6. Confirm the buttons are installed and move freely.
7. Confirm the PCBA sits flush in the front housing.
8. Confirm the screen ribbon is fully inserted and the connector is closed.
9. Confirm the screen ribbon and battery wires are routed away from pinch points.
10. Confirm the housings and battery cover are fastened with tri-lobe screws.
11. Confirm the knob is seated on the encoder.
12. Confirm the device powers on and passes the current functional test protocol.

## QC Checklist

- [ ] PCBA edges were deburred until smooth.
- [ ] Weather-strip area was cleaned with isopropyl alcohol.
- [ ] 2.5 cm weather-strip piece was installed on the PCBA.
- [ ] Printed power switch cover was installed.
- [ ] Battery was connected with the PCBA switched off.
- [ ] Front-housing bumpers were trimmed.
- [ ] Screen film was removed.
- [ ] Screen was installed flush in the front housing.
- [ ] Screen was secured with a thin, low-profile hot glue bead.
- [ ] Buttons were dropped into the front housing.
- [ ] Screen ribbon was fed through the PCBA cutout.
- [ ] PCBA was pressed flush into the housing.
- [ ] Screen ribbon connector was opened, seated, and closed.
- [ ] Screen ribbon was tucked into the housing.
- [ ] Back housing was aligned around the on/off switch.
- [ ] Battery was pulled through the battery compartment opening.
- [ ] Front and back housings were fastened with tri-lobe screws.
- [ ] Battery was placed inside the battery compartment.
- [ ] Battery cover was installed and screwed shut.
- [ ] Knob bore was cleaned of excess filament.
- [ ] Knob was pressed onto the encoder until it clicked.
- [ ] Device was powered on and tested as per protocol.
