# FAULTMINE session and D3D11 presentation contract

This document records the interactive application/session and presentation boundaries established by FM-004. It is authoritative for the first native interactive shell until a later accepted issue explicitly supersedes part of it.

## Architectural boundary

FM-004 keeps three concerns distinct:

```text
canonical core / fault semantics
        ^
        |
application session + deterministic proxy policy
        ^
        |
Win32 controls / D3D11 presentation
```

D3D11 never executes canonical fault operators in FM-004. It receives already-rendered canonical or explicitly proxy-preview RGBA8 CPU buffers and displays them as textures.

Window repaint, resize, zoom, pan, before/after selection, D3D device recreation and GPU vendor/driver state do not alter source bytes, genome state, root seed or canonical result bytes.

## Session model

`faultmine_session` owns non-window application state that is testable without a GPU:

- current normalized full-resolution source and source identity;
- current starter genome;
- deterministic preview proxy/cache state;
- cached preview result;
- semantic render-dirty state and render generation counter;
- view state (fit/actual/custom zoom, pan and before/after selection).

It depends on the canonical core but not Win32, D3D11 or WIC.

WIC loading happens before a source enters the session. A source is accepted only when the supplied source identity matches the canonical bytes. Failed file loads therefore leave the previous session unchanged.

## Semantic render scheduling

Semantic rendering is not performed from `WM_PAINT`.

When source/genome/proxy-semantic state changes, the Win32 shell posts a dedicated `WM_APP` render message. That handler calls the session's canonical CPU pipeline once, updates the cached preview, uploads presentation pixels and invalidates the window.

`WM_PAINT` only draws the already-cached texture. Additional repaint events cannot consume entropy or advance canonical state.

The session exposes a monotonic render-generation counter used by tests to prove that fit/zoom/pan/before-after events do not execute the canonical pipeline.

## Deterministic proxy preview

FM-004 introduces proxy method version `1`:

- nearest-neighbour sampling;
- default maximum bounds `1280 x 960`;
- aspect ratio preserved with integer arithmetic;
- no genome changes;
- no hidden parameter compensation;
- source identity remains the identity of the full normalized source.

The cache key is SHA-256 of the UTF-8 text:

```text
FAULTMINE-PROXY-v1:<source-identity>:<max-width>:<max-height>:<method-version>:nearest
```

A source already within the bounds is returned unchanged and marked non-proxy. Oversized sources are marked proxy explicitly.

The interactive application enables proxy preview by default. The status line displays `PROXY` whenever the displayed source/result is based on proxy pixels and `FULL` otherwise. `P` toggles proxy preview.

Proxy pixels are a performance/presentation artefact. They are not a replacement source and do not change the saved genome.

## Full-resolution export boundary

Export always executes the canonical CPU pipeline against the retained full-resolution normalized source, regardless of interactive proxy state.

The GUI then passes that full canonical result to the existing WIC PNG adapter. A proxy preview can therefore never be silently exported as the final artwork.

## D3D11 presentation

`faultmine_d3d11` owns:

- D3D11 device/context;
- DXGI swap chain;
- render target;
- one RGBA8 texture/SRV copied from CPU pixels;
- presentation-only vertex/pixel shaders;
- sampler/blend/constant-buffer state.

Creation tries a hardware D3D11 device first and falls back to WARP. The shaders only place and sample the already-rendered texture; they do not implement fault semantics.

The renderer retains a CPU copy of the currently uploaded presentation image. If `Present` or resize reports `DXGI_ERROR_DEVICE_REMOVED` / `DXGI_ERROR_DEVICE_RESET`, the renderer recreates the D3D resources and reuploads the same presentation pixels without changing session/genome state.

## Canvas view semantics

- `F` / View -> Fit: fit image inside client area and centre it.
- `1` / View -> 1:1: one source/result pixel per display pixel before OS/DPI effects outside this model.
- mouse wheel or View zoom commands: custom zoom relative to fit.
- left-button drag or arrow keys: presentation pan only.
- `B`: toggle before/after.

Before/after shows the current preview source versus the result computed from that same preview source. Thus when proxy mode is active both sides are explicitly proxy-resolution; when full mode is active both are full-resolution.

View changes do not dirty semantic rendering.

## Starter interactive controls

FM-004 intentionally does not build the generic editor planned for FM-008. It exposes a small programmatic starter stack and compact controls sufficient to exercise real canonical semantics without editing JSON:

- `Space`: enable/disable the starter fault stack;
- `[` / `]`: row-offset -1 / +1;
- `-` / `=`: scanline-jitter magnitude -1 / +1;
- `R`: deterministically reroll the explicit root seed.

The initial starter stack uses:

1. row offset (`wrap`);
2. `bgra` channel permutation;
3. byte XOR on red/blue;
4. named-stream scanline jitter (`wrap`).

These are existing FM-003 operator semantics, not UI-specific reimplementations.

## Native file workflow

- `Ctrl+O` opens a WIC-supported still through the FM-003 normalizer.
- cancelled dialogs make no state change.
- failed WIC/core operations display an actionable error and retain the previous valid session.
- source switching clears old preview/proxy caches before the new semantic render.
- `Ctrl+E` exports a full-resolution canonical PNG through the FM-003 WIC path.

The status surface reports source/preview dimensions, `FULL` versus `PROXY`, `BEFORE` versus `RESULT`, starter-fault state, row/jitter values, root-seed prefix and hardware versus WARP D3D11 presentation.

## CI / smoke contract

The existing `FAULTMINE.exe --smoke-test` is strengthened in FM-004. It now creates the real Win32/D3D11 shell, builds a synthetic canonical source, renders the starter CPU pipeline, uploads the result as a D3D11 texture, presents one frame using hardware or WARP and then closes non-interactively.

Core/session tests remain independently runnable without D3D11. The D3D smoke supplements rather than replaces pure model tests.

## Compatibility rule

FM-004 adds no new canonical fault semantics and must not change the accepted FM-002 entropy/genome vectors or FM-003 visual golden hashes.

A later change that makes a proxy alter genome semantics, makes paint/display timing advance canonical state, or makes GPU output authoritative would be an architecture change requiring explicit documentation and equivalence/provenance decisions.
