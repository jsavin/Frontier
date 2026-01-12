# xml.frontiervaluetotaggedtext Implementation Plan

**Status:** Planning Phase - User Review Required
**Created:** 2026-01-11
**File:** tests/headless_xml_verbs.c (lines 83-86)

## Overview

This is the last remaining stub in the XML processor. Implementing this verb will complete XML processor coverage (92% → 100%).

The verb converts a Frontier value to XML-RPC tagged text format, which is used for serializing ODB values for transport over network protocols (XML-RPC, SOAP, etc.).

---

## Specification

### Purpose

`xml.frontiervaluetotaggedtext(adrValue, indentLevel)` converts any Frontier value into an XML-RPC tagged text representation. This format wraps primitive values in type-specific XML tags and recursively serializes compound structures (tables and lists).

### Parameters

1. **adrValue** (address) - Address to the Frontier value to convert
2. **indentLevel** (long) - Indentation level for formatting (number of tabs)

### Return Value

Returns a **string** containing the XML-tagged representation of the value.

### Output Format Specification

The output format follows the XML-RPC specification for data types:

#### Primitive Types

| Frontier Type | XML Tag | Example Input | Example Output |
|--------------|---------|---------------|----------------|
| string | (no tag) | "hello" | `hello` (escaped: &, <, ]]>) |
| int/long | `<i4>` | 42 | `<i4>42</i4>` |
| double | `<double>` | 3.14 | `<double>3.14</double>` |
| boolean | `<boolean>` | true | `<boolean>1</boolean>` |
| | | false | `<boolean>0</boolean>` |
| date | `<dateTime.iso8601>` | date value | `<dateTime.iso8601>20260111T12:30:45</dateTime.iso8601>` |
| binary | `<base64>` | binary data | `<base64>BASE64DATA...</base64>` |

**String Escaping Rules:**
- `&` → `&amp;`
- `<` → `&lt;`
- `]]>` → `]]&gt;`

**Boolean Conversion:**
- `true` → coerced to long → "1"
- `false` → coerced to long → "0"

#### List Type (Array)

Lists are converted to `<array>` structures with a `<data>` container:

```xml
<array>
	<data>
		<value>item1</value>
		<value><i4>42</i4></value>
		<value>item3</value>
	</data>
</array>
```

**Special Case for Strings in Arrays:**
- String values are wrapped directly in `<value>` tags (no indentation change)
- Non-string values get indented `<value>` wrapper with nested type tag

#### Table Type (Struct)

Tables (external values that can be coerced to hash tables) are converted to `<struct>` with `<member>` elements:

```xml
<struct>
	<member>
		<name>fieldName</name>
		<value>fieldValue</value>
	</member>
	<member>
		<name>count</name>
		<value><i4>10</i4></value>
	</member>
</struct>
```

Each member contains:
- `<name>` tag with the hash key name
- `<value>` tag with the tagged representation of the value

#### External Values

External values (e.g., outline, wptext, menu, script) that are NOT tables:
- Fall through to default case
- Converted using `xmlvaltostring()` (delegates to type-specific serialization)

### Indentation

- Each level adds one tab character (`\t`)
- Opening tags increase indent level for nested content
- Closing tags restore previous indent level
- Leading tabs are stripped from final output
- Trailing carriage returns are stripped from final output

### Example Transformations

#### Example 1: Simple String
```usertalk
Input:  @workspace.name = "Test"
        xml.frontiervaluetotaggedtext(@workspace.name, 0)
Output: "Test"
```

#### Example 2: Integer
```usertalk
Input:  @workspace.count = 42
        xml.frontiervaluetotaggedtext(@workspace.count, 0)
Output: "<i4>42</i4>"
```

#### Example 3: Boolean
```usertalk
Input:  @workspace.flag = true
        xml.frontiervaluetotaggedtext(@workspace.flag, 0)
Output: "<boolean>1</boolean>"
```

#### Example 4: List
```usertalk
Input:  @workspace.items = {"hello", 42, true}
        xml.frontiervaluetotaggedtext(@workspace.items, 0)
Output: "<array>\n\t<data>\n\t\t<value>hello</value>\n\t\t<value><i4>42</i4></value>\n\t\t<value><boolean>1</boolean></value>\n\t</data>\n</array>"
```

