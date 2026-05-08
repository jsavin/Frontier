# Shared headless verb file list for frontier-cli and tests
# This file ensures both Makefiles stay in sync when headless verb files are added/removed.
#
# Usage:
#   frontier-cli/Makefile: Uses $(TESTSDIR)/$(file) for each file (verb files remain in tests/)
#   tests/Makefile: Uses $(file) directly (files are in same directory)
# Note: Runtime stubs (headless_*_stubs.c) have moved to frontier-cli/stubs/.
#
# When adding new headless verb files:
#   1. Add the filename to HEADLESS_VERBS_SOURCES below
#   2. Both frontier-cli and tests Makefiles will automatically pick it up
#   3. No need to modify frontier-cli/Makefile or tests/Makefile directly
#
# To exclude a processor from registration (e.g., script-implemented processors),
# add it to EXCLUDED_PROCESSORS in tools/kernelverbs_parser/parse_kernelverbs.py

HEADLESS_VERBS_SOURCES = \
    headless_lang_verbs.c \
    headless_frontier_verbs.c \
    headless_file_verbs.c \
    headless_table_verbs.c \
    headless_odb_stubs.c \
    headless_threadglobals.c \
    headless_op_verbs.c \
    headless_opattributes_verbs.c \
    headless_script_verbs.c \
    headless_osa_verbs.c \
    headless_pict_verbs.c \
    headless_clock_verbs.c \
    headless_date_verbs.c \
    headless_dialog_verbs.c \
    headless_kb_verbs.c \
    headless_mouse_verbs.c \
    headless_point_verbs.c \
    headless_rectangle_verbs.c \
    headless_rgb_verbs.c \
    headless_speaker_verbs.c \
    headless_target_verbs.c \
    headless_bit_verbs.c \
    headless_semaphore_verbs.c \
    headless_base64_verbs.c \
    headless_tcp_verbs.c \
    headless_dll_verbs.c \
    headless_python_verbs.c \
    headless_htmlcontrol_verbs.c \
    headless_statusbar_verbs.c \
    headless_sys_verbs.c \
    headless_string_verbs.c \
    headless_db_verbs.c \
    headless_html_verbs.c \
    headless_xml_verbs.c \
    headless_re_verbs.c \
    headless_sqlite_verbs.c \
    headless_mysql_verbs.c \
    headless_window_verbs.c \
    headless_rez_verbs.c \
    headless_search_verbs.c \
    headless_filemenu_verbs.c \
    headless_editmenu_verbs.c \
    headless_launch_verbs.c \
    headless_clipboard_verbs.c \
    headless_mainwindow_verbs.c \
    headless_searchengine_verbs.c \
    headless_mrcalendar_verbs.c \
    headless_webserver_verbs.c \
    headless_inetd_verbs.c \
    headless_wp_verbs.c
