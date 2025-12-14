#!/usr/bin/env python3
"""
Exception tables for verb name mapping.

These tables handle processors where RC verb names don't map predictably
to C enum tokens or case labels.
"""

# Pattern C: Inconsistent naming exceptions
# Processors where most verbs follow {verb}func pattern, but some exceptions exist
PATTERN_C_EXCEPTIONS = {
    'op': {
        # RC verb → C enum token
        'getlinetext': 'linetextfunc',
        'subsexpanded': 'getexpandedfunc',
        'getselection': 'getselectfunc',
    },
    'pict': {
        'expressions': 'evalfunc',
    },
}

# Pattern D: Multi-processor consolidation
# Processors that are implemented in langverbs.c instead of separate files
PATTERN_D_PROCESSORS = {
    'dialog', 'clock', 'date', 'kb', 'mouse',
    'point', 'rectangle', 'rgb', 'speaker', 'target'
}

# Pattern D: Specific exceptions for consolidated processors
# These have unusual transformations beyond standard patterns
PATTERN_D_EXCEPTIONS = {
    'dialog': {
        # RC verb → C enum token
        'notify': 'notifytdialogfunc',           # Typo in C (extra 't')
        'getpassword': 'askpassworddialogfunc',  # Different verb entirely
    },
}


def get_pattern_c_exception(processor: str, rc_verb: str) -> str:
    """
    Get exception mapping for Pattern C processor.

    Args:
        processor: Processor name (e.g., "op")
        rc_verb: RC verb name (e.g., "getlinetext")

    Returns:
        C enum token if exception exists, None otherwise
    """
    if processor in PATTERN_C_EXCEPTIONS:
        return PATTERN_C_EXCEPTIONS[processor].get(rc_verb)
    return None


def is_pattern_d_processor(processor: str) -> bool:
    """
    Check if processor uses Pattern D (multi-processor consolidation).

    Pattern D processors are implemented in langverbs.c instead of
    having separate files.

    Args:
        processor: Processor name

    Returns:
        True if processor uses Pattern D
    """
    return processor in PATTERN_D_PROCESSORS


def get_pattern_d_exception(processor: str, rc_verb: str) -> str:
    """
    Get exception mapping for Pattern D processor.

    Args:
        processor: Processor name (e.g., "dialog")
        rc_verb: RC verb name (e.g., "notify")

    Returns:
        C enum token if exception exists, None otherwise
    """
    if processor in PATTERN_D_EXCEPTIONS:
        return PATTERN_D_EXCEPTIONS[processor].get(rc_verb)
    return None


def generate_pattern_d_candidates(processor: str, rc_verb: str) -> list:
    """
    Generate possible enum token candidates for Pattern D processors.

    Pattern D processors inject the processor name into the enum token
    in various positions.

    Args:
        processor: Processor name (e.g., "dialog")
        rc_verb: RC verb name (e.g., "alert")

    Returns:
        List of candidate enum tokens to try, in priority order
    """
    candidates = []

    # Check exception table first
    exception = get_pattern_d_exception(processor, rc_verb)
    if exception:
        candidates.append(exception)

    # Pattern D1: {verb}{processor}func (most common)
    # Example: dialog.getvalue → getvaluedialogfunc
    candidates.append(f"{rc_verb}{processor}func")

    # Pattern D2: {processor}{verb}func
    # Example: dialog.alert → alertdialogfunc
    candidates.append(f"{processor}{rc_verb}func")

    # Pattern D3: verb has processor infix
    # Example: dialog.getvalue → getdialogvaluefunc
    # Try to insert processor name after common prefixes
    for prefix in ['get', 'set', 'is', 'run']:
        if rc_verb.startswith(prefix):
            remainder = rc_verb[len(prefix):]
            candidates.append(f"{prefix}{processor}{remainder}func")

    # Fallback: standard pattern
    candidates.append(f"{rc_verb}func")

    return candidates
