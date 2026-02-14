# Phase 7 — Polyglot Scripting

Status
- State: Draft
- Phase: Phase 7
- Last Updated: 2026-02-13
- Notes: Specification only. No code changes.

## Overview

Phase 7 explores adding JavaScript and Python as first-class scripting languages in Frontier, alongside compiled extensions (Go, Rust) via shared libraries. These documents are specifications — no implementation work has begun.

## Documents

| Document | Description |
|----------|-------------|
| [00 — Vision & Architecture](00-polyglot-vision.md) | Why multi-language matters, prior art (Godot, Neovim, Blender, Redis, PostgreSQL), two architectural approaches, value type bridge, cross-language calling, threading, and roadmap |
| [01 — Language Interface Spec](01-language-interface-spec.md) | `FrontierLanguage` C vtable, language registry, value bridge API, error handling, GIL protocol, ODB access API, script lifecycle |
| [02 — JavaScript Integration](02-javascript-integration.md) | Engine comparison (QuickJS, JSC, V8, Duktape), type mapping, `frontier` global object API, ES modules mapped to ODB, async considerations, POC scope |
| [03 — Python Integration](03-python-integration.md) | CPython vs MicroPython, the double-GIL problem (4 solutions analyzed), `frontier` module API, memory management at boundary, pip ecosystem, POC scope |
| [04 — Compiled Extensions](04-compiled-extensions.md) | Why Go/Rust are extensions not scripting engines, shared library plugin ABI, Rust and Go examples, plugin lifecycle, comparison with existing EFP pattern |
| [05 — Package Ecosystem Integration](05-package-ecosystem-integration.md) | Filesystem mounts bridging ODB to npm/pip, `#packages` convention, two-tier model (ODB-managed vs external), auto-detection, manifest strategies, future Frontier package system |

## Key Design Decisions

- **Licensing**: Frontier is currently GPLv2 (MIT migration underway) — all engines must be permissively licensed
- **Architecture**: Two approaches spec'd (vtable vs RPC hybrid); decision deferred to implementation
- **ODB access**: Full read/write from all languages (same trust model as UserTalk)
- **Script storage**: Both ODB-native and filesystem
- **Cross-language calling**: Bidirectional via kernel projection (hub-and-spoke, not direct)
- **Threading**: Shared GIL first, per-engine GIL as evolution
- **JS engine**: Deferred to POC phase
- **Python GIL**: Ordered acquisition (Frontier GIL first, then CPython GIL)
- **Packages**: ODB declares dependencies; filesystem stores installed packages; filesystem mounts bridge the two
- **Filesystem mounts**: New ODB table type that transparently maps to a directory on disk (e.g., `node_modules/`, `site-packages/`)
- **`#packages` convention**: Language-agnostic table hierarchy (`#packages.npm`, `#packages.pip`, `#packages.frontier`) for dependency management
- **Two-tier model**: Tier 1 (ODB-managed, Frontier runs npm/pip) and Tier 2 (auto-detect existing environments)

## Prerequisites

- Phase 4: Threading model (GIL, thread safety) must be stable
- Phase 5: UTF-8 awareness for clean string bridging (full transition not required)

## Related Docs

- `planning/phase4/threading/README.md` — GIL model (ADR-014)
- `Common/headers/lang.h` — Value type system
- `Common/source/langpython.c` — Legacy Python 1.6 experiment
- `Common/headers/osacomponent.h` — Legacy AppleScript/OSA integration
