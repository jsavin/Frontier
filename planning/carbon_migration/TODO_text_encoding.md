# Text Encoding Shim Notes
**Date:** October 31, 2025  
**Owner:** Codex  

Why this matters beyond header cleanup:
- Consolidates classic TEC constants (`kTextEncoding*`, `kTEC*Err`, etc.) into a single portable shim so headless vs desktop builds never diverge.
- Provides explicit stub behaviour for headless (return `kTextUnsupportedEncodingErr` instead of silently succeeding), preventing silent data loss when conversions are unavailable.
- Creates a future swap point: once we introduce a real converter (`iconv` or similar), only the shim needs to change; shared code already routes through it.

Talking points for commit/PR summary later:
- "Introduce portable TEC shim so headless builds fail fast when conversions aren’t available; drop duplicate macros from scattered headers." 
- "Centralise text encoding typedefs, enabling a future drop-in replacement without touching headless code." 
- "Documented headless behaviour (no conversion) and ensured desktop builds still take the Carbon path." 

Implementation checklist (next steps):
1. Create `portable/text_encoding_portable.{h,c}` with constants, typedefs, and stub functions.
2. Include the new shim from `osincludes_portable.h` and strip duplicate definitions from `headless_stubs.h`/elsewhere.
3. Update `strings.c` (and any other TEC callers) to check the return value and handle headless failure explicitly.
4. Rebuild key tests (`make -C tools/strings_compiler`, `make -C tests handle_tests`, `make -C tests strings_generated`).
5. Fold this note into the eventual commit/PR description and clean up the doc once work is complete.
