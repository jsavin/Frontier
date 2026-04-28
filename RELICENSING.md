# Relicensing Notice: GPLv2 → MIT

This project was originally released under the GNU General Public License,
version 2 (GPLv2). Effective with this commit, the project is relicensed
under the MIT License (see `LICENSE`).

## Authorization

The original copyright holder of UserLand Frontier(tm), UserLand Software,
Inc., became defunct more than two decades ago. The remaining sole owner of
the company at the time it was wound down authorized the current maintainer
to use any license of their choice for this codebase. There are no remaining
investors, partners, or other interested parties with rights in the code.
Authorization for the relicensing is on file with the maintainer.

## Scope

The relicensing applies to:

- All first-party source code authored under the UserLand Frontier project
  (C, C++, Objective-C, headers, Rez, and Win32 resource files in
  `Common/`, `frontier-cli/`, `portable/`, `tests/`, `tools/`, and the
  related build inputs).
- The license declaration in version-info strings emitted by the Windows
  build.
- The top-level `LICENSE` file and historical `docs/LICENSE.txt`.

## What was removed

- The bundled MySQL client (`Common/source/langmysql.c`,
  `Common/headers/langmysql.h`, `scripts/build_mysql_client.sh`,
  `docs/mysql_client_setup.md`) was removed because the upstream MySQL
  client library is GPL-licensed and incompatible with the new MIT terms.
  The `mysql.*` verbs remain registered as headless stubs that return
  a script error indicating the feature is not implemented on this
  platform; existing UserTalk scripts that reference these verbs will
  receive a clean error rather than a "verb not found" failure.
- `LICENSES/GPL-2.0.txt` was removed because no remaining bundled component
  requires GPLv2.

## Third-party components

All other third-party components bundled with this project use
MIT-compatible licenses (Apple Sample Code, PCRE, RSA-MD5, SSLeay/OpenSSL).
See `LICENSES/README.md` for details and the corresponding license texts.

## File-level markers

Every relicensed source file carries an `SPDX-License-Identifier: MIT`
marker at the top, alongside the standard MIT permission notice. New code
contributed to this project should follow the same pattern.
