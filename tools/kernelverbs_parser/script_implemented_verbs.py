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
    'html': {
        # Html processor verbs implemented in UserTalk
        # These are called by C helper functions during macro processing (see langhtml.c)
        # C helpers use langrunscript() to invoke UserTalk implementations
        # This is intentional design per dmb (5.0.2b14): "these type of lookups aren't that slow in UserTalk"
        'refglossary',        # html.refGlossary - glossary reference lookup
        'getonedirective',    # html.getOneDirective - directive extraction
        'normalizename',      # html.normalizeName - name normalization

        # Note: html.drawcalendar is GUI-dependent (QuickDraw), intentionally stubbed in headless mode
    },
    'script': {
        # Script processor verbs implemented in UserTalk glue scripts
        # See usertalk_scripts/Frontier.root/system/verbs/builtins/script/
        'uncompile',          # script.unCompile - decompile bytecode to source
        'getlanguage',        # script.getLanguage - get script language
        'setlanguage',        # script.setLanguage - set script language
        'makecomment',        # script.makeComment - comment out lines
        'uncomment',          # script.unComment - uncomment lines
        'iscomment',          # script.isComment - check if line is comment
        'getbreakpoint',      # script.getBreakpoint - get breakpoint info
        'setbreakpoint',      # script.setBreakpoint - set breakpoint
        'clearbreakpoint',    # script.clearBreakpoint - clear breakpoint
        'startprofile',       # script.startProfile - start profiling
        'stopprofile',        # script.stopProfile - stop profiling

        # Note: compile and removeSource also have glue but are implemented in C
    },
    'inetd': {
        # Inetd processor - internet daemon management (UserTalk-implemented)
        # See usertalk_scripts/Frontier.root/system/verbs/builtins/inetd/
        'supervisor',         # inetd.supervisor - daemon supervisor loop
    },
    'launch': {
        # Launch processor - application launching (UserTalk-implemented)
        # See usertalk_scripts/Frontier.root/system/verbs/builtins/launch/
        'applemenu',          # launch.appleMenu - launch from Apple menu
        'application',        # launch.application - launch application
        'appwithdocument',    # launch.appWithDocument - launch app with document
        'resource',           # launch.resource - launch resource
        'anything',           # launch.anything - launch any file
    },
    'search': {
        # Search processor - find/replace operations (UserTalk-implemented)
        # See usertalk_scripts/Frontier.root/system/verbs/builtins/search/
        'reset',              # search.reset - reset search state
        'findnext',           # search.findNext - find next occurrence
        'replace',            # search.replace - replace current match
        'replaceall',         # search.replaceAll - replace all occurrences
        'findtextdialog',     # search.findTextDialog - show find dialog
        'replacetextdialog',  # search.replaceTextDialog - show replace dialog
    },
    'tcp': {
        # TCP processor - TCP/IP networking (UserTalk-implemented)
        # See usertalk_scripts/Frontier.root/system/verbs/builtins/tcp/
        'addressdecode',      # tcp.addressDecode - decode IP address
        'addressencode',      # tcp.addressEncode - encode IP address
        'addresstoname',      # tcp.addressToName - resolve IP to hostname
        'nametoaddress',      # tcp.nameToAddress - resolve hostname to IP
        'myaddress',          # tcp.myAddress - get local IP address
        'abortstream',        # tcp.abortStream - abort connection
        'closestream',        # tcp.closeStream - close connection
        'closelisten',        # tcp.closeListen - close listen socket
        'openaddrstream',     # tcp.openAddrStream - open by IP address
        'opennamestream',     # tcp.openNameStream - open by hostname
        'readstream',         # tcp.readStream - read from stream
        'writestream',        # tcp.writeStream - write to stream
        'listenstream',       # tcp.listenStream - listen for connections
        'statusstream',       # tcp.statusStream - get connection status
        'getpeeraddress',     # tcp.getPeerAddress - get remote IP
        'getpeerport',        # tcp.getPeerPort - get remote port
        'writestringtostream', # tcp.writeStringToStream - write string
        'writefiletostream',  # tcp.writeFileToStream - write file
        'readstreamuntil',    # tcp.readStreamUntil - read until delimiter
        'readstreambytes',    # tcp.readStreamBytes - read N bytes
        'readstreamuntilclosed', # tcp.readStreamUntilClosed - read all
        'getstats',           # tcp.getStats - get connection statistics
        'countconnections',   # tcp.countConnections - count active connections
    },
    'webserver': {
        # Webserver processor - HTTP server (UserTalk-implemented)
        # See usertalk_scripts/Frontier.root/system/verbs/builtins/webserver/
        'server',             # webserver.server - main server loop
        'dispatch',           # webserver.dispatch - dispatch request
        'parseheaders',       # webserver.parseHeaders - parse HTTP headers
        'parsecookies',       # webserver.parseCookies - parse cookies
        'buildresponse',      # webserver.buildResponse - build HTTP response
        'builderrorpage',     # webserver.buildErrorPage - build error page
        'getserverstring',    # webserver.getServerString - get server ID string
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
