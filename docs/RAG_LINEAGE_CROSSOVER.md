# FAULTMINE RAG — Crossover, lineage, favourites, and provenance

Status: FM-010 contract, project schema v2.

## Purpose

FM-010 turns exploration into a durable search history without conflating that history with the editor's undo stack. A specimen is a canonical genome tied to the canonical source identity plus explicit derivation metadata. Pixels remain disposable render products and are never lineage authority.

The implementation has four distinct layers:

1. `core::crossover_genomes` — deterministic typed multi-parent crossover;
2. `app::LineageGraph` — retained specimen DAG, active node, favourites and provenance;
3. project schema v2 — durable lineage persistence and explicit v1 migration;
4. Win32 exploration panel — ordered parent selection, breeding, favourites, lineage activation and provenance inspection.

## Identity and authority

Canonical genome identity remains `genome_identity_hex(genome)`. Lineage metadata, favourites, creation ordinals, UI state and locks do not alter that identity.

Each retained `SpecimenRecord` stores:

- canonical genome identity;
- canonical source identity;
- complete typed `Genome`;
- one authoritative derivation record;
- durable favourite flag;
- deterministic project-local `creation_ordinal`.

The source identity is repeated per record intentionally. This makes each retained specimen self-describing inside the project and allows validation to reject accidental cross-source graph contamination.

`creation_ordinal` and favourite state are non-semantic project/session metadata. They do not participate in rendering or genome identity.

## Derivation kinds

`SpecimenDerivation` has five explicit kinds:

- `manual-root` — current manual editor state materialized as a history-free lineage root;
- `mutation` — FM-009 descendant with exact mutation policy version, one parent identity, mutation seed, descendant index and radius;
- `crossover` — FM-010 child with exact crossover policy version, ordered parent identities and crossover seed;
- `imported-genome` — reserved for explicit genome import without fabricated ancestry;
- `migrated-project` — project schema v1 state migrated into v2 without inventing pre-v2 parents.

Root/import derivations must not contain parent links or algorithm-policy state. Mutation derivations must contain exactly one parent and the complete FM-009 request address. Crossover derivations require at least two ordered parents and the complete crossover seed address.

## Duplicate-genome policy

The lineage graph is keyed by canonical genome identity. Re-encountering the same canonical genome does **not** create another graph node.

Policy v1 deliberately retains the first accepted derivation as the authoritative provenance for that canonical specimen. A later duplicate may promote the existing node to favourite but does not append another derivation event. This prevents no-op mutations, convergent search and repeated crossover from exploding the graph with identity-equivalent nodes.

If the same genome identity is presented with conflicting canonical genome content or a different source identity, the operation is rejected as an identity/provenance conflict.

## DAG contract and cycle prevention

Every non-root parent identity must already refer to a retained specimen when a new runtime node is retained. Project loading performs a complete graph validation independent of serialized record order:

- specimen identities must match their canonical genomes;
- source identities must match the project source;
- genome schemas must validate against the active operator registry;
- creation ordinals and genome identities must be unique;
- every parent must exist;
- self-parent links are rejected;
- duplicate parent links are rejected;
- depth-first graph validation rejects cycles;
- the active identity must name a retained node.

Navigation follows this specimen DAG and is intentionally separate from manual edit undo/redo.

## Manual edits versus specimen lineage

A manual editor operation marks the session as `lineage_detached`. It does not silently create one lineage node per nudge, drag, reorder or undo boundary.

When a lineage-aware action needs the current manual state — mutation retention, breeding, project save, or other exploration transition — the current canonical genome is materialized as a `manual-root` if it is not already retained. It has no fabricated historical parent.

This preserves the architectural distinction:

- editor history answers “how did I edit this current genome?”;
- lineage answers “which retained specimens produced which search specimens?”

Promotion, crossover activation and lineage navigation replace editor state with a clear history boundary; they are not encoded as undo operations.

## Crossover policy v1

`kCrossoverPolicyVersion == 1`.

### Ordered parents

Parent selection order is semantic. The first selected parent is the **primary topology scaffold**. Reversing parent order is therefore allowed to produce a different child even with the same seed.

The Win32 tray exposes this explicitly: Ctrl+click assigns `P1`, `P2`, ... ranks. The Breed command submits parents in that order.

### Typed alignment

Crossover never splices serialized genome JSON.

For each primary operator, secondary parents are aligned conservatively:

1. exact stable instance ID **and** operator type, if available;
2. otherwise the nearest unused operator of the same type by stack position;
3. otherwise there is no compatible aligned operator from that parent.

Each secondary operator can satisfy at most one primary operator.

### Gene inheritance

The primary operator's stable instance ID and stack position remain the scaffold identity. For an unlocked primary operator:

