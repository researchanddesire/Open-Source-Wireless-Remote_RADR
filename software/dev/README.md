# RADR development

Local development services and settings for the `development` build belong
here, next to the PlatformIO project in `../firmware/` and the tooling in
`../scripts/`.

The `development` environment in `../firmware/platformio.ini` targets a
dashboard running on `http://localhost:3000`. RADR needs no other local
services today.

`lovense-emulator/` is a BLE peripheral for a Lockbox board that pretends to be
a Lovense toy and logs every write it receives. Use it to check what RADR sends
to a toy without having one.
