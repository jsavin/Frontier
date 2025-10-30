# YAML Parsing Options (Decision Log Snippet)

We have implemented the “vendor libyaml” option below; the bison/flex pipeline
is deferred to the future worklist.

## Option: Vendor libyaml (C reference parser)
- **Status:** Implemented (libyaml 0.2.5 under `third_party/libyaml/`).
- **Pros:**
  1. Robust parser with full YAML 1.1 support (handles anchors, folded scalars, tagging, etc.).
  2. Mature code base used in production across many projects.
  3. MIT-licensed and compatible with Frontier’s licensing.
  4. Reduces long-term maintenance risk and shrinks the testing surface for YAML quirks.
- **Cons:**
  1. Adds third-party source to the tree; we must track upstream security fixes.
  2. Slightly larger build footprint; compiler must link the bundled sources.
  3. Custom wrapper still required to translate libyaml events into Frontier’s data structures.
- **Integration notes:**
  - Sources live under `third_party/libyaml` with a local README noting the upstream tag.
  - `tools/strings_compiler` builds the libyaml objects directly and links them into the generator.
  - `strings_yaml_loader.c` adapts libyaml events to the `strings_document` builder.
- **Follow-up:** Evaluate replacing the bundled copy with the enhanced bison/flex parser once parity tests confirm equivalent behaviour (tracked in `planning/TODO_future_improvements.md`).
