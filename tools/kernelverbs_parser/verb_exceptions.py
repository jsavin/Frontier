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
    'frontier': {
        # frontier verbs are in shellsysverbs.c with irregular mappings
        'getprogrampath': 'programpathfunc',
        'getfilepath': 'filepathfunc',
        'enableagents': 'agentsenablefunc',
        'ispowerpc': 'isnativefunc',
        'version': 'frontierversionfunc',
    },
    'sys': {
        # sys verbs are in shellsysverbs.c with irregular mappings
        'osversion': 'systemversionfunc',
        'appisrunning': 'apprunningfunc',
        'frontmostapp': 'frontappfunc',
    },
    'crypt': {
        # crypt verbs in langcrypt.c use lowercase C enum tokens
        # Note: verb names are normalized to lowercase in parse_kernelverbs.py
        'hmacmd5': 'hmacmd5func',
        'md5': 'md5func',
        'sha1': 'sha1func',
        'hmacsha1': 'hmacsha1func',
    },
    'file': {
        # file verbs in fileverbs.c have complex naming patterns:
        # 1. File I/O verbs with infix: open → openfilefunc, close → closefilefunc
        # 2. Volume verbs with prefix: eject → volumeejectfunc, isejectable → volumeisejectablefunc
        # 3. Space verbs with prefix: freespaceonvolume → volumefreespacefunc
        # 4. Dialog verbs with prefix: getfiledialog → sfgetfilefunc
        # 5. Version verbs: getversion → getshortversionfunc, getfullversion → setlongversionfunc
        # Note: verb names are normalized to lowercase in parse_kernelverbs.py
        'close': 'closefilefunc',
        'eject': 'volumeejectfunc',
        'findapplication': 'filelaunchfunc',
        'freespaceonvolume': 'volumefreespacefunc',
        'freespaceonvolumedouble': 'volumefreespacedoublefunc',
        'getlabelindex': 'getlabelindexfunc',
        'getlabelnames': 'getlabelnamesfunc',
        'getposixpath': 'getposixpathfunc',
        'getdiskdialog': 'sfgetdiskfunc',
        'getfiledialog': 'sfgetfilefunc',
        'getfolderdialog': 'sfgetfolderfunc',
        'getfullversion': 'setlongversionfunc',
        'getversion': 'getshortversionfunc',
        'isejectable': 'volumeisejectablefunc',
        'open': 'openfilefunc',
        'putfiledialog': 'sfputfilefunc',
        'setlabelindex': 'setlabelindexfunc',
        'setcreated': 'setfilecreatedfunc',
        'setcreator': 'setfilecreatorfunc',
        'setfullversion': 'setlongversionfunc',
        'setmodified': 'setfilemodifiedfunc',
        'settype': 'setfiletypefunc',
        'setversion': 'setshortversionfunc',
    },
    'table': {
        # table verbs in tableverbs.c have consistent mapping with {verb}func pattern
        # Note: verb names are normalized to lowercase in parse_kernelverbs.py
        'getsortorder': 'sortorderfunc',
        'getcursor': 'getcursorfunc',
        'go': 'gofunc',
        'goto': 'gotofunc',
        'gotoname': 'gotonamefunc',
        'sortby': 'sortbyfunc',
    },
    'string': {
        # string verbs in stringverbs.c - mostly case-sensitive naming issues
        # Note: verb names are normalized to lowercase in parse_kernelverbs.py
        'hashmd5': 'hashmd5func',
        'innercasename': 'innercasefunc',
        'lower': 'lowercasefunc',
        'upper': 'uppercasefunc',
        'parsehttpargs': 'parseargsfunc',
        'processhtmlmacros': 'processmacrosfunc',
    },
    'lang': {
        # lang verbs in langverbs.c
        'flushmemory': 'flushmemfunc',
    },
    'db': {
        # db verbs in dbverbs.c
        # Note: verb names are normalized to lowercase in parse_kernelverbs.py
        'istable': 'istablefunc',
        'newtable': 'newtablefunc',
    },
    'op': {
        # Add existing exceptions
        'getlinetext': 'linetextfunc',
        'subsexpanded': 'getexpandedfunc',
        'getselection': 'getselectfunc',
        # New exception
        'tabkeyreorg': 'tabkeyreorgfunc',
    },
    'menu': {
        # menu verbs in menuverbs.c
        'getscript': 'getscriptfunc',
    },
    'html': {
        # html verbs (some in langhtml.c)
        'drawcalendar': 'htmlcalendardrawfunc',
    },
    'inetd': {
        # inetd verbs in langhtml.c
        'supervisor': 'inetdsupervisorfunc',
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
    'clock': {
        # RC verb names differ from C enum tokens
        'now': 'timefunc',
        'set': 'settimefunc',
        'sleepfor': 'sleepfunc',
        'ticks': 'tickcountfunc',
        'milliseconds': 'millisecondcountfunc',
        'waitseconds': 'delayfunc',
        'waitsixtieths': 'delaysixtiethsfunc',
    },
    'date': {
        # date verbs in langverbs.c - extracted time components
        'hour': 'datehourfunc',
        'minute': 'dateminutefunc',
        'month': 'datemonthfunc',
        'year': 'dateyearfunc',
    },
    'rectangle': {
        # All rectangle verbs need full mapping
        'get': 'getrectfunc',
        'set': 'setrectfunc',
    },
    'speaker': {
        # RC verb names differ from C enum tokens
        'beep': 'sysbeepfunc',
        'sound': 'soundfunc',
        'playnamedsound': 'playsoundfunc',
    },
    'target': {
        # All target verbs need full mapping
        'get': 'gettargetfunc',
        'set': 'settargetfunc',
        'clear': 'cleartargetfunc',
    },
    'kb': {
        # Standard pattern works - all match {verb}func
        # But adding for completeness
        'optionkey': 'optionkeyfunc',
        'cmdkey': 'cmdkeyfunc',
        'shiftkey': 'shiftkeyfunc',
        'controlkey': 'controlkeyfunc',
    },
    'mouse': {
        # RC verb names need mouse prefix
        'button': 'mousebuttonfunc',
        'location': 'mouselocationfunc',
    },
    'point': {
        # All point verbs need full mapping
        'get': 'getpointfunc',
        'set': 'setpointfunc',
    },
    'rgb': {
        # All rgb verbs need full mapping
        'get': 'getrgbfunc',
        'set': 'setrgbfunc',
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
