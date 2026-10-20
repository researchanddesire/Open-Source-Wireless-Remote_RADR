# AGENTS.md - RADR Wireless Remote

Guidance for agents working in this product repository.

## Repository Layout

- The PlatformIO firmware project lives at the repository root: `platformio.ini`,
  `src/`, `include/`, `lib/`, `data/`, `test/`, and `config/` (boards and
  `partition.csv`). Run `pio` commands from the root.
- `hardware/` holds `Print Files/`, `Photos/`, `pcb/`, `cad/`, `cables/`, and `bom.csv`.
- `developer-docs/` is the MkDocs developer site; `assembly-docs/` is the hub
  mirror described below.
- No GitHub Pages. Firmware releases publish only to Supabase: immutable
  releases, then the `master`, `production`, and `alpha` aliases.


## Technician Build PC

- Use the `$build-pc` skill for the Research + Desire Windows workstation that technicians use to build and flash devices for sale.
- Keep the workstation checkout on a clean, current `main`; preserve unexpected work with a stash or backup before synchronization.
- Repair build-machine tooling, dependencies, and caches there. Reproduce source defects there, but fix tracked code or project configuration in this development repository and land the correction through the normal branch workflow.

## Branch and Release Policy

- Persistent branches are lowercase `staging` and `main`. Fetch before branching.
- The firmware directory is the firmware project at the repository root (`src/`, `include/`, `lib/`, `data/`, `config/`, `test/`, `ButtplugIO/`, `platformio.ini` and the build scripts). `FIRMWARE_PATHS` in `pr-firmware.yml` and `pr-target-policy.yml` and the `publish_firmware.yml` push paths define it; keep them in sync.
- Firmware changes enter `staging` first: branch from `origin/staging` and open the PR into `staging`. Promote with a same-repository `staging` → `main` PR.
- A production fix may take firmware to `main` directly from a `fix/<issue>` branch (an owner prefix such as `AJ/fix/<issue>` also works) started from `origin/main`.
- Changes outside the firmware directory (documentation, hardware, BOMs, workflows) may open their PR straight into `main`; they need no build or hardware checks. `PR target policy` enforces these rules.
- After every merge into `main` that is not a promotion, `hotfix-sync.yml` merges `main` back into `staging` directly (main wins documentation, staging keeps its own firmware version) and opens a `main` → `staging` PR only when a conflict needs a person.
- Squash feature and fix PRs into one focused commit with a concise, imperative title. Merge `staging` → `main` promotions and `main` → `staging` sync PRs with a merge commit; never squash or rebase them.
- Use matching branch names and linked pull requests for changes spanning RAD App or another firmware repository.
- Staging firmware reports the `staging` track and checks `https://staging.researchanddesire.com`. Main firmware reports `main` and checks `https://dashboard.researchanddesire.com`.
- Firmware is update-eligible only after its immutable Supabase artifacts and all required validation records verify. Missing hardware runners leave a release in `validating`; never bypass a gate.

## Supabase

- The only Supabase project agents and workflows address is `acjajruwevyyatztbkdf`, through the organization secret `SUPABASE_ACCESS_TOKEN`. Name it explicitly in every remote operation; never infer a project from a URL, environment variable, cached CLI link, neighboring repository or prior conversation.
- Firmware releases reach it only through the dashboard firmware API. PR hardware evidence videos go to its private `hil-evidence` bucket.

## Memory Rules (RAD-2158)

- The RADR never opens a TLS socket while NimBLE is initialised. NimBLE plus the RAD BLE service cost ~100 KB of internal RAM at boot and a TLS handshake needs ~50 KB in one block; the largest free block with BLE up is ~24 KB. Network jobs about the OSSM (pairing, update) run on the OSSM over a BLE trigger; the RADR's own OTA tears BLE down first.
- Every task on an internal-RAM stack is created through `createInternalTask` (or `startTask`) so an out-of-memory condition logs the heap state and shows on screen. Never call `xTaskCreate*` bare and drop the result.
- Boot logs `[MEM] boot after BLE init: ... largest=...` at error level. A release whose largest block is below `RADR_MIN_LARGEST_BLOCK` (24 KB; measured 31.7 KB at boot on 2026-09-08) on the desk unit must not be promoted; record the number in the release PR.