#### Example 5: Table (Struct)
```usertalk
Input:  new(tableType, @workspace.person)
        workspace.person.name = "Alice"
        workspace.person.age = 30
        xml.frontiervaluetotaggedtext(@workspace.person, 0)
Output: "<struct>\n\t<member>\n\t\t<name>age</name>\n\t\t<value><i4>30</i4></value>\n\t</member>\n\t<member>\n\t\t<name>name</name>\n\t\t<value>Alice</value>\n\t</member>\n</struct>"
```

### Edge Cases

1. **Empty string** → Returns empty string (no tags)
2. **Empty list** → `<array><data></data></array>`
3. **Empty table** → `<struct></struct>`
4. **External values that aren't tables** → Delegates to `xmlvaltostring()` for type-specific serialization
5. **Unsupported value types** → Returns error via `xmlvaltostring()` delegation

---

## Implementation Plan

### Phase 1: Function Signature and Parameter Extraction

**File:** `tests/headless_xml_verbs.c`

**Tasks:**
1. Replace stub case `xmlv_frontiervaluetotaggedtext` with real implementation
2. Extract address parameter (param 1): Use `getaddressparam()`
3. Extract indentlevel parameter (param 2): Use `getlongvalue()` with `flnextparamislast = true`
4. Resolve address to hash table and name: Use `getaddressvalue()`
5. Lookup value in hash table: Use `langhashtablelookup()` to get `tyvaluerecord` and `hdlhashnode`

**Pattern to follow:**
```c
case xmlv_frontiervaluetotaggedtext: {
    Handle htext;
    hdlhashtable ht;
    bigstring bs;
    tyvaluerecord val;
    long indentlevel;
    hdlhashnode hnode;

    if (!getaddressparam(hparam1, 1, &val))
        return false;

    if (!getaddressvalue(val, &ht, bs))
        return false;

    flnextparamislast = true;

    if (!getlongvalue(hparam1, 2, &indentlevel))
        return false;

    if (!langhashtablelookup(ht, bs, &val, &hnode))
        return false;

    // Call core conversion function...

    return setheapvalue(htext, stringvaluetype, vreturned);
}
```

### Phase 2: Core Conversion Function

**Reuse existing code:** The function `xmlfrontiervaltotaggedtext()` already exists in `Common/source/langxml.c` (lines 380-530).

**Strategy:** Call the existing implementation directly:

```c
if (!xmlfrontiervaltotaggedtext(&val, indentlevel, &htext, hnode))
    return false;

return setheapvalue(htext, stringvaluetype, vreturned);
```

**Why this works:**
- The existing function is already exported (used internally by `xmladdtaggedvalue()`)
- It handles all value types correctly (primitives, lists, tables, externals)
- It performs proper indentation and cleanup (strip leading tabs, trailing CRs)
- It's battle-tested in legacy Frontier

### Phase 3: Function Declaration

**Files to modify:**
1. `Common/source/langxml.c` - Remove `static` keyword from function definition (line 380)
2. `Common/headers/langxml.h` - Add forward declaration

**Changes:**

**In langxml.c (line 380):**
```c
// Change from:
static boolean xmlfrontiervaltotaggedtext (tyvaluerecord *val, short indentlevel, Handle *xmltext, hdlhashnode hnode) {

// To:
boolean xmlfrontiervaltotaggedtext (tyvaluerecord *val, short indentlevel, Handle *xmltext, hdlhashnode hnode) {
```

**In langxml.h (add at end of file, before closing):**
```c
extern boolean xmlfrontiervaltotaggedtext(tyvaluerecord *val, short indentlevel, Handle *xmltext, hdlhashnode hnode);
```

### Phase 4: Build Integration

**Files to modify:**
1. `tests/headless_xml_verbs.c` - Implementation
2. `Common/headers/langxml.h` - Declaration (if needed)
3. Possibly `frontier-cli/Makefile` - Ensure `langxml.c` is linked

**Compilation check:**
```bash
make clean && make
```

