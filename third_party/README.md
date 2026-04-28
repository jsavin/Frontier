# Third-Party Dependencies

| Library | Upstream | License | Pinned Version | Purpose | Integration Notes |
| --- | --- | --- | --- | --- | --- |
| libyaml | https://github.com/yaml/libyaml | MIT | 0.2.5 release | YAML parser used by `tools/strings_compiler` to load STR# replacements | Vendored source under `third_party/libyaml`; linked directly by the strings compiler; no local patches beyond build glue. |
| cJSON | https://github.com/DaveGamble/cJSON | MIT | upstream snapshot | JSON parser linked by the CLI | Vendored source under `third_party/cJSON`. |
| linenoise | https://github.com/antirez/linenoise | BSD-2-Clause | upstream snapshot | Line-editing for the CLI REPL | Vendored source under `third_party/linenoise`. |
| pexpect | https://github.com/pexpect/pexpect | ISC | 4.9.0 | PTY-based interactive dialog verb testing | Vendored pure-Python source under `tests/vendor/pexpect`; used by the integration test runner to drive interactive dialog prompts via expect/send pairs. Test-only dependency — not linked into the CLI binary. |
| ptyprocess | https://github.com/pexpect/ptyprocess | ISC | 0.7.0 | PTY process management (pexpect dependency) | Vendored pure-Python source under `tests/vendor/ptyprocess`; required by pexpect for spawning and managing PTY subprocesses. Test-only dependency. |

Add new entries here whenever we vendor or patch third-party code so future upgrades stay tractable.

## Update Procedures

### libyaml
1. Visit the upstream repo and choose the desired release/tag.
2. Replace `third_party/libyaml` with a fresh archive/clone of that release (preserving our build glue in `tools/strings_compiler`).
3. Regenerate the strings compiler (`make -C tools/strings_compiler`) and rerun `make strings_generated` to ensure the new version works.
4. Update the pinned version in the table above.