- `enabled` is selected deterministically from aligned candidates;
- each declared typed parameter is selected deterministically from compatible typed values among aligned candidates;
- named entropy streams are addressed from the explicit crossover seed, primary instance ID, stack position and parameter tag.

The primary genome root seed remains the child root seed. The crossover seed controls inheritance; it does not silently rewrite the execution seed that existing operators consume.

### Locks and protected behaviour

Primary-parent protection has precedence because parent order is semantic.

- a whole-operator lock on the primary preserves that operator byte-for-byte as typed state and keeps its exact stack index;
- a primary parameter lock preserves that exact parameter value;
- unmatched secondary whole-operator locks force that operator to be inherited as an indivisible unit if capacity permits;
- unmatched operators are appended only after the complete primary scaffold, so no insertion can shift a primary topology anchor.

The v1 request structure permits locks per parent, but the interactive UI supplies the active editor lock set only when the active genome itself is one of the ordered crossover parents. Retained specimens do not retroactively acquire editor lock state as semantic specimen data.

### Unmatched operators and new instance IDs

After aligned inheritance, each unused secondary operator has an independent deterministic include/exclude decision derived from crossover seed, source instance ID, parent index and source index. A protected unmatched operator is forced in.

Inherited unmatched operators are appended, subject to the global 64-operator limit. They receive deterministic **new child instance IDs** via `derive_instance_id`; source-parent instance IDs are not copied into the new structural branch. Collision resolution is deterministic.

If a forced protected unmatched operator cannot fit under the operator limit, the crossover fails explicitly.

### Validation

All parents are validated against `OperatorRegistry` before crossover. Parent lock targets are validated before inheritance. The final typed child is validated again before it can leave the crossover layer.

The session then performs a full canonical render before adopting a crossover child, preserving the FM-009 promotion rule that a thumbnail or proxy is never promoted as canonical pixels.

## Favourites and pins

FM-009's tray pin was session-only. FM-010 changes the user-visible meaning to a durable favourite:

- toggling Favourite first retains the specimen and its exact mutation provenance;
- favourite state is stored on the lineage node;
- favourite specimens survive tray rerolls because pinned tray items remain visible;
- favourite state survives project save/reload through schema v2;
- removing the favourite flag does **not** delete the lineage node or its provenance.

The tray remains a transient visual working set. The lineage graph is the durable search record.

## Project schema v2

Serialization always writes `project_version: 2`.

Top-level fields are:

- `project_version`;
- `source`;
- active `genome`;
- active-genome mutation `locks`;
- `lineage`;
- `session`;
- `ui`.

`lineage` contains `active_genome_identity` and a `specimens` array. Each specimen serializes its identity, source identity, complete canonical genome, favourite flag, creation ordinal and fixed-shape derivation object.

The top-level active genome is intentionally retained instead of making lineage the only genome location. It preserves the existing project/editor boundary and is cross-validated against the active lineage record on load.

### v1 migration

The v2 parser accepts historical project schema v1. Migration is explicit and conservative:

- existing source, genome, locks, session and UI are parsed under the old strict field set;
- one lineage record is created from the existing active genome;
- its derivation kind is `migrated-project`;
- it has no parents and no fabricated policy/seed/index/radius state;
- it becomes the active lineage node;
- the in-memory document version is v2 and the next save serializes v2.

Unsupported future versions remain rejected.

## UI contract

The exploration panel provides:

- deterministic mutation tray generation/reroll/radius/population controls from FM-009;
- normal click for visual comparison;
- Ctrl+click to toggle ordered crossover parents (`P1`, `P2`, ...);
- Breed button/menu/hotkey (`Ctrl+Alt+B`);
- Favourite toggle backed by durable lineage state;
- lineage combo listing retained records in creation order, including derivation kind and favourite marker;
- Activate to return to any retained specimen in the list;
- Provenance to surface source/genome identity, derivation kind, parent identities and seed/index/radius where applicable.

Changing the active lineage specimen schedules a canonical preview render and refreshes the ordinary editor controls. Lineage activation is not undo/redo.

## Verification obligations

FM-010 automated coverage includes:

- same ordered parents + seed => byte-identical canonical child;
- swapped parent order demonstrates explicit order semantics;
- primary whole-operator and parameter locks survive crossover;
- unmatched inheritance creates deterministic child instance IDs;
- invalid parents are rejected before crossover;
- duplicate genome identities collapse rather than exploding lineage;
- parent edges and favourite state are retained;
- cyclic project lineage is rejected;
- project v1 migrates to one history-free `migrated-project` root;
- project v2 lineage round-trips canonically;
- session mutate -> retain favourite -> breed -> navigate ancestor/descendant -> save -> reload reproduces lineage and source identity constraints;
- native Debug and Release smoke execute mutation, durable retention, crossover and lineage navigation through the real Win32/D3D11/session path.

CI remains the repository's Windows x64 MSVC Debug + Release build/test/native-smoke gate.