### Phase 5: Testing

#### Unit Test Strategy

Create integration test file: `tests/integration/xml_frontiervaluetotaggedtext.yaml`

**Test cases:**

1. **Primitive string**
   ```yaml
   - name: "xml.frontiervaluetotaggedtext - string value"
     script: |
       local(s = "hello");
       return xml.frontiervaluetotaggedtext(@s, 0)
     expected_output: "hello"
   ```

2. **Primitive integer**
   ```yaml
   - name: "xml.frontiervaluetotaggedtext - integer value"
     script: |
       local(n = 42);
       return xml.frontiervaluetotaggedtext(@n, 0)
     expected_output: "<i4>42</i4>"
   ```

3. **Boolean true**
   ```yaml
   - name: "xml.frontiervaluetotaggedtext - boolean true"
     script: |
       local(b = true);
       return xml.frontiervaluetotaggedtext(@b, 0)
     expected_output: "<boolean>1</boolean>"
   ```

4. **Boolean false**
   ```yaml
   - name: "xml.frontiervaluetotaggedtext - boolean false"
     script: |
       local(b = false);
       return xml.frontiervaluetotaggedtext(@b, 0)
     expected_output: "<boolean>0</boolean>"
   ```

5. **List (array)**
   ```yaml
   - name: "xml.frontiervaluetotaggedtext - list"
     script: |
       local(items = {"a", 1, true});
       return xml.frontiervaluetotaggedtext(@items, 0)
     expected_pattern: "<array>.*<data>.*<value>a</value>.*<value><i4>1</i4></value>.*<value><boolean>1</boolean></value>.*</data>.*</array>"
   ```

6. **Table (struct)**
   ```yaml
   - name: "xml.frontiervaluetotaggedtext - table"
     script: |
       new(tableType, @temp);
       temp.name = "test";
       temp.count = 5;
       return xml.frontiervaluetotaggedtext(@temp, 0)
     expected_pattern: "<struct>.*<member>.*<name>count</name>.*<value><i4>5</i4></value>.*</member>.*<member>.*<name>name</name>.*<value>test</value>.*</member>.*</struct>"
   ```

7. **String with escaping**
   ```yaml
   - name: "xml.frontiervaluetotaggedtext - string with XML chars"
     script: |
       local(s = "a < b & c");
       return xml.frontiervaluetotaggedtext(@s, 0)
     expected_output: "a &lt; b &amp; c"
   ```

8. **Indentation level**
   ```yaml
   - name: "xml.frontiervaluetotaggedtext - indentation"
     script: |
       local(n = 42);
       return xml.frontiervaluetotaggedtext(@n, 2)
     expected_output: "<i4>42</i4>"  # Leading tabs stripped
   ```

#### Manual Testing

```bash
# Test string
./frontier-cli/frontier-cli -e 'local(s="hello"); return xml.frontiervaluetotaggedtext(@s, 0)'

# Test integer
./frontier-cli/frontier-cli -e 'local(n=42); return xml.frontiervaluetotaggedtext(@n, 0)'

# Test list
./frontier-cli/frontier-cli -e 'local(items={"a", 1}); return xml.frontiervaluetotaggedtext(@items, 0)'

# Test table
./frontier-cli/frontier-cli -e 'new(tableType, @t); t.x=1; return xml.frontiervaluetotaggedtext(@t, 0)'
```

---

## Dependencies

### Code Dependencies

1. **Existing function:** `xmlfrontiervaltotaggedtext()` in `Common/source/langxml.c`
2. **Helper functions:**
   - `xmlvaltostring()` - Converts primitive values to XML strings
   - `xmladdtaggedvalue()` - Adds tagged values to handlestream
   - `getaddressparam()` - Extracts address parameter
   - `getaddressvalue()` - Resolves address to hash table + name
   - `langhashtablelookup()` - Looks up value in hash table
   - `setheapvalue()` - Returns string result

### Build Dependencies

- `Common/source/langxml.c` must be linked into `frontier-cli`
- Check `frontier-cli/Makefile` for `langxml.o` inclusion

---

## Complexity Estimate

**Complexity:** Low

