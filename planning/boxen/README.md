# planning/boxen/

Planning materials for **boxen** — a portable window manager library for terminal applications, designed to be Frontier's TUI substrate and (eventually) extracted as a standalone open-source library.

## What boxen is

A library that sits on top of [termbox2](https://github.com/termbox/termbox2) and provides the abstractions termbox2 itself does not: windows, z-order, move, resize, scrolling content, focus, and a small set of common chrome (borders, titles, status bars). The first backend is termbox2; the backend interface is designed to be swappable so that boxen's public API can outlast any specific terminal-handling library.

Tagline: *"boxen — a portable window manager for terminal applications. Backed by termbox2."*

## What this directory holds

| File | Purpose |
|------|---------|
| `README.md` | This file — orientation for teammates and future-Jake. |
| `OVERVIEW.md` | Plan: motivation, the Frontier TUI surfaces boxen serves, architecture sketch, API shape, extraction roadmap. |
| (later) `EXECUTION_PLAN.md` | Detailed sequencing produced by the system-architect after OVERVIEW is reviewed. |
| (later) `API_DRAFT.h` | First-cut public header, written once the execution plan is approved. |

## Status

- **2026-06-06**: Direction approved. Frontier picks termbox2 over notcurses for Windows-support and dependency-chain reasons. Decision: build boxen on termbox2 with a swappable backend interface, exercise it inside Frontier through at least two surfaces (debugger TUI, then outline editor), extract as a standalone library at v1.0.
- Pre-requisite spike: cmd-2-click identifier resolution feasibility (issue #691 / task #135) — verdict GREEN, runtime side is ready.

## How this fits into the broader plan

1. boxen is built inside Frontier first, in `frontier-cli/boxen/` (working location, subject to change).
2. The debugger TUI (#691) is boxen's first consumer.
3. The outline editor is boxen's second consumer — required before extraction so the API has been pressure-tested by two surfaces.
4. Extraction to its own repository happens at boxen v1.0, with the public API stable.
5. Frontier then consumes boxen as a vendored git submodule.

## Pointers

- Frontier's planning index: `planning/INDEX.md`
- Frontier's TUI debugger context: issue #691, task #135
- Discussion that led here: session 2026-06-05/06 (UserTalk debugger TUI direction, termbox2 vs notcurses, splitting boxen as standalone)
