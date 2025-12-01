# v6 Root Fixture Plan (Legacy App Authored)

## Status
- State: Planning
- Last Updated: 2025-11-30
- Owner: Codex
- Goal: Define a canonical v6 `test.root` that exercises every datatype we need to round-trip into v7 for migration/serializer tests.

## Constraints
- Authored with the legacy Windows Frontier app (v6-era) to ensure genuine v6 on-disk layout and metadata.
- No hand-editing of the file; all content created via UI/verbs to keep headers/variance/avail lists authentic.
- Keep the root compact (single file) but include one well-formed instance of each datatype; avoid huge payloads.

## Target Structure (entries under root table)
- **Scalars**
  - `int32_small`: 1234
  - `int32_negative`: -4321
  - `int64_like_string`: literal `"0x1_0000_0000"` to catch widening.
  - `boolean_true`: true
  - `boolean_false`: false
  - `date_epoch`: 1970-01-01T00:00:00Z
  - `date_far_future`: 2099-12-31T23:59:59Z
  - `string_ascii`: `"Hello, Frontier!"`
  - `string_extended`: includes high-bit characters (e.g., `café`) to verify encoding.
  - `binary_small`: 16-byte blob (0x00..0x0F) to verify binaryType.

- **Tables / Records**
  - `table_mixed`: keys `a:int`, `b:string`, `c:bool`, `d:date`, `e:binary` (small blob), `f:subtable`.
    - `f:subtable` with `x:int`, `y:string`.
  - `record_simple`: legacy record with fields `name`, `version`, `flags` (small ints/strings).

- **Lists / Arrays**
  - `list_strings`: `["alpha", "beta", "gamma"]`
  - `list_mixed`: `[1, "two", true, date(2000-01-01)]`

- **Outlines**
  - `outline_basic`: root headline `"root"` with two children `"child1"`, `"child2"`, and a nested grandchild under child2 `"grandchild"`. Include one headline with a note/body text to verify outline text storage.

- **WPText**
  - `wptext_basic`: short paragraph with bold/italic span and a newline to confirm formatting survives.

- **Menus**
  - `menu_sample`: menu `"Sample"` with items:
    1. `"Item One"` (enabled)
    2. `"Item Two"` (disabled)
    3. Separator
    4. Submenu `"Sub"` with `"Sub Item"`

- **PICT**
  - `pict_small`: small drawn rectangle (any simple PICT) to exercise the legacy PICT external type.

- **Scripts**
  - `script_hello`: simple script `dialog.alert(\"hello\")` to confirm script storage and external refs.
  - `script_dbops`: script that reads/writes `table_mixed` to ensure serialized references resolve.

- **WPText/Card-like**
  - `card_note`: single-card equivalent (if supported) with title `"Card Note"` and body `"card body"` to cover CARD if encountered.

- **Views**
  - View0 should reference the root table.
  - No additional views unless the app forces them; if present, document addresses.

## Capture Notes
- After creation, record:
  - File size, view addresses, and root table address.
  - For each external (outline/wptext/menu/pict/script), note its database address if easily visible.
  - Any UI-side quirks (e.g., default values added by the app).
- Avoid Save As/compaction after creation; deliver the raw v6 file.

## Follow-ups (post-file delivery)
- Add automated regression that migrates this fixture to v7 and asserts:
  - All entries exist with matching logical content (scalars, lists, tables).
  - Externals (outline/wptext/menu/pict/script/binary) deserialize without free-block hits and reserialize in BE64.
  - No leftover legacy views/headers; headerLength = 88, version = 7 after migration.

## ASCII Layout (root table)

```
root (table)
├─ int32_small = 1234
├─ int32_negative = -4321
├─ int64_like_string = "0x1_0000_0000"
├─ boolean_true = true
├─ boolean_false = false
├─ date_epoch = 1970-01-01T00:00:00Z
├─ date_far_future = 2099-12-31T23:59:59Z
├─ string_ascii = "Hello, Frontier!"
├─ string_extended = "café …" (high-bit chars)
├─ binary_small = 16-byte blob 0x00..0x0F
├─ table_mixed (table)
│  ├─ a = 1
│  ├─ b = "two"
│  ├─ c = true
│  ├─ d = date(2000-01-01)
│  ├─ e = binary (small)
│  └─ f (table)
│     ├─ x = 10
│     └─ y = "sub"
├─ record_simple (record; name→value pairs)
│  ├─ name = "rec"
│  ├─ version = 1
│  └─ flags = 1 (small int)
├─ list_strings = ["alpha", "beta", "gamma"]
├─ list_mixed = [1, "two", true, date(2000-01-01)]
├─ outline_basic (outline)
│  ├─ root
│  │  ├─ child1
│  │  └─ child2
│  │     └─ grandchild (with note/body)
├─ wptext_basic (wp text with bold/italic span + newline)
├─ menu_sample (menu)
│  ├─ Item One (enabled)
│  ├─ Item Two (disabled)
│  ├─ Separator
│  └─ Sub → [Sub Item]
├─ pict_small (PICT graphic)
├─ script_hello = dialog.alert("hello")
├─ script_dbops = script that reads/writes table_mixed
└─ card_note (card-equivalent) title/body

view0 → root table
```
