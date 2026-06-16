# AGENTS.md - RADR Wireless Remote

Guidance for agents working in this product repository.

## Ohai Assembly Docs Contract

RADR is an Ohai-eligible product repo. Keep the GitHub topic
`ohai-assembly-docs` only when the assembly package is complete and local
assembly/build checks pass.

Required assembly package files:

- `assembly-docs/site.yml`
- `assembly-docs/nav.yml`
- `assembly-docs/index.md`
- `assembly-docs/pcb-overview.md`
- `assembly-docs/cable-harnesses.md`
- `assembly-docs/bom.md`
- `assembly-docs/assembly-guide.md`

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
