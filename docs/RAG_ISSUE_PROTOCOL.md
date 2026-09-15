# FAULTMINE autonomous issue protocol

Every `FM-###` implementation issue is written so an autonomous implementation agent can complete it without relying on inaccessible chat context.

## Required references

Every issue must direct the implementer to read the current versions of:

- `README.md`
- `AGENTS.md`
- `docs/RAG_INDEX.md`
- `docs/RAG_PRODUCT.md`
- `docs/RAG_ARCHITECTURE.md`
- `docs/RAG_ROADMAP.md`
- `docs/RAG_VERIFICATION.md`
- `docs/RAG_ISSUE_PROTOCOL.md`
- current `main`, accepted prerequisite implementations, repository tests/workflows, and the issue discussion itself

The issue body may add issue-specific constraints, but it must not quietly contradict the authoritative repository contracts.

## Standard issue structure

Each issue should contain:

1. **Objective** — the concrete user/system capability to establish.
2. **Why now** — dependency position and why the issue exists.
3. **Prerequisites** — accepted `FM-###` work or contracts that must already be on `main`.
4. **Required scope** — complete set of implementation responsibilities.
5. **Architectural constraints** — relevant invariants from the RAG docs, restated where failure would be especially costly.
6. **Expected artefacts** — source areas, tests, docs, workflows or executables likely to change/create, without over-prescribing exact filenames where current architecture may evolve.
7. **Verification** — automated tests and manual smoke evidence required.
8. **Acceptance criteria** — observable conditions that must be true on `main`.
9. **Non-goals** — nearby attractive work that should not expand scope.
10. **Autonomous completion protocol** — branch -> implementation -> tests -> docs -> PR -> CI repair -> merge -> verify `main` -> close.

## Execution prompt

Unless an issue explicitly narrows the process, the following instruction is implied and should be included or referenced:

> Implement this issue completely against current `main`. Treat the issue, `AGENTS.md`, the RAG documentation set, current accepted code/tests and prerequisite implementations as authoritative. Inspect before modifying; do not duplicate existing mechanisms. Add/adjust tests and documentation for changed contracts. Open a focused PR referencing this issue, run the relevant automated checks, repair failures, and merge after all required checks pass. You have permission to merge your own PR for this repository. Verify from the repository that the merge landed on `main`; only then close the issue if every acceptance criterion is genuinely satisfied. Do not stop at planning/scaffolding unless the issue explicitly requests it.

## Handling ambiguity

An implementation agent should not block on minor design choices already bounded by the repository contracts. Choose the simplest design consistent with current architecture, document the choice in code/PR where material, and continue.

Ask for external/user input only when a decision materially changes product semantics and cannot be resolved from the authoritative documents or accepted code.

If an issue's exact implementation prescription is stale but the intent remains valid, adapt to current `main` rather than rebuilding old architecture.

## Handling discovered defects

- Fix a small defect in scope when it blocks correct completion and the fix is clearly necessary.
- If a distinct serious defect is discovered but does not belong in the issue, document evidence and open a focused follow-up issue where repository policy permits.
- Do not conceal unrelated failures by weakening tests.
- Do not rewrite large unrelated subsystems merely because a cleaner design is imaginable.

## PR expectations

PR description should include:

- issue reference;
- summary of behaviour established;
- important design decisions;
- files/subsystems affected;
- tests/checks run and CI status;
- deterministic golden/schema changes, if any;
- user-visible behaviour or screenshots where useful;
- known limitations/follow-up issues.

## Merge expectations

A PR may merge when:

- required CI checks pass;
- issue acceptance criteria are implemented rather than deferred;
- persisted-format changes are documented/migrated as required;
- no known test failure is hidden or reclassified without justification;
- relevant documentation matches the merged behaviour.

After merge, explicitly verify `main` contains the accepted commit/PR before closing the issue.

## Documentation references are live

Issues reference repository documents by path rather than copying every architectural paragraph. Implementers must read the current document versions, because accepted prerequisite PRs may have refined the contracts since the issue was created.

Issue-specific acceptance criteria remain binding unless a later accepted repository decision explicitly supersedes them.
