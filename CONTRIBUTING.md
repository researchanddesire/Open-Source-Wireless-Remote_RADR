# Contributing to RADR

Firmware and hardware are **CERN-OHL-S v2 only** (no MPL split).

## Developer documentation

Edit **`developer-docs/`** in this repo. Published at [dev.researchanddesire.com/radr](https://dev.researchanddesire.com/radr/).

User guides: [`simple-docs`](https://github.com/researchanddesire/simple-docs) `docs/radr/`.

## Build

Software lives in `software/`: the PlatformIO project in `software/firmware/`,
tooling in `software/scripts/`, and local development services in
`software/dev/`.

```bash
cd software/firmware
pio run -e development
```

## Hardware BOMs

`hardware/bom.csv` follows the [BOM standard](https://dev.researchanddesire.com/meta/bom-standard/) — fixed columns, closed category codes, `–` for empty cells. **Anyone is welcome to edit `bom.csv` directly and push/PR — as long as `bom-lint` passes, it's good.** Only the schema is vendored read-only at `.github/workflows/scripts/bom.schema.json` (do not hand-edit — synced from [`dev-docs`](https://github.com/researchanddesire/dev-docs)). The legacy `hardware/*.xlsx` BOMs should migrate into `hardware/bom.csv` over time.

## Branches

Open pull requests against `staging`. `main` receives reviewed promotions from `staging` and urgent hotfixes.
