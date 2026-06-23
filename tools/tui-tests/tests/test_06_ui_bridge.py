"""
Phase 1 of the boxen <-> UserTalk UI bridge (see
planning/phase_c/BOXEN_USERTALK_UI_BRIDGE.md).

These tests drive UserTalk verbs that prompt for input (dialog.ask /
dialog.getString / dialog.getInt / dialog.getPassword) from inside the
boxen REPL and assert:

  - the prompt becomes visible WITHOUT the user having to press an
    unrelated key first (the "left-arrow makes it appear" symptom from
    the JES manual report on 2026-06-22);
  - typed input is reflected in the modal as it is entered;
  - Enter submits the value back to the script;
  - Esc / Ctrl-C cancels the modal (script receives a cancel return).

Pre-fix: every dialog verb dispatched from the boxen REPL is invisible
until dispatch completes (the GIL is held for the whole script and the
capture pipe never drains).  These tests reproduce that and verify the
fix.
"""

import time

from tui_harness import TUI, snapshot


def test_jump_menu_prompt_appears_immediately():
	"""/R J opens the Jump dialog, whose body calls dialog.ask to prompt
	for a destination table.  Pre-fix the prompt is invisible until the
	user touches an unrelated key; post-fix the modal renders the prompt
	text as soon as it opens.
	"""
	with TUI(boot_wait=1.5) as t:
		t.wait_for(">", timeout=5)
		t.send("/")
		time.sleep(0.6)
		t.wait_for("╔", timeout=2)

		# Open REPL menu (R), then Jump (J).  The Jump handler calls
		# dialog.ask("Jump to table:", @promptResult).
		t.send("R")
		time.sleep(0.3)
		t.send("J")
		# Without polling the user, the script blocks inside the dialog.
		# Give the modal time to draw.
		time.sleep(0.8)
		snapshot(t, "ui-bridge-jump-prompt-visible")

		text = t.capture()
		assert t.is_alive(), "REPL exited during Jump dispatch"
		assert "Jump to table" in text, (
			"Jump prompt was not visible without a follow-up keystroke. "
			"Pre-fix symptom: the prompt is buffered into the capture pipe "
			"but never presented because the GIL is held for the whole "
			"dispatch.  Capture:\n" + text
		)


def test_jump_dialog_esc_cancels_cleanly():
	"""Esc inside the Jump dialog cancels the prompt.  Script should
	receive a false return from dialog.ask, skip the jump, and the REPL
	should keep running.
	"""
	with TUI(boot_wait=1.5) as t:
		t.wait_for(">", timeout=5)
		t.send("/")
		time.sleep(0.6)
		t.wait_for("╔", timeout=2)
		t.send("R")
		time.sleep(0.3)
		t.send("J")
		time.sleep(0.6)
		# Cancel.
		t.send_key("Escape")
		time.sleep(0.5)
		snapshot(t, "ui-bridge-jump-esc-cancelled")
		assert t.is_alive(), "REPL exited when Esc cancelled the Jump dialog"
		# Modal box should be gone; prompt-line ">" visible again.
		t.wait_for(">", timeout=2)


def test_jump_dialog_typed_input_appears_in_modal():
	"""Type characters into the Jump dialog and confirm they render in
	the modal as the user types.  Pre-fix every echo lands in the capture
	pipe and only surfaces after the dialog returns.  Post-fix the modal
	owns its own rendering and shows characters live.
	"""
	with TUI(boot_wait=1.5) as t:
		t.wait_for(">", timeout=5)
		t.send("/")
		time.sleep(0.6)
		t.wait_for("╔", timeout=2)
		t.send("R")
		time.sleep(0.3)
		t.send("J")
		time.sleep(0.6)
		# Type a recognisable string and confirm it shows in the modal
		# BEFORE we hit Enter.
		t.send("workspace")
		time.sleep(0.3)
		snapshot(t, "ui-bridge-jump-typed-input")
		text = t.capture()
		assert t.is_alive(), "REPL exited while typing into Jump dialog"
		assert "workspace" in text, (
			"Characters typed into the dialog did not render live. "
			"Pre-fix: echoes go through the captured stderr pipe which "
			"only drains when the dispatch returns.  Capture:\n" + text
		)
		# Esc out so the test does not leave the REPL navigated somewhere
		# unexpected.
		t.send_key("Escape")
		time.sleep(0.3)


def test_jump_dialog_enter_submits_and_runs_script():
	"""End-to-end: open Jump dialog, type a target, press Enter, observe
	that the dispatched script ran to completion (the modal is gone and
	there is no '(menu script failed)' marker).  Phase 1 does not assert
	the script's effect (the navigation) because the test does not
	currently inspect REPL navigation state, but it does confirm the
	dispatch boundary closes cleanly when the dialog returns a value.
	"""
	with TUI(boot_wait=1.5) as t:
		t.wait_for(">", timeout=5)
		t.send("/")
		time.sleep(0.6)
		t.wait_for("╔", timeout=2)
		t.send("R")
		time.sleep(0.3)
		t.send("J")
		time.sleep(0.6)
		t.send("workspace")
		time.sleep(0.3)
		t.send_key("Enter")
		time.sleep(1.0)
		snapshot(t, "ui-bridge-jump-enter-roundtrip")

		text = t.capture()
		assert t.is_alive(), "REPL exited during Jump dispatch round-trip"
		assert "(menu script failed)" not in text, (
			"Script reported failure after the dialog returned a value. "
			"Capture:\n" + text
		)
		# Modal should be gone -- the box-drawing chars should no longer
		# dominate the screen.  Look for the REPL prompt at the bottom.
		t.wait_for(">", timeout=3)
