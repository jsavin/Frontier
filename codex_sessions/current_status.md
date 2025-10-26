# Frontier Database Format Investigation - Current Status

**Last Updated**: October 26, 2025 (Headless System work queue)
**Branch**: feature/headless-system-bootstrap

## Updated Plan — October 25, 2025

1. **Scanner & Docs (done)**: Documented that the 442-byte block at `views[0]` is a serialized Cancoon/About record and taught the scanner to follow its `adrroottable`. Helper script `scripts/dump_tables.py` enumerates actual modern blocks for debugging.
2. **Next Work Items**:
   - Re‑audit the v6→v7 migration path now that we can reliably read the v6 roots. Verify block copying and format conversion are lossless.
   - Resume headless runtime work: load the v7 Frontier.root, bring the system table online, and ensure glue scripts can call through to our kernel implementations.
   - Preserve Cancoon state for future UI re‑hosting (tracked in `planning/TODO_future_improvements.md`).

## Longer-Term Goals
- Lock in the v6→v7 migration (Phase 1/Phase 2 deliverables) with automated verification once the parser is stable.
- Complete headless runtime parity so server deployments can run without GUI dependencies while still exposing necessary UI state (About/Cancoon, msg log, agent status) through new channels.
- Phase 3+ work: database hash-table modernization, headless CLI improvements, and eventual UI layer re-host.
