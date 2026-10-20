# AGENTS.md - RADR Wireless Remote

Guidance for agents working in this product repository.

## Repository Layout

- The PlatformIO firmware project lives at the repository root: `platformio.ini`,
  `src/`, `include/`, `lib/`, `data/`, `test/`, and `config/` (boards and
  `partition.csv`). Run `pio` commands from the root.
- `hardware/` holds print files, PCB sources, cables, CAD, and `bom.csv`.
- `developer-docs/` is the MkDocs developer site; `assembly-docs/` is the hub
  mirror described below.
- `ota/` is the GitHub Pages payload, including the production `v/latest` bridge.


## Technician Build PC

- Use the `$build-pc` skill for the Research + Desire Windows workstation that technicians use to build and flash devices for sale.
- Keep the workstation checkout on a clean, current `main`; preserve unexpected work with a stash or backup before synchronization.
- Repair build-machine tooling, dependencies, and caches there. Reproduce source defects there, but fix tracked code or project configuration in this development repository and land the correction through the normal branch workflow.

## Branch and Release Policy

- Persistent branches are lowercase `staging` and `main`.
- Fetch before branching. Start feature and release work from `origin/staging`, then open the pull request back into `staging`.
- Only urgent production bug fixes may target `main` directly. Start them from `origin/main`, open the pull request into `main`, and make the standalone word `hotfix` the first word of the PR title (case-insensitive).
- Promote normal releases with a same-repository `staging` to `main` pull request only after every required build, unit, integration, artifact-verification, and RADR hardware validation succeeds.
- Immediately merge every main hotfix back into `staging` with a `main` to `staging` pull request.
- Squash feature and hotfix pull requests into one focused commit with a concise, imperative title. Merge long-lived branch synchronization pull requests (`staging` to `main` and `main` to `staging`) with a merge commit; never squash or rebase them.
- Use matching branch names and linked pull requests for changes spanning RAD App or another firmware repository.
- Staging firmware reports the `staging` track and checks `https://staging.researchanddesire.com`. Main firmware reports `main` and checks `https://dashboard.researchanddesire.com`.
- Firmware is update-eligible only after its immutable Supabase artifacts and all required validation records verify. Missing hardware runners leave a release in `validating`; never bypass a gate.

## Development Safety

- Keep stable eFuse identity, update protocol, filesystem/application ordering, and rollback behavior covered by native tests.
- Never commit credentials, local build products, or generated secrets.

## PR hardware validation

- Every PR into staging or main builds all hardware variants, including docs-only changes.
- Install the exact staging bundle for the PR merge SHA; production still builds.
- Require Build validation and Hardware validation in addition to existing checks.
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

This repository is the English-only radr spoke of the rad-app hub.
`assembly-docs/**` mirrors only
`rad-app/apps/docs-ohai/content/docs/radr/**`. Hub `page.en.mdx` becomes
`page.mdx` here, and `meta.en.json` becomes `meta.json`. Product assets,
KiCad sources and `_bom/` snapshots travel unchanged. Keep CSV, provenance and
generated JSON together; do not add translated pages or other products.

The notifier wakes rad-app after content changes. Reviewed sync PRs import to
hub staging, then fan out to the full mirror. Never sync directly to
assembly-docs or another spoke. `.github/assembly-sync.json` is the hub-managed
baseline; do not hand-edit it. Initial replacement of legacy MkDocs content
requires a separately reviewed hub seed PR. After seeding, use Fumadocs
`meta.json` and MDX; do not restore `site.yml`, `nav.yml`, or generated HTML
BOM markers. Hardware BOM source remains human-owned and outside content sync.

Synchronization runs main to main only; staging receives assembly docs
through the main-to-staging back-merge. The notifier uses `RAD_VERSION_CONTROL_APP_ID` and
`RAD_VERSION_CONTROL_PRIVATE_KEY` with a token scoped only to rad-app.

Hardware source folders are `hardware/cad/`, `hardware/pcb/`, and
`hardware/cables/`.

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
