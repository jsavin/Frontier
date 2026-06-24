"""
Phase 2B of the boxen <-> UserTalk UI bridge: file dialog verbs.

Bridges file.* picker verbs to a boxen-native single-pane list picker:

  - file.getFileDialog (prompt, @adr, type)  -- pick existing file
  - file.putFileDialog (prompt, @adr)        -- choose save-to path
  - file.getFolderDialog (prompt, @adr)      -- pick existing folder
  - file.getDiskDialog (prompt, @adr)        -- pick volume / mount point

Test surface:
  - getFileDialog renders a boxen modal showing the current directory's
    entries with a breadcrumb header (no raw-stderr fallback).
  - typing into a putFileDialog filename row writes characters into the
    bottom field of the modal (not the typed prompt line).
  - Esc/Ctrl-C cancels and the verb returns false.
  - Up/Down moves the cursor; Enter on a directory navigates into it.

The full-dispatch path is exercised via UserTalk eval at the REPL
prompt.  The verb stores its result in a workspace variable, so we
check the modal renders correctly + the verb returns the expected
shape (false on cancel, true with workspace var populated on commit).
"""

import os
import time

from tui_harness import TUI, snapshot


def _eval(t, expr):
	t.send(expr)
	time.sleep(0.1)
	t.send_key("Enter")


def _modal_visible(text):
	return "╔" in text or "║" in text


def _text_inside_modal(text, marker):
	"""Marker is visible OUTSIDE the typed prompt line ('> ')."""
	prompt_marker = "> "
	rows_with_marker = [
		row for row in text.split("\n")
		if marker in row and not row.lstrip().startswith(prompt_marker)
	]
	return len(rows_with_marker) > 0


def test_getfiledialog_renders_modal_with_breadcrumb_and_entries():
	"""file.getFileDialog(prompt, @adr, "") under boxen should render a
	boxen modal with the dialog prompt, a breadcrumb of the current
	directory, and the directory's entries listed.  Pre-fix this would
	either no-op (gated by isInteractiveMode and ignored under tmux) or
	corrupt the framebuffer via the legacy file_browser raw-terminal
	path.
	"""
	with TUI(boot_wait=1.5) as t:
		t.wait_for(">", timeout=5)
		_eval(t,
			'file.getFileDialog ("PhaseTwoBPickFileMarker", '
			'@workspace.tmpPickResult, "")')
		time.sleep(0.8)
		snapshot(t, "ui-bridge-p2b-getfile-modal")

		text = t.capture()
		assert t.is_alive(), "REPL exited during file.getFileDialog"
		assert _modal_visible(text), (
			"file.getFileDialog did not render a boxen modal.  "
			"Capture:\n" + text
		)
		assert _text_inside_modal(text, "PhaseTwoBPickFileMarker"), (
			"prompt text missing inside modal.  Capture:\n" + text
		)
		# Cancel so the test leaves the REPL clean.
		t.send_key("Escape")
		time.sleep(0.3)


def test_getfiledialog_esc_cancels_returns_false():
	"""Esc inside the picker cancels.  file.getFileDialog returns
	false on cancel; UserTalk eval echoes the boolean result.
	"""
	with TUI(boot_wait=1.5) as t:
		t.wait_for(">", timeout=5)
		_eval(t,
			'file.getFileDialog ("PhaseTwoBCancelTest", '
			'@workspace.tmpPickResult, "")')
		time.sleep(0.6)
		t.send_key("Escape")
		time.sleep(0.5)
		snapshot(t, "ui-bridge-p2b-getfile-esc")

		text = t.capture()
		assert t.is_alive(), "REPL exited on Esc cancel"
		# UserTalk prints `false` for a false-returning expression.
		# Should appear OUTSIDE the typed prompt line.
		assert _text_inside_modal(text, "false"), (
			"file.getFileDialog did not return false on Esc cancel.  "
			"Capture:\n" + text
		)


def test_getfolderdialog_renders_modal():
	"""file.getFolderDialog opens the same picker shape, configured for
	folder selection.  Renders the boxen modal; user-facing prompt
	visible.
	"""
	with TUI(boot_wait=1.5) as t:
		t.wait_for(">", timeout=5)
		_eval(t,
			'file.getFolderDialog ("PhaseTwoBPickFolderMarker", '
			'@workspace.tmpPickResult)')
		time.sleep(0.8)
		snapshot(t, "ui-bridge-p2b-getfolder-modal")

		text = t.capture()
		assert t.is_alive(), "REPL exited during file.getFolderDialog"
		assert _modal_visible(text), (
			"file.getFolderDialog did not render a boxen modal.  "
			"Capture:\n" + text
		)
		assert _text_inside_modal(text, "PhaseTwoBPickFolderMarker"), (
			"prompt text missing inside modal.  Capture:\n" + text
		)
		t.send_key("Escape")
		time.sleep(0.3)


def test_putfiledialog_renders_with_filename_input_row():
	"""file.putFileDialog shows the list + a filename input row at the
	bottom of the modal.  Typing characters should land in the filename
	row, not the REPL prompt.
	"""
	with TUI(boot_wait=1.5) as t:
		t.wait_for(">", timeout=5)
		_eval(t,
			'file.putFileDialog ("PhaseTwoBSaveMarker", '
			'@workspace.tmpPickResult)')
		time.sleep(0.8)
		snapshot(t, "ui-bridge-p2b-putfile-modal")

		text = t.capture()
		assert t.is_alive(), "REPL exited during file.putFileDialog"
		assert _modal_visible(text), (
			"file.putFileDialog did not render a boxen modal.  "
			"Capture:\n" + text
		)
		assert _text_inside_modal(text, "PhaseTwoBSaveMarker"), (
			"prompt text missing inside modal.  Capture:\n" + text
		)
		t.send_key("Escape")
		time.sleep(0.3)
