#!/usr/bin/env python3
"""
stub_config.py - Configuration for automated stub generation

This file defines the implementation strategy for kernel verbs that can't
run in headless mode. Used by generate_processor_stubs.py to generate
appropriate stub implementations.

Based on Phase 3 categorization from MISSING_VERBS_REVIEW_WITH_AUDITS.md
"""

# Stub implementation categories
STUB_ERROR = 'error'      # Return false with specific error message
STUB_NOOP = 'noop'        # Return true silently (safe no-op)
STUB_FORWARD = 'forward'  # Forward to real C implementation function
STUB_CUSTOM = 'custom'    # Hand-implemented in the C file; generator must not overwrite
STUB_DEFAULT = 'default'  # Generic "not implemented" error

# Error message templates by category
ERROR_MESSAGES = {
    'osa': "Can't call OSA verbs because AppleScript is not available in headless mode",
    'gui_dialog': "Can't use modal dialog verbs because GUI is not available in headless mode",
    'gui_statusbar': "Can't use status bar verbs because GUI is not available in headless mode",
    'gui_window': "Can't use window verbs because GUI is not available in headless mode",
    'gui_mainwindow': "Can't use mainwindow verbs because GUI is not available in headless mode",
    'admin': "Can't {action} because it requires administrator privileges",
    'platform_windows': "Can't run Windows shell commands because they are not available on this platform",
    'platform_unsupported': "Can't {action} because it is not implemented on this platform",
    'usertalk_layer': "Can't {verb_name} because it is not implemented in the kernel - it will be available via UserTalk glue scripts",
}