## Development Safety

- Keep stable eFuse identity, update protocol, filesystem/application ordering, and rollback behavior covered by native tests.
- Never commit credentials, local build products, or generated secrets.

## PR hardware validation

- PRs into `staging` or `main` that change the firmware directory, or the validation tooling (`scripts/hil/`, `pr-firmware.yml`, `hardware-run.yml`, `.github/scripts/hil_evidence.py`), build every hardware variant and run the exact staging bundle for the PR merge SHA on the shared Test Rig. Other PRs skip both; `Build validation` and `Hardware validation` then pass without work.
- `PR target policy`, `Build validation` and `Hardware validation` are required on `staging` and `main`.
- The rig records the product's camera for the whole run; `Hardware evidence` comments on the PR with each device's video and result, pass or fail. Video never decides a result.
- Merging a firmware change into `staging` or `main` allocates the next version and publishes a signed candidate through `publish_firmware.yml`.
- Firmware bundles and hardware evidence are Actions artifacts, never tracked files.
- The reviewed harness and fixture inventory live outside the runner work directory.
- Never infer product identity or flash capacity from a COM port or advertising name.
- Follow scripts/hil/README.md for enrollment, shared-device locks, and rollout.
- Keep Wi-Fi credentials, fixture identities, and raw device logs outside the repository.
- A missing device or incomplete three-minute observation must fail validation.

## Development display capture

- Development builds expose the read-only USB `screen` command; staging and production omit it.
- Follow developer-docs/docs/development/screen-capture.md; use the display mutex for snapshots and keep serial transmission outside it.
- Preserve the capture mirror when adding TFT bulk drawing paths. Keep capture files outside Git.

## Serial identity

- All firmware tracks expose the read-only boot/`info` identity described in developer-docs/docs/development/serial-identity.md.
- Keep one serial input reader; development screen commands register with it.
- Report target memory and measured memory separately; a label never authorizes flashing or motion.

## Ohai Assembly Docs Contract

- `assembly-docs/` on `main` is the authoritative source for the RADR pages on ohai.researchanddesire.com. The rad-app hub (`scripts/assembly-sync/config.json` in researchanddesire/rad-app) imports it main to main only and merges the import automatically. Staging is never synced; for documentation, `main` always wins.
- `.github/workflows/trigger-assembly-docs.yml` notifies the hub on pushes to `main` that touch `assembly-docs/**`, and the hub also polls hourly. There is no checkpoint file.
- Every change to `assembly-docs/` on `main`, including deletions, reaches the live site within minutes without review. Do not delete or move written documentation without AJ's approval, and do not rewrite `main` history.
- Page files are `.mdx` without a language suffix; the hub publishes them as `docs/radr/<page>.en.mdx`. Translations live in researchanddesire/assembly-docs.
- Keep `assembly-docs/_bom/source.json` and `bom.json` consistent with `_bom/bom.csv`: `bom.json` is the source fields plus the parsed columns and rows. The hub rejects stale or mismatched BOM data.

## BOM And Cable Rules

- `hardware/bom.csv` is the product-level BOM source and is human-owned.
- Use the fixed RAD 12-column BOM header and only canonical category codes.
- Use the en dash `–` for empty values; do not leave blank cells.
- Product BOMs list cable harnesses as top-level assemblies.
- Detailed cable BOMs belong to Wireviz output. Link generated `.bom.tsv`
  artifacts from assembly docs instead of copying child cable BOM rows into
  `hardware/bom.csv`.
- Do not commit generated assembly-site output, rendered BOM HTML, `.DS_Store`,
  `cable-export/`, or generated cable render outputs.

## Workflow

- Do not auto-commit changes unless explicitly asked.
- Keep changes reviewable and scoped to the request.
- Prefer existing project patterns over new abstractions.
