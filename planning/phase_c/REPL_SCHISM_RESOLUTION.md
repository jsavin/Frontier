# Proposal: Resolving the Legacy/Boxen REPL Schism

**Date**: 2026-06-10
**Author**: Claude (with JES collaboration)
**Status**: APPROVED 2026-06-10 -- Path A selected
**Supersedes**: nothing yet; complements `planning/phase_c/PROPOSAL_REVISED.md`

## 1. The Problem

Two REPLs coexist today:

| Aspect | Legacy (`repl.c` + linenoise) | Boxen (`boxen_repl.c`) |
|---|---|---|
| Activation | Default `frontier-cli` (no flag) | `--debug-tui` opt-in |
| LOC | ~4,235 | ~1,011 |
| Input editing | linenoise (full readline) | Single-line, printable ASCII only |
| History | `.frontier_history` persistent | In-memory only |
| Tab completion | Yes (ODB paths, slash commands) | None |
| Slash-menu palette | Yes (termbox2-based overlay) | None |
| Async output | Yes (linenoiseHide/Show) | Output goes to scrollback batched |
| Multiple windows | No (terminal-overlay palette only) | Yes (outline editor on top) |
| Stdout/stderr | Goes to terminal | Captured via pipe + dup2 into scrollback |
| Threading | Same GIL discipline | Same GIL discipline |
| `--debug-tui @path` launch | N/A (linenoise can't open TUI) | Works (PR #756 -> PR #758) |

The boxen REPL exists because:
1. The standalone debugger TUI (B.0-B.8) needed a way for users to launch debugging sessions without leaving the linenoise REPL
2. The C.1 outline editor needs multi-window coordination that linenoise can't provide
3. Phase C planning (`PROPOSAL_REVISED.md`) targets the boxen REPL as the long-term home for editor windows and the debug split

The legacy REPL exists because:
1. It works today for daily use
2. 102 linenoise-touching lines + 4,235 total lines of integration with all of Frontier's slash commands, palette, completion, eval, etc.
3. Users (JES) actually use it

The schism is a problem because:
1. **You can't use the editor's full value without switching to a less-capable REPL.** `/edit @path` only works in `--debug-tui`, but `--debug-tui` lacks history and tab completion. So debugging a script feels like a regression in basic ergonomics.
2. **Two implementations of "the REPL" diverge over time.** Slash commands added to legacy don't appear in boxen, and vice versa.
3. **The PROPOSAL eventually targets boxen-as-default.** That milestone (C.6) keeps slipping if boxen isn't featureful enough.

## 2. The Three Possible Resolutions

### Path A: Bring boxen REPL to feature parity with linenoise, then deprecate linenoise

Move history, tab completion, slash-menu palette, async output into boxen REPL. Once parity is reached, flip the default; eventually delete linenoise REPL.

**Pros:**
- Single end-state
- Boxen architecture (multi-window, captured stdout, structural draw model) is more flexible
- Aligns with PROPOSAL_REVISED.md C.6

**Cons:**
- Large body of work spread across several PRs
- Risk of subtle behavior drift during migration
- During the migration window, daily-use users still need linenoise

### Path B: Hybrid mode -- share underlying logic between the two REPLs

Extract the input-handling logic (history, completion, slash dispatch) into a shared layer that both REPLs can use. Linenoise stays as the terminal renderer in default mode; boxen takes over rendering in `--debug-tui` mode. Both consume the same "REPL core" library.

**Pros:**
- Both REPLs benefit from new features immediately
- No deprecation pressure on linenoise
- Smaller PRs

**Cons:**
- The "input layer" abstraction is the hard part -- linenoise wants raw mode + escape sequences; boxen wants codepoint events
- Some features (palette overlay) are fundamentally different in the two
- Two REPLs continue to exist; the schism doesn't actually close, it just gets a shared core
- More complex than it sounds because the two input models are genuinely incompatible

### Path C: Embed boxen's editor windows into the legacy REPL on demand

Keep linenoise as the REPL. When the user types `/edit @path`, briefly suspend linenoise, enter boxen mode (terminal goes raw, full screen), let them interact with the editor, then exit boxen and resume linenoise. The editor is a transient overlay, not a co-resident surface.

**Pros:**
- Linenoise keeps all its features
- Editor lives in its own modal context (much like `vim` does from a shell)
- Lower invasion of the existing code
- No `--debug-tui` flag needed long-term -- `/edit` just works from the default REPL

**Cons:**
- "Multiple editor windows" only makes sense within a session, not across edit-and-resume cycles
- Switching between REPL and editor is a mode change, not free coexistence
- Loses the "REPL and editor visible at once" model that PROPOSAL_REVISED.md targeted
- The debugger split (Phase C.2) doesn't fit this model well -- debugging interleaves with REPL use

## 3. Trade-off Matrix

| Dimension | A: parity then flip | B: shared core | C: embed-on-demand |
|---|---|---|---|
| Effort estimate | 4-6 PRs over weeks | 6-10 PRs (abstraction work) | 2-3 PRs |
| User experience continuity | Best long-term | Best short-term | Best immediately |
| End-state architecture | Clean (one REPL) | Two REPLs sharing core | One REPL + transient editor |
| Multi-window vision (debug split, multiple editors) | Native | Requires more work in B | Awkward fit |
| Risk to existing linenoise users | High during migration | Medium | Low |
| Phase C alignment | Matches PROPOSAL | Diverges (keeps linenoise indefinitely) | Diverges significantly |
| "Daily use Cmd-S editor while running code" | Works in C.6 | Works after parity | Doesn't work -- mode switch required |

## 4. Recommendation: Path A, sequenced incrementally

Move toward boxen-as-default. Stage it so daily-use users keep their linenoise REPL through most of the work, then flip the default in a single milestone once boxen has parity.

**Why A:**
- PROPOSAL_REVISED.md already targets boxen-as-default as C.6. The schism is the symptom of having started Phase C without acknowledging the C.0.x parity work needed first.
- Path B's shared-core abstraction is conceptually appealing but the input models genuinely don't share well. Months of work for limited end-state benefit.
- Path C's "vim-like modal editor" pattern is fine for tools that don't need to coexist with the REPL. But the debugger work (Phase C.2 debug split) requires the editor and REPL to be visible at the same time, which kills C.
- The cost is sequencing: each C.0.x milestone needs to be small and shippable.

**Why not B (shared core):**
- linenoise's input model is "give me a string when Enter pressed; I handle escape sequences." Boxen's is "give me events as they happen." Sharing requires either (a) abstracting under linenoise (loses boxen's event flexibility) or (b) reimplementing linenoise's input on top of boxen events (defeats the purpose). The shared core would be slash-dispatch + completion logic only, which is already fairly cleanly separated.

**Why not C (embed-on-demand):**
- The Phase C vision (and the work already done) is for editor + REPL coexistence. Moving to "open editor -> leave REPL -> close editor -> return to REPL" is a step backward from what's shipped.
- Doesn't solve the schism -- it just makes the schism explicit instead of implicit.

## 5. Sequencing Plan (revised C.0.x milestones)

Each milestone is one PR. After C.0.6 ships, C.6 (flip the default) is a 30-line change. Estimated 4-6 weeks of incremental work; users keep linenoise default throughout.

### C.0.1 -- Persistent history with linenoise file format
- Read `~/.frontier_history` on `boxen_repl_main` entry, write on clean exit
- Up/down arrow walk history; Ctrl-R inverse search (minimal version)
- Linenoise file format preserved so users don't lose history when they switch
- ~200 LOC, 2 new tests
- Risk: low -- well-understood pattern

### C.0.2 -- Tab completion for slash commands and ODB paths
- Reuse `complete_slash_command_path` and `repl_resolve_path_ex` from repl.c (already independent of input layer per OVERVIEW Q4)
- Wire Tab key in boxen input bar to invoke completion
- Render completion candidates in a transient floating window
- ~300 LOC, 4 new tests
- Risk: medium -- completion candidate rendering UX needs care

### C.0.3 -- Slash-menu palette as boxen modal
- Migrate the palette implementation from raw termbox2 to a boxen modal window
- Same `/`-key trigger and 250ms delay
- Same UserTalk-handler invocation contract (palette_arg_inject + dispatch)
- ~400 LOC + ~150 LOC reused from palette.c
- Risk: medium-high -- palette has complex state (arg injection, slot-key shortcuts)

### C.0.4 -- Async output during in-progress input
- When a thread writes to stdout while user is typing, the boxen output pane scrolls but the input bar isn't disturbed
- C.0 already captures stdout via pipe; this milestone makes the drain happen smoothly during input
- ~100 LOC
- Risk: low -- already mostly working

### C.0.5 -- Multi-line input
- The B.7 scratch-eval pane was single-line; that survived into C.0
- Add `\`-continuation or explicit multi-line mode for inline scripts
- ~150 LOC, 2 new tests
- Risk: low

### C.0.6 -- Parity verification + documentation
- Side-by-side feature checklist against linenoise REPL
- Document any intentional behavioral differences (there will be a few)
- Update `docs/CLI_USAGE_GUIDE.md` to reflect boxen REPL as default-eligible
- Risk: low

### C.6 -- Flip default; deprecate `--debug-tui`
- Default `frontier-cli` -> boxen REPL
- Add `--plain` flag to fall back to linenoise (CI use, pipe mode per OVERVIEW Q1)
- Remove `--debug-tui` flag (becomes a no-op alias)
- Update tests, docs, examples
- After 1 release: delete linenoise REPL entirely
- Risk: medium -- user-visible mode change

## 6. The Cost of Doing Nothing

If the schism persists, we'll end up with:
- C.1.1 (text-cursor editing) in boxen REPL, daily users can't use it because they're in linenoise
- C.1.2 (full structural editing) same problem
- C.2 (debug split) only accessible via `--debug-tui`, which is missing history/completion
- Each new editor or debugger feature ships into the less-usable REPL
- Eventually the two diverge irreparably or we ship a half-functional product

So the cost of the proposal is real but the cost of not doing it is worse.

## 7. Open Questions

1. **Should C.0.x interleave with C.1.x/C.2?** Or should we pause new feature work until parity ships? Recommendation: sequential -- finish C.0.x first, then resume C.1.1+.

2. **Linenoise tests during the migration.** Coverage that linenoise still works while we work on boxen is probably fine -- we're not touching repl.c.

3. **History format compatibility.** Linenoise's history file format is simple. Verify that no behavior we add in boxen requires extending the format incompatibly.

4. **Palette migration risk.** The palette is complex (palette_arg_inject, slot-key shortcuts, ODB navigation). C.0.3 could expand into multiple PRs if needed.

5. **Backward compat for users scripting `--debug-tui`.** Anyone using `--debug-tui` from a shell script will see behavior change in C.6. Probably acceptable since `--debug-tui` is days old, but worth flagging.

## 8. Approved Direction

**Path A**, sequenced via C.0.1 -> C.0.6 -> C.6, approved by JES 2026-06-10.

Detailed execution plan: see `REPL_SCHISM_EXECUTION_PLAN.md`.
