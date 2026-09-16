# FAULTMINE RAG index

This is the retrieval entry point for agents and maintainers. Read the documents below before implementation work; together they define the product and engineering contract.

## Authoritative documents

- [`../README.md`](../README.md) — project charter and top-level product principles.
- [`../AGENTS.md`](../AGENTS.md) — autonomous issue execution protocol and non-negotiable engineering invariants.
- [`RAG_PRODUCT.md`](RAG_PRODUCT.md) — product intent, workflows, terminology and non-goals.
- [`RAG_ARCHITECTURE.md`](RAG_ARCHITECTURE.md) — system decomposition, deterministic engine contract, data model and safety boundaries.
- [`RAG_DETERMINISM.md`](RAG_DETERMINISM.md) — accepted genome v1, seed/instance IDs, named entropy, canonical JSON and identity byte-level contract.
- [`RAG_EXTERNAL_DECODERS.md`](RAG_EXTERNAL_DECODERS.md) — external-decoder isolation, non-canonical decoder behaviour, freeze/materialize rules and laboratory provenance.
- [`RAG_ROADMAP.md`](RAG_ROADMAP.md) — initial plan, review findings, improved dependency-ordered implementation plan and milestones.
- [`RAG_VERIFICATION.md`](RAG_VERIFICATION.md) — deterministic testing, CI, performance and release verification strategy.
- [`RAG_ISSUE_PROTOCOL.md`](RAG_ISSUE_PROTOCOL.md) — standard structure and completion protocol used by every `FM-###` GitHub implementation issue.

## Authority and conflict resolution

1. Current accepted code/tests on `main` define implemented reality.
2. Explicit contracts in `AGENTS.md` and these RAG documents define intended architecture.
3. A specific issue defines the scope of its change.
4. If an issue becomes stale relative to accepted prerequisite work, preserve intent and acceptance criteria while adapting to current `main`; document any material reconciliation in the PR.
5. Do not silently resolve genuine contract conflicts. Reconcile documentation in the same PR or create a focused follow-up issue where appropriate.

## Core vocabulary

- **source** — normalized input image/data plus identity metadata.
- **fault operator** — a safe deterministic transformation that deliberately models a representational, addressing, bit, signal, reconstruction or state error.
- **fault stack / pipeline** — ordered operator graph for a specimen; v1 begins as a serial stack but serialization must permit future typed expansion.
- **genome** — complete serializable description of pipeline topology and parameters, excluding incidental UI state.
- **seed** — explicit 64-bit root deterministic entropy used to derive named random streams under `RAG_DETERMINISM.md`.
- **operator instance ID** — stable persisted 128-bit identity independent of list position.
- **specimen** — rendered result from source + genome + seed + engine version (+ explicit tick for temporal work).
- **gene** — mutable unit of genome state used by exploration controls.
- **lock** — mutation protection applied to one or more genes/operators.
- **mutation radius** — magnitude/topological freedom allowed while producing descendants.
- **lineage** — parent/child/crossover provenance among specimens.
- **canonical output** — result of the authoritative deterministic CPU engine.
- **preview** — interactive presentation; it must not silently redefine canonical semantics.
- **materialized laboratory source** — validated normalized pixels retained from an external-decoder experiment so downstream canonical work does not depend on rerunning decoder-specific recovery behaviour.

## v1 target

A compact Windows x64 application capable of opening an image, constructing and editing a deterministic fault stack, exploring seeded descendants in a specimen tray, locking/mutating/breeding results, saving projects/genomes, and exporting reproducible still or animated output. Advanced codec/binary corruption and batch mining are later v1 phases but are architected from the start as bounded extensions rather than unsafe shortcuts.