# Verb-specific stub configurations
# Format: (processor, verb_name): (stub_type, error_category_or_params)
STUB_CONFIGS = {
    # Category 1a: OSA/AppleScript verbs (8 verbs) - lang processor
    ('lang', 'DDEevent'): (STUB_ERROR, 'osa'),
    ('lang', 'callxcmd'): (STUB_ERROR, 'osa'),
    ('lang', 'countapplelistitems'): (STUB_ERROR, 'osa'),
    ('lang', 'getapplelistitem'): (STUB_ERROR, 'osa'),
    ('lang', 'putapplelistitem'): (STUB_ERROR, 'osa'),
    ('lang', 'geteventattribute'): (STUB_ERROR, 'osa'),
    ('lang', 'seteventinteraction'): (STUB_ERROR, 'osa'),
    ('lang', 'transactionEvent'): (STUB_ERROR, 'osa'),

    # Category 1b: GUI-Only Dialog Verbs (5 verbs) - dialog processor
    ('dialog', 'hideitem'): (STUB_ERROR, 'gui_dialog'),
    ('dialog', 'ismodalcard'): (STUB_ERROR, 'gui_dialog'),
    ('dialog', 'runcard'): (STUB_ERROR, 'gui_dialog'),
    ('dialog', 'setmodalcardtimeout'): (STUB_ERROR, 'gui_dialog'),
    ('dialog', 'showitem'): (STUB_ERROR, 'gui_dialog'),

    # Category 1c: Status Bar Verbs (5 verbs) - statusbar processor
    ('statusbar', 'msg'): (STUB_ERROR, 'gui_statusbar'),
    ('statusbar', 'getmessage'): (STUB_ERROR, 'gui_statusbar'),
    ('statusbar', 'getsectionone'): (STUB_ERROR, 'gui_statusbar'),
    ('statusbar', 'getsections'): (STUB_ERROR, 'gui_statusbar'),
    ('statusbar', 'setsections'): (STUB_ERROR, 'gui_statusbar'),

    # Category 1d: Window Verbs (3 verbs) - window processor
    ('window', 'dbstats'): (STUB_ERROR, 'gui_window'),
    ('window', 'getposition'): (STUB_ERROR, 'gui_window'),
    ('window', 'setposition'): (STUB_ERROR, 'gui_window'),

    # Category 1d2: Mainwindow Verbs (7 verbs) - mainwindow processor
    ('mainwindow', 'showflag'): (STUB_ERROR, 'gui_mainwindow'),
    ('mainwindow', 'hideflag'): (STUB_ERROR, 'gui_mainwindow'),
    ('mainwindow', 'showpopup'): (STUB_ERROR, 'gui_mainwindow'),
    ('mainwindow', 'hidepopup'): (STUB_ERROR, 'gui_mainwindow'),
    ('mainwindow', 'showbuttons'): (STUB_ERROR, 'gui_mainwindow'),
    ('mainwindow', 'hidebuttons'): (STUB_ERROR, 'gui_mainwindow'),
    ('mainwindow', 'showserverstats'): (STUB_ERROR, 'gui_mainwindow'),

    # Category 1e: Admin-Required Operations (1 verb)
    ('clock', 'set'): (STUB_ERROR, {'template': 'admin', 'action': 'set system time'}),

    # Category 1f: Platform-Specific Operations (1 verb)
    ('sys', 'winshellcommand'): (STUB_ERROR, 'platform_windows'),
    ('file', 'mountservervolume'): (STUB_ERROR, {'template': 'platform_unsupported', 'action': 'mount server volumes'}),

    # Category 1g: UserTalk Layer Implementation (2 verbs)
    # These verbs should be implemented in UserTalk glue scripts, not in kernel
    ('file', 'getspecialfolderpath'): (STUB_ERROR, {'template': 'usertalk_layer', 'verb_name': 'file.getSpecialFolderPath'}),
    ('file', 'getsystemfolderpath'): (STUB_ERROR, {'template': 'usertalk_layer', 'verb_name': 'file.getSystemFolderPath'}),

    # Category 2: Silent Success (Noop) Implementation (3 verbs)
    ('table', 'getdisplaysettings'): (STUB_NOOP, None),
    ('table', 'setdisplaysettings'): (STUB_NOOP, None),
    ('lang', 'flushmemory'): (STUB_NOOP, None),

    # Category 3: Forward to Real C Implementation
    # These verbs have real implementations that can be called directly
    ('lang', 'new'): (STUB_FORWARD, 'newvaluefunc'),
    ('lang', 'gettarget'): (STUB_FORWARD, 'langgettargetfunc'),
    ('lang', 'settarget'): (STUB_FORWARD, 'langsettargetfunc'),
    ('lang', 'cleartarget'): (STUB_FORWARD, 'langcleartargetfunc'),

    # Phase 1 type conversion verbs
    ('lang', 'double'): (STUB_FORWARD, 'langdoublefunc'),
    ('lang', 'single'): (STUB_FORWARD, 'langsinglefunc'),
    ('lang', 'fixed'): (STUB_FORWARD, 'langfixedfunc'),
    ('lang', 'direction'): (STUB_FORWARD, 'langdirectionfunc'),
    ('lang', 'string4'): (STUB_FORWARD, 'langstring4func'),

    # Phase 2: Memory & Utility verbs
    ('lang', 'abs'): (STUB_FORWARD, 'langabsfunc'),
    ('lang', 'random'): (STUB_FORWARD, 'langrandomfunc'),
    ('lang', 'memavail'): (STUB_FORWARD, 'langmemavailfunc'),

    # Phase 3: Core Lang Operations verbs
    ('lang', 'delete'): (STUB_FORWARD, 'langdeletefunc'),
    ('lang', 'evaluate'): (STUB_FORWARD, 'langevaluatefunc'),
    ('lang', 'callscript'): (STUB_FORWARD, 'langcallscriptfunc'),
    ('lang', 'msg'): (STUB_FORWARD, 'langmsgfunc'),

    # Phase 4: Date/Time verbs
    ('lang', 'timecreated'): (STUB_FORWARD, 'langtimecreatedfunc'),
    ('lang', 'timemodified'): (STUB_FORWARD, 'langtimemodifiedfunc'),
    ('lang', 'settimecreated'): (STUB_FORWARD, 'langsettimecreatedfunc'),
    ('lang', 'settimemodified'): (STUB_FORWARD, 'langsettimemodifiedfunc'),

    # Phase 5: Type conversion verbs (15 verbs)
    ('lang', 'boolean'): (STUB_FORWARD, 'langbooleanfunc'),
    ('lang', 'char'): (STUB_FORWARD, 'langcharfunc'),
    ('lang', 'long'): (STUB_FORWARD, 'langlongfunc'),
    ('lang', 'date'): (STUB_FORWARD, 'langdatefunc'),
    ('lang', 'string'): (STUB_FORWARD, 'langstringfunc'),
    ('lang', 'address'): (STUB_FORWARD, 'langaddressfunc'),
    ('lang', 'binary'): (STUB_FORWARD, 'langbinaryfunc'),
    ('lang', 'point'): (STUB_FORWARD, 'langpointfunc'),
    ('lang', 'rect'): (STUB_FORWARD, 'langrectfunc'),
    ('lang', 'rgb'): (STUB_FORWARD, 'langrgbfunc'),
    ('lang', 'pattern'): (STUB_FORWARD, 'langpatternfunc'),
    ('lang', 'filespec'): (STUB_FORWARD, 'langfilespecfunc'),
    ('lang', 'list'): (STUB_FORWARD, 'langlistfunc'),
    ('lang', 'record'): (STUB_FORWARD, 'langrecordfunc'),
    ('lang', 'enum'): (STUB_FORWARD, 'langenumfunc'),

    # target verbs
    ('target', 'get'): (STUB_FORWARD, 'langgettargetfunc'),
    ('target', 'set'): (STUB_FORWARD, 'langsettargetfunc'),
    ('target', 'clear'): (STUB_FORWARD, 'langcleartargetfunc'),

    # Category 4: Hand-Implemented (Custom) Frontier Verbs
    # These are hand-implemented in headless_frontier_verbs.c and must not
    # be overwritten by the generator. Each has specific headless behavior.
    ('frontier', 'getprogrampath'): (STUB_CUSTOM, 'returns path to CLI executable'),
    ('frontier', 'getfilepath'): (STUB_CUSTOM, 'returns path to loaded database'),
    ('frontier', 'enableagents'): (STUB_CUSTOM, 'no-op, returns true'),
    ('frontier', 'isruntime'): (STUB_CUSTOM, 'returns false (CLI is not runtime-only)'),
    ('frontier', 'countthreads'): (STUB_CUSTOM, 'delegates to processthreadcount()'),
    ('frontier', 'ispowerpc'): (STUB_CUSTOM, 'returns true (suppresses legacy 68k workarounds)'),
    ('frontier', 'reclaimmemory'): (STUB_CUSTOM, 'no-op, returns true (modern OS handles memory)'),
    ('frontier', 'version'): (STUB_CUSTOM, 'returns product version string'),
    ('frontier', 'cliversion'): (STUB_CUSTOM, 'returns CLI distribution version'),
    ('frontier', 'isvalidserialnumber'): (STUB_CUSTOM, 'always returns true in headless'),
}


