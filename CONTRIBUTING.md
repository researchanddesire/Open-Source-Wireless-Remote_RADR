# Contributing to RADR

Firmware and hardware are **CERN-OHL-S v2 only** (no MPL split).

## Developer documentation

Edit **`developer-docs/`** in this repo. Published at [dev.researchanddesire.com/radr](https://dev.researchanddesire.com/radr/).

User guides: [`simple-docs`](https://github.com/researchanddesire/simple-docs) `docs/radr/`.

## Build

```bash
pio run -e development
```

## Hardware BOMs

`hardware/bom.csv` follows the [BOM standard](https://dev.researchanddesire.com/meta/bom-standard/) — fixed columns, closed category codes, `–` for empty cells. **Anyone is welcome to edit `bom.csv` directly and push/PR — as long as `bom-lint` passes, it's good.** Only the schema is vendored read-only at `.github/workflows/scripts/bom.schema.json` (do not hand-edit — synced from [`dev-docs`](https://github.com/researchanddesire/dev-docs)). The legacy `hardware/*.xlsx` BOMs should migrate into `hardware/bom.csv` over time.

## OSS staging

Structural prep on **`opensource-prep`** / `RADR-OSS` until cutover. See [STAGING.md](STAGING.md).
