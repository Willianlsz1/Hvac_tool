# CoolTrack Pro — Deep Technical Review (Frontend-Only, Hospital HVAC Context)

## 🔴 Critical Issues (must fix)

### 1) Data integrity can be silently corrupted (single JSON blob in localStorage)
- **Problem:** Entire operational state is persisted as one mutable JSON object (`cooltrack_v3`) without schema versioning, migrations, checksums, locking, or append-only history.
- **Why it matters:** In hospital operations, maintenance history is auditable evidence. A malformed write, user tab conflict, or manual browser data clear can erase critical traceability.
- **Suggested fix:**
  - Add schema version + migration pipeline.
  - Split storage by entity (`equipamentos`, `registros`, `meta`).
  - Add journaling (append-only event log) and periodic snapshots.
  - Move to IndexedDB now, backend ASAP.

```js
const DB_VERSION = 2;
const safeState = { version: DB_VERSION, equipamentos: [], registros: [], events: [] };
```

---

### 2) XSS risk through direct `innerHTML` rendering of unsanitized user inputs
- **Problem:** User-provided fields (`obs`, `pecas`, equipment names, locations) are interpolated into template strings and rendered with `innerHTML`.
- **Why it matters:** Even in local-first apps, malicious payloads can execute scripts in the technician’s browser, exfiltrate local data, or corrupt UI state.
- **Suggested fix:**
  - Render dynamic content via `textContent` when possible.
  - If HTML rendering is required, sanitize all user strings with a strict allowlist sanitizer.
  - Centralize escaping helper.

```js
function esc(s='') {
  return s.replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}
```

---

### 3) Alert logic produces duplicate/ambiguous operational signals
- **Problem:** Alerts derive from every record `proxima` plus equipment `danger` status, without dedupe by equipment or selecting latest schedule baseline.
- **Why it matters:** Technicians can receive multiple contradictory alerts for one asset, reducing trust and delaying action.
- **Suggested fix:**
  - Compute alert state per equipment from the **latest valid planning record**.
  - Introduce alert identity key (`equipId + alertType + dueDate`) and deduplicate.

---

### 4) Status model is oversimplified and can regress incorrectly
- **Problem:** Equipment status is overwritten by the latest saved record status with no approval state, no reason code, no SLA context.
- **Why it matters:** Critical assets may appear “OK” after a non-root-cause visit; unsafe in hospital HVAC (OR/UTI pressure/temperature compliance).
- **Suggested fix:**
  - Use finite-state model (`operational`, `degraded`, `out_of_service`, `under_monitoring`).
  - Track transition reason, actor, timestamp, and optional supervisor confirmation.

---

### 5) Photo persistence can exceed storage quotas and break saves
- **Problem:** Photos are stored as base64 in localStorage within records.
- **Why it matters:** localStorage quota is small (often ~5MB). A few photos can exceed quota and fail persistence with poor error handling.
- **Suggested fix:**
  - Store media in IndexedDB (Blob) with thumbnails.
  - Save only metadata/references in record.
  - Add try/catch with user feedback on quota errors.

---

### 6) Missing domain validations for mandatory HVAC operational safety
- **Problem:** No structured field validations for measured values (pressures, supply/return temps, electrical current, ΔT).
- **Why it matters:** Hospital systems need measurable proof, not only free-text notes.
- **Suggested fix:**
  - Add typed measurement schema with min/max rules per equipment type.
  - Block closure of critical corrective ticket unless mandatory measurements are present.

## 🟡 Improvements

### Architecture & Scalability
- Current code is cleaner than a fully procedural script (modules like `Store`, `Alerts`, `Historico`), but still tightly coupled through global DOM IDs and shared mutable state.
- Recommended target architecture:
  1. **Domain layer:** entities + rules (Equipment, WorkOrder, AlertPolicy).
  2. **Application layer:** use-cases (`createRecord`, `closeAlert`, `schedulePM`).
  3. **Infrastructure:** persistence adapters (localStorage/IndexedDB/API).
  4. **UI layer:** rendering/components.
- For long-term scale: migrate to TypeScript + component framework (React/Vue/Svelte) + state container (Zustand/Pinia/Redux Toolkit).

### Code Quality
- Strength: naming improved vs v1 (`Store`, `Templates`, `Utils`, `Registro`).
- Still needed:
  - Enforce linting/formatting (ESLint + Prettier).
  - Add JSDoc or TS interfaces for `Equipamento`, `Registro`, `Alert`.
  - Replace magic strings with enums/constants (service type, fluid type, status codes).

### Performance
- Repeated sorts/filter scans (`regsForEquip().sort(...)`, alert recomputation per render) become O(n log n) hotspots.
- Add memoized selectors and precomputed indexes:
  - `Map<equipId, registros[]>`
  - `lastRegByEquipId`
- Avoid full view rerenders on single-record updates; patch update specific nodes.

### Security
- Add CSP header when served in production.
- Add input length limits (e.g., notes max 2000 chars, tags max 50).
- Add integrity checksum for persisted payload.
- Add role concept even in frontend mock (`technician`, `supervisor`) to prepare for backend ACL.

### UX / Real Technician Workflow
- Positive mobile-first direction, but workflow gaps:
  - No **offline sync status** indicator.
  - No **quick critical actions** (mark out-of-service + call escalation).
  - No **checklists** by equipment type.
  - No **task queue by priority/zone** for shift planning.
- Add one-tap “Start/Finish intervention”, elapsed time, and handoff notes.

### Business Logic
- Need separation between:
  - planned preventive maintenance (PM)
  - corrective incident
  - inspection-only event
- Add SLA timers and compliance KPIs:
  - MTTR, MTBF, overdue %, first-time-fix rate
- For scheduling: maintain one active PM plan per equipment, not per record.

### HVAC Domain Enhancements (high-value)
- Add structured fields per intervention:
  - suction/discharge pressure
  - superheat/subcooling
  - return/supply air temperature + ΔT
  - compressor/ fan current draw
- Add automatic rules:
  - High superheat + low suction => possible undercharge/restriction
  - Low ΔT + clean filter false => coil/fan diagnostic prompts
- Add critical-area compliance flags (OR, ICU): pressure cascade and temperature ranges.

### Persistence & Reliability
- Roadmap:
  1. **Now:** IndexedDB + service worker + local backup export/import.
  2. **Next:** Backend API with append-only event log and server timestamps.
  3. **Then:** Multi-user conflict resolution, audit trail, signed actions.

## 🟢 What is well done
- Good visual hierarchy and mobile-first interaction model for field technicians.
- Logical module partitioning (`Store`, `Alerts`, `Templates`, `Registro`).
- Consistent status semantics (`ok/warn/danger`) across UI.
- Nice practical touches: photo capture, historical timeline, printable report.
- Defensive optional chaining in many lookups reduces hard crashes.

## 🚀 Suggested Next Steps
1. **Stabilize data model** with schema versioning + typed entities.
2. **Eliminate XSS vectors** by sanitizing or rendering text-only user input.
3. **Move media to IndexedDB** and keep metadata references in records.
4. **Refactor alert engine** to per-equipment effective schedule with dedupe.
5. **Introduce HVAC measurement form** (pressures, temperatures, electrical).
6. **Implement offline-first reliability** (queue, sync state, backup/export).
7. **Prepare backend contract** (auth, audit trail, multi-user, role policies).

---

### Production viability verdict
Current version is a strong prototype and significantly improved in structure/UX, but **not yet safe for production hospital operations** due to persistence fragility, XSS surface, and insufficient domain-grade validation/auditability.
