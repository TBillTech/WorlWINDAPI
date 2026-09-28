# WWATP - Copilot Instructions

WWATP is a C++20 agent communication layer built on QUIC and HTTP/3. It provides a
tree-based data model, composable backends and frontends, C++ and JavaScript client
libraries, and server/test programs.

Correctness, protocol clarity, determinism, and testability have priority over
convenience or speculative abstraction.

---

## Non-negotiables
- Avoid hidden global state, process-wide mutable singletons, and static caches unless explicitly approved.
- Keep tree state, backend behavior, protocol rules, and serialization independent of UI or transport adapters where practical.
- Frontends and transports are adapter layers: translate external events into explicit backend operations and expose state without owning it.
- Prefer explicit dependency injection and composition over runtime discovery or implicit registration.
- Make resource ownership, stream lifetimes, shutdown, and thread boundaries explicit.

---

## State and Protocol Discipline
- All tree mutations must go through explicit backend or transaction operations.
- Preserve versioning, conflict-policy, and transaction invariants at the owning backend boundary.
- Do not silently initialize or mutate shared state from an adapter constructor.
- Network behavior must define ownership, ordering, retries, cancellation, and shutdown.
- Randomness, clocks, and concurrency must be explicit dependencies when they affect observable behavior.
- Changes to wire formats or public interfaces require compatibility and regression tests.

---

## Resource and Adapter Lifecycle
- Define creation, ownership, cancellation, shutdown, and thread boundaries explicitly.
- Keep frontends and transports as thin adapters around backend operations.

---

## Architecture Expectations
- Frontends must not bypass backend interfaces or mutate backend-owned state directly.
- Core backends should not depend on UI, browser, or transport implementation details.
- Keep adapters thin, readable, and responsible for cleanup.
- Prefer explicit composition over inheritance.

---

## Data and Configuration
- Keep configuration separate from mutable runtime tree state.
- Use existing structured representations and parsers for YAML, JSON, and protocol data.
- Avoid premature data-driven abstractions and unrelated API redesigns.

---

## Quality Gates
- Behavior changes require a deterministic reproduction sequence or a focused automated test.
- C++ changes should be checked with the relevant CMake target and Catch2/backend tests.
- JavaScript changes should be checked with the `js_client_lib` Vitest suite and configured lint/type checks.
- Do not refactor unrelated files opportunistically.
- Optimize for clarity over cleverness.

---

## GitHub Tickets (Allowed)
- Agents may read/search existing GitHub Issues/PRs (“tickets”) for context, prior art, acceptance criteria, and to propose follow-up tickets.
- Prefer referencing existing tickets over re-specifying requirements from memory.

---

## Dependency Boundaries
- Treat vendored or externally built libraries under `libraries/` as implementation dependencies, not places for opportunistic edits.
- Do not guess unavailable third-party API signatures. Stop at the project-owned adapter/interface and document the missing capability.
- Keep CI-safe code and tests independent of machine-local credentials, services, and untracked dependencies.
