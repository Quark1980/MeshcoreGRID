# MeshcoreGRID Project Instructions

This repository extends MeshCore. Preserve MeshCore core behavior and data models unless a change is explicitly required.

MeshCore base code should remain untouched whenever possible so future upstream MeshCore updates can be integrated without unnecessary conflicts or regressions.

## Core-first rule

For all feature work, fixes, UI integrations, storage changes, protocol handling, BLE behavior, channel management, time handling, and mesh behavior:

- Always prefer existing MeshCore core functions, structures, storage flows, and protocol semantics.
- Keep MeshCore base implementation untouched unless a change is strictly necessary and there is no safe extension point.
- Always check whether MeshCore already provides a function, slot model, persistence path, or protocol command before adding new logic.
- Do not create parallel GRID-only implementations when an existing MeshCore mechanism already exists.
- Do not duplicate or reinterpret MeshCore state in a separate custom model unless there is no safe core path available.
- When extending behavior, hook into the existing MeshCore flow instead of replacing it.
- Protect future upstream mergeability: prefer thin GRID layers over edits to MeshCore core files.

## Practical expectations

- Use existing channel APIs and storage slots instead of custom channel memory.
- Use existing BLE startup/control paths instead of inventing alternate transport state.
- Use existing MeshCore packet, contact, channel, and persistence semantics as the source of truth.
- Keep protocol compatibility with upstream MeshCore whenever possible.
- If a new GRID-specific layer is required, keep it UI-focused and thin over the MeshCore base.

## Decision rule

Before implementing a new feature, first answer:
1. Which existing MeshCore function or flow already does this?
2. Can GRID call or extend that directly?
3. Only if not, add the smallest possible project-specific layer.

When in doubt, choose the solution that keeps MeshCore base behavior most intact.
