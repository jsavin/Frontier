# Pattern E: Nested Sub-Processors in Single EFP Block

## Discovery

The RC file structure reveals that **some EFP blocks contain MULTIPLE processors** as sub-blocks within a single resource.

## Structure in kernelverbs.rc

```c
1000  /*idopverbs*/ EFP DISCARDABLE
BEGIN
    3,                          // Number of "blocks" in this resource

    "op\0",                     // FIRST processor
        true,                   // Window required
        45,                     // Count of verbs
        "getlinetext\0",
        "level\0",
        ...

    "opattributes\0",           // SECOND processor (nested!)
        true,
        5,
        "addgroup\0",
        "getall\0",
        ...

    "script\0",                 // THIRD processor (nested!)
        true,
        13,
        "compile\0",
        "uncompile\0",
        ...
END
```

## Key Insight

**Three separate processors share one C implementation file:**

1. **op** (45 verbs) → `opverbs.c`
2. **opattributes** (5 verbs) → `opverbs.c` (same file!)
3. **script** (13 verbs) → `opverbs.c` (same file!)

**Total: 63 verbs in one enum, one dispatch function, one file**

## Implications

This is **different from Pattern D** (langverbs.c consolidation):

| Aspect              | Pattern D (langverbs.c)      | Pattern E (opverbs.c)           |
|---------------------|------------------------------|----------------------------------|
| RC structure        | Separate EFP blocks          | Single EFP block, nested procs   |
| Motivation          | Consolidate MANY small procs | Logical grouping of related procs|
| Processors          | Unrelated (dialog, kb, etc.) | Related (op, opattributes, script)|
| Namespace           | processor.verb               | processor.verb (hierarchical)    |
| C enum              | One big enum                 | One big enum                     |
| File location       | langverbs.c (generic)        | opverbs.c (specific)             |

## Complete Nested Processor List

From scanning kernelverbs.rc:

### EFP 1000 (opverbs)
- **op** (45 verbs)
- **opattributes** (5 verbs)
- **script** (13 verbs)
- **osa** (2 verbs) - might also be here

### Hypothesis: Check Other EFP Blocks

Let me check if there are other multi-processor EFP blocks...

## Verb Namespace

In UserTalk, these appear as:

```
op.getlinetext()          // First processor
op.attributes.addgroup()  // Second processor (dot notation!)
script.compile()          // Third processor
```

So the RC file structure **matches the UserTalk namespace hierarchy**!

- `op` is the parent
- `op.attributes` is accessed as `opattributes` in the RC file
- `script` is standalone but grouped with `op` in the same EFP block

## Implementation Pattern

All three processors share:

1. **Single enum** (`tyoptoken` in opverbs.c):
```c
typedef enum tyoptoken {
    // op verbs (0-44)
    linetextfunc,
    levelfunc,
    ...

    // opattributes verbs (start at ~45)
    addgroupfunc,
    getallfunc,
    ...

    // script verbs (start at ~50)
    compilefunc,
    uncompilefunc,
    ...

    ctopverbs  // count
} tyoptoken;
```

2. **Single dispatch function**:
```c
static boolean opfunctionvalue(short token, hdltreenode hparam1,
                                tyvaluerecord *vreturned,
                                bigstring bserror) {
    switch(token) {
        // Handles all 63 verbs across all 3 processors
    }
}
```

3. **Multiple init functions** (one per processor):
```c
boolean opinitverbs(void);           // Registers "op" processor
boolean opattributesinitverbs(void); // Registers "opattributes" processor
boolean scriptinitverbs(void);       // Registers "script" processor
```

## Impact on Parser

The current `parse_kernelverbs.py` **already handles this correctly**:

```python
# From parse_kernelverbs.py, line 271:
for proc_match in re.finditer(PROCESSOR_PATTERN, block_content):
    processor_name = proc_match.group(1)
    # ... creates separate EFPProcessor object for each
```

It treats each nested processor as a **separate processor** with its own init function.

This is **correct behavior** because each processor has its own:
- Namespace in UserTalk
- Init function in C
- Verb table registration

Even though they share:
- C source file
- Enum
- Dispatch function

## Pattern E vs Pattern A/B/C/D

**Pattern E is orthogonal to the other patterns:**

- Pattern A/B/C/D describe **enum naming conventions**
- Pattern E describes **RC file structure and file organization**

A processor can be:
- **Pattern E + Pattern C**: Like `op` (nested + inconsistent naming)
- **Pattern A alone**: Like `file` (standalone + consistent naming)
- **Pattern D alone**: Like `dialog` (consolidated in langverbs.c)

## Recommendation

**No special handling needed** - the parser already works correctly.

Just document that:
1. Some EFP blocks contain multiple processors
2. These processors share implementation but have separate init functions
3. This reflects UserTalk's namespace hierarchy (e.g., `op.attributes`)

## Other Potential Pattern E Candidates

Need to scan RC file for other EFP blocks with "Number of blocks" > 1:

```bash
grep -A 1 "EFP.*DISCARDABLE" kernelverbs.rc | grep -E "BEGIN|[0-9]+,"
```

This would show if there are other multi-processor EFP blocks.
