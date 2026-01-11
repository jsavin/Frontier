#!/usr/bin/env python3
"""
Configuration for kernel verbs implemented in UserTalk scripts.

Some kernel verbs are implemented in UserTalk glue scripts rather than in C.
This module tracks these verbs so the verb binding analyzer can correctly
identify them as "implemented" rather than "stubbed".

The analyzer only looks at C source files by default, so script-implemented
verbs would incorrectly show as missing implementations without this tracking.
"""

# Verbs implemented in UserTalk scripts
# Format: {processor_name: {verb_names}}
SCRIPT_IMPLEMENTED_VERBS = {
    'op': {
        # Op processor verbs implemented in UserTalk
        'insertatendoflist',  # op.insertAtEndOfList - add item as last child
        'wipe',               # op.wipe - delete all headings
        'visit',              # op.visit - tree traversal with callback
        'fullexpand',         # op.fullExpand - expand entire outline
        'fullcollapse',       # op.fullCollapse - collapse entire outline

        # Note: op.visitAll is implemented in C (headless_op_verbs.c)
        # Note: op.visitSelection is also script-implemented but may not be in RC file
    },
}


def is_script_implemented(processor: str, verb_name: str) -> bool:
    """
    Check if a verb is implemented in UserTalk scripts.

    Args:
        processor: Processor name (e.g., "op")
        verb_name: Verb name (lowercase, e.g., "insertatendoflist")

    Returns:
        True if verb is script-implemented, False otherwise
    """
    if processor not in SCRIPT_IMPLEMENTED_VERBS:
        return False

    # Normalize verb name to lowercase for comparison
    verb_lower = verb_name.lower()
    return verb_lower in SCRIPT_IMPLEMENTED_VERBS[processor]


def get_script_implemented_verbs(processor: str) -> set:
    """
    Get all script-implemented verbs for a processor.

    Args:
        processor: Processor name (e.g., "op")

    Returns:
        Set of verb names (lowercase) that are script-implemented
    """
    return SCRIPT_IMPLEMENTED_VERBS.get(processor, set())
