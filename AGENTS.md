# AGENTS.md - RADR Wireless Remote

Guidance for agents working in this product repository.

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

Normal synchronization is staging to staging; main follows reviewed
staging-to-main promotion. The notifier uses `RAD_VERSION_CONTROL_APP_ID` and
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
