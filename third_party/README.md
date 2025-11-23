# Third-Party Dependencies

| Library | Upstream | License | Pinned Version | Purpose | Integration Notes |
| --- | --- | --- | --- | --- | --- |
| libyaml | https://github.com/yaml/libyaml | MIT | 0.2.5 release | YAML parser used by `tools/strings_compiler` to load STR# replacements | Vendored source under `third_party/libyaml`; linked directly by the strings compiler; no local patches beyond build glue. |
| Paige (HERMES fork) | https://github.com/nmatavka/HERMES-Paige | GPL-2.0 | commit `a2fe9b1` (2024-01-19) | Rich text engine powering WPText serialization/migration | Snapshot checked into `third_party/Paige` (no longer a submodule) with our headless platform file `PGPLATFO/PGUNX.C`. `CMakeLists.txt` is patched to append that file, define `UNIX_COMPILE`/`C_LIBRARY`/`NO_OS_INLINE`, and treat `PGPLATFO/PGIO.C`, `PGSCRAP.C`, and `PGOSUTL.C` as C sources (these files were originally built as C++ due to the `.C` extension, which produced mangled exports). Those three files also carry small signature fixes so `pgScrapMemoryWrite`, `pgStandardRead/Write`, `pgOSRead/Write`, and `pgUnicodeToBytes` match their headers (`size_t` parameters instead of `long`). Headless runtime links `libpaige.a` built from this tree. |
| CMake | https://cmake.org | BSD-3-Clause | 3.29.6 | Builds Paige (system image lacks cmake) | Built from source into `third_party/cmake-install/`; Paige Makefiles call `third_party/cmake-install/bin/cmake`. |

Add new entries here whenever we vendor or patch third-party code so future upgrades stay tractable.

## Update Procedures

### libyaml
1. Visit the upstream repo and choose the desired release/tag.
2. Replace `third_party/libyaml` with a fresh archive/clone of that release (preserving our build glue in `tools/strings_compiler`).
3. Regenerate the strings compiler (`make -C tools/strings_compiler`) and rerun `make strings_generated` to ensure the new version works.
4. Update the pinned version in the table above.

### Paige (HERMES fork)
1. Remove `third_party/Paige`, clone the upstream repo, check out the target commit, and delete its `.git` directory.
2. Copy our headless-specific files back in (`PGPLATFO/PGUNX.C`, CMake non-Windows additions, any local docs) and reapply merges if upstream touched those areas.
3. Rebuild the archive using the vendored cmake:  
   `third_party/cmake-install/bin/cmake -S third_party/Paige -B third_party/Paige/build-headless -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`  
   `third_party/cmake-install/bin/cmake --build third_party/Paige/build-headless`
4. Run `make -C tests runtime_tests` (or the CLI build) to confirm the new `libpaige.a` links cleanly.
5. Update the commit hash in the table above and note any additional integration steps.

### CMake
1. Download the desired CMake source tarball from https://cmake.org/download/.
2. Extract into `third_party/cmake-src`, build/install into `third_party/cmake-install` (see previous bootstrap command history for flags).
3. Verify `third_party/cmake-install/bin/cmake --version` reports the new version.
4. Update this README with the new version number.
