# WPText Refactor Notes

## Historical constraints

- **32 KB size limit:** WPText objects grew out of the Classic Mac OS word processing widgetry, which carried a hard 32 KB ceiling due to Toolbox data structures that mirrored Pascal string limits. The runtime still assumes that ceiling.
- **Platform character sets:** The code path remains rooted in the old Mac/Windows encodings rather than a Unicode-aware text model. Rich text authored in the IDE therefore depends on the legacy character maps.

## Future work reminders

- Remove the 32 KB ceiling during the WPText refactor so modern documents are not silently truncated.
- Transition WPText storage and editing to UTF-8/Unicode, retiring the historical per-platform encodings.
- Revisit how the word-processor window is surfaced in headless builds vs. UI shells, ensuring the runtime’s rich-text data operations remain usable without a visible window.