def get_stub_config(processor: str, verb: str) -> tuple:
    """
    Get stub configuration for a specific verb.

    Args:
        processor: Processor name (e.g., 'lang', 'dialog')
        verb: Verb name (e.g., 'DDEevent', 'hideitem')

    Returns:
        Tuple of (stub_type, config) or (STUB_DEFAULT, None) if not configured
    """
    return STUB_CONFIGS.get((processor, verb), (STUB_DEFAULT, None))


def get_error_message(config) -> str:
    """
    Generate error message from config.

    Args:
        config: Either a string key into ERROR_MESSAGES or a dict with 'template' and params

    Returns:
        Formatted error message string
    """
    if isinstance(config, str):
        return ERROR_MESSAGES[config]
    elif isinstance(config, dict):
        template = ERROR_MESSAGES[config['template']]
        return template.format(**{k: v for k, v in config.items() if k != 'template'})
    return "not implemented"


def get_stub_implementation(processor: str, verb: str, token_name: str) -> list:
    """
    Generate C code lines for stub implementation.

    Args:
        processor: Processor name
        verb: Verb name
        token_name: C enum token name (e.g., 'lanv_DDEevent')

    Returns:
        List of C code lines for the case statement
    """
    stub_type, config = get_stub_config(processor, verb)

    lines = [f"        case {token_name}:"]

    if stub_type == STUB_ERROR:
        error_msg = get_error_message(config)
        lines.extend([
            f"            /* {processor}.{verb} - {stub_type} stub */",
            f"            if (bserror)",
            f"                copystring(BIGSTRING(\"\\p{error_msg}\"), bserror);",
            f"            return false;",
        ])
    elif stub_type == STUB_NOOP:
        lines.extend([
            f"            /* {processor}.{verb} - {stub_type} stub (safe no-op) */",
            f"            (void)hparam1;  /* Suppress unused parameter warning */",
            f"            return true;",
        ])
    elif stub_type == STUB_FORWARD:
        # Forward to real C implementation function
        func_name = config
        lines.extend([
            f"            /* Verb: {processor}.{verb} - forward to real implementation */",
            f"            return {func_name}(hparam1, vreturned);",
        ])
    elif stub_type == STUB_CUSTOM:
        # Hand-implemented in the C file; generator emits a placeholder comment
        description = config or 'hand-implemented'
        lines.extend([
            f"            /* Verb: {processor}.{verb} - CUSTOM: {description} */",
            f"            /* This verb is hand-implemented. Do not auto-generate. */",
            f"            #error \"{processor}.{verb} is marked STUB_CUSTOM -- do not regenerate this file\"",
        ])
    else:  # STUB_DEFAULT
        lines.extend([
            f"            /* Verb: {processor}.{verb} - not yet implemented */",
            f"            log_warn(LOG_COMP_LANG, \"{processor}.{verb} not yet implemented\");",
            f"            if (bserror) copystring(BIGSTRING(\"\\pnot implemented\"), bserror);",
            f"            return false;",
        ])

    return lines
