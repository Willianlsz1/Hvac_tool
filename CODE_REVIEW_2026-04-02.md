# Deep Code Review Notes (2026-04-02)

This document captures a structured technical review of the current single-page HVAC maintenance app.

## Key findings

1. The JS module path in `index.html` points to `./js/app.js`, but code currently lives in `hvca/js/`.
2. UI rendering, business logic, and persistence remain tightly coupled in `hvca/js/ui.js`.
3. Alerts are computed per record rather than per equipment/plan, leading to duplicated and potentially noisy alerts.
4. `localStorage` is used without schema versioning, migration strategy, encryption, or integrity checks.
5. The data model does not yet capture core HVAC operational telemetry (pressures, temperatures, superheat/subcooling).

## Suggested architecture direction

- Split into:
  - `domain/` (entities, validation, rules)
  - `application/` (use cases)
  - `infra/` (storage adapters)
  - `ui/` (renderers + interaction)
- Add a repository layer now to decouple local storage from future API backend.
- Introduce schema versioning and migrations.
- Normalize schedules into first-class entities instead of deriving everything from records.

## Operational reliability notes

- Add backup/export + import flow and checksum.
- Add conflict-safe sync assumptions if moving to multi-device usage.
- Introduce audit trail fields (`createdBy`, `createdAt`, `updatedAt`, `sourceDevice`).
