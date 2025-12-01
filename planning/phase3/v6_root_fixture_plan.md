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
  - `real_number`: 3.14159
  - `nil_value`: nil
  - `date_epoch`: 1970-01-01 00:00:00 (local date/time; no TZ)
  - `date_far_future`: 2099-12-31 23:59:59 (local date/time; no TZ)
  - `string_ascii`: `"Hello, Frontier!"`
  - `string_extended`: includes high-bit characters (e.g., `café`) to verify encoding.
  - `binary_small`: 16-byte blob (0x00..0x0F) to verify binaryType.
  - `addressValueSystem`: live odb address into `Frontier.root` (system DB).
  - `addressValueGuest`: live odb address inside this file (`test.root` guest DB, e.g., `binarySmall`).

- **Tables / Records**
  - `table_mixed`: keys `a:int`, `b:string`, `c:bool`, `d:date`, `e:binary` (small blob), `f:subtable`.
    - `f:subtable` with `x:int`, `y:string`.
  - `record_simple`: legacy record with fields `name`, `flags`, `tag`, `subtable` (non-scalar), and `empty_slot` (nil).
  - `record_empty`: empty record.

- **Lists / Arrays**
  - `list_strings`: `["alpha", "beta", "gamma"]`
  - `list_mixed`: `[1, "two", true, date(2000-01-01)]`
  - `list_with_record`: `[ { recname = "r1", recval = 42 }, "tail" ]`
  - `list_with_table`: `[ { k = "v" }, table_mixed ]`
  - `list_empty`: `[]`

- **Outlines**
  - `outline_basic`: root headline `"root"` with refcon, children `child1`, `child2` (refcons), and a nested `grandchild` (refcon) with note/body to verify outline text/refcon storage.

- **WPText**
  - `wptext_basic`: short paragraph with bold/italic span and a newline to confirm formatting survives.
  - `wptext_stylesheet`: multiple styled runs and a blank line.

- **Menus**
  - `menu_sample`: menu `"Sample"` with items:
    1. `"Item One"` (enabled)
    2. `"Item Two"` (disabled)
    3. Separator
    4. Submenu `"Sub"` with `"Sub Item"`

- **PICT**
  - `pict_small`: small drawn rectangle (any simple PICT) to exercise the legacy PICT external type.

- **Scripts**
  - `scriptHello`: simple script `dialog.alert(\"hello\")` to confirm script storage and external refs.
  - `scriptDbOps`: script that reads/writes `tableMixed` to ensure serialized references resolve.

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
├─ int32Small = 1234
├─ int32Negative = -4321
├─ stringLargeNumberLiteral = "0x1_0000_0000"
├─ booleanTrue = true
├─ booleanFalse = false
├─ realNumber = 3.14159
├─ nilValue = nil
├─ dateEpoch = 1970-01-01 00:00:00 (local)
├─ dateFarFuture = 2099-12-31 23:59:59 (local)
├─ stringAscii = "Hello, Frontier!"
├─ stringExtended = "café …" (high-bit chars)
├─ binarySmall = 16-byte blob 0x00..0x0F
├─ addressValueSystem = (odb address pointing into Frontier.root for system DB ref)
├─ addressValueGuest = (odb address pointing into this test.root guest DB, e.g., binarySmall)
├─ tableMixed (table)
│  ├─ a = 1
│  ├─ b = "two"
│  ├─ c = true
│  ├─ d = date(2000-01-01)
│  ├─ e = binary (small)
│  └─ f (table)
│     ├─ x = 10
│     └─ y = "sub"
├─ recordSimple (record; name→value pairs)
│  ├─ name = "rec"
│  ├─ flags = 1 (small int)
│  ├─ tag = "sample" (string)
│  ├─ subtable = { nested:int = 5, nestedStr = "nest" } (non-scalar value)
│  └─ emptySlot = nil
├─ recordEmpty (record with no fields)
├─ listStrings = ["alpha", "beta", "gamma"]
├─ listMixed = [1, "two", true, date(2000-01-01)]
├─ listWithRecord = [ { recname = "r1", recval = 42 }, "tail" ]
├─ listWithTable = [ { k = "v" }, tableMixed ] (non-scalar element reuse)
├─ listEmpty = [ ]
├─ outlineBasic (outline)
│  ├─ root (refcon: "root-ref")
│  │  ├─ child1 (refcon: "c1")
│  │  └─ child2 (refcon: "c2")
│  │     └─ grandchild (refcon: "gc"; with note/body)
├─ wptextBasic (wp text with bold/italic span + newline)
├─ wptextStylesheet (wp text with multiple styled runs and blank line)
├─ menuSample (menu)
│  ├─ Item One (enabled)
│  ├─ Item Two (disabled)
│  ├─ Separator
│  └─ Sub → [Sub Item]
├─ pictSmall (PICT graphic)
├─ scriptHello = dialog.alert("hello")
├─ scriptDbOps = script that reads/writes tableMixed
└─ (no card_type in fixture; can backfill from Frontier.root if needed later)

view0 → root table
```