**Reasoning:**
- Core implementation already exists in `Common/source/langxml.c`
- Only need to wire up verb dispatcher to existing function
- Pattern is identical to `xmlvaltostringverb()` (lines 3496-3523)
- Minimal new code required (~15-20 lines)

**Estimated Time:**
- Implementation: 30 minutes
- Testing: 1 hour (write integration tests, manual verification)
- Total: ~1.5 hours

---

## Open Questions

### Question 1: Function Visibility ✅ RESOLVED

**Q:** Is `xmlfrontiervaltotaggedtext()` already visible to `tests/headless_xml_verbs.c`, or does it need to be declared in a header?

**Status:** NEEDS DECLARATION

**Investigation results:**
- Checked `Common/headers/langxml.h` - Function is NOT currently exported
- File currently exports: `xmlcompile`, `isxmlmatch`, `xmlgetname`, `xmlgetattribute`, `gethashnodetable`, `replaceallinhandle`
- `xmlfrontiervaltotaggedtext()` is a static function in langxml.c (line 380)

**Resolution:** Need to add declaration to `Common/headers/langxml.h`:
```c
extern boolean xmlfrontiervaltotaggedtext(tyvaluerecord *val, short indentlevel, Handle *xmltext, hdlhashnode hnode);
```

**Also need to:** Remove `static` keyword from function definition in `Common/source/langxml.c:380`

### Question 2: Build System ✅ RESOLVED

**Q:** Is `langxml.c` already linked into `frontier-cli` binary?

**Status:** YES - Already linked

**Verification result:**
```bash
$ grep -n "langxml" frontier-cli/Makefile
109:    ../Common/source/langxml.c \
```

**Resolution:** No changes needed - `langxml.c` is already part of the build.

---

## Risks and Mitigations

### Risk 1: Linker Errors

**Risk:** `xmlfrontiervaltotaggedtext()` might not be linked into headless build.

**Mitigation:**
- Verify `langxml.o` is in `frontier-cli/Makefile`
- If missing, add to `COMMON_OBJECTS` or equivalent

### Risk 2: Header Visibility

**Risk:** Function declaration might not be visible to headless verbs.

**Mitigation:**
- Add `#include "langxml.h"` to `tests/headless_xml_verbs.c` if needed
- Declare function in header if not already exported

### Risk 3: Integration Test Complexity

**Risk:** Testing table/list output might be challenging due to indentation/whitespace variations.

**Mitigation:**
- Use pattern matching (regex) for complex structures instead of exact string match
- Strip whitespace for comparison if needed
- Start with simple primitive tests, then add complex structures

---

## Success Criteria

1. ✅ **Compilation:** Code compiles without errors or warnings
2. ✅ **Basic functionality:** Primitive types (string, int, boolean, double) convert correctly
3. ✅ **Complex structures:** Lists and tables serialize to correct XML-RPC format
4. ✅ **String escaping:** XML special characters are properly escaped
5. ✅ **Indentation:** Output is correctly indented and stripped
6. ✅ **Integration tests:** All test cases pass
7. ✅ **Coverage:** XML processor reaches 100% coverage

---

## Post-Implementation Tasks

1. Update file header in `tests/headless_xml_verbs.c` to indicate it's no longer a generated stub
2. Update verb implementation status tracking
3. Update coverage reports (92% → 100%)
4. Consider adding this verb to OPML export for Dave Winer's subscription

---

## References

- **Existing implementation:** `Common/source/langxml.c:380-530` (xmlfrontiervaltotaggedtext)
- **Verb dispatcher pattern:** `Common/source/langxml.c:3496-3523` (xmlfrontiervaltotaggedtextverb)
- **XML-RPC spec:** http://www.xmlrpc.com/spec (for tagged text format)
- **UserTalk reference:** `usertalk_scripts/Frontier.root/system/verbs/builtins/xml/coercions/frontierValueToTaggedText.ut`
- **String constants:** `Common/headers/stringdefs.h` (STR_boolean, STR_double, STR_int, etc.)
- **Tag constants:** `Common/source/langxml.c:64-75` (STR_value_begin, STR_array_begin, etc.)
