"""
Phase 2A of the boxen <-> UserTalk UI bridge (see
planning/phase_c/BOXEN_USERTALK_UI_BRIDGE.md).

Bridges the remaining 4 dialog verbs to boxen modals:

  - dialog.alert(message)       -- beep + message + wait for Enter
  - dialog.notify(message)      -- message + wait for Enter (no beep)
  - dialog.twoway(prompt, b1, b2)    -- 2-button selector
  - dialog.threeway(prompt, b1, b2, b3)   -- 3-button selector

We drive these from a tiny ad-hoc UserTalk expression evaluated via the
REPL prompt rather than via a slash-menu dispatched script, because the
existing REPL menubar in Virgin.root only exercises dialog.getString
through /R J (Jump).  Pre-fix these prompts go to raw stderr and are
either invisible under boxen or corrupt the framebuffer.
"""

import time

from tui_harness import TUI, snapshot


def _eval(t, expr):
	"""Type a UserTalk expression at the REPL prompt and press Enter.
	The boxen REPL evaluates and shows the result in scrollback.
	"""
	t.send(expr)
	time.sleep(0.1)
	t.send_key("Enter")


def _modal_visible(text):
	"""A boxen modal renders with box-drawing characters around its
	content.  The legacy raw-stderr path doesn't, so the presence of
	the top-left corner glyph plus the modal's content is a reliable
	"the bridge ran" signal."""
	return "╔" in text or "║" in text


def _text_inside_modal(text, marker):
	"""Marker is visible OUTSIDE the typed prompt line.  The capture
	puts the typed input on a line starting with "> ".  We want to see
	the marker on a row that ISN'T the prompt row."""
	prompt_marker = "> "
	rows_with_marker = [
		row for row in text.split("\n")
		if marker in row and not row.lstrip().startswith(prompt_marker)
	]
	return len(rows_with_marker) > 0


def test_dialog_notify_renders_modal_immediately():
	"""dialog.notify("hello") should open a centered boxen modal
	showing "hello" and a "Press Enter to continue" hint.  Pre-fix the
	message went to captured stderr but the framebuffer wasn't
	repainted, so the only thing visible was raw escape sequences
	leaking into the scrollback after the dialog returned.
	"""
	with TUI(boot_wait=1.5) as t:
		t.wait_for(">", timeout=5)
		_eval(t, 'dialog.notify ("PhaseTwoNotifyMarker")')
		time.sleep(0.6)
		snapshot(t, "ui-bridge-p2-notify-visible")
		text = t.capture()
		assert t.is_alive(), "REPL exited during dialog.notify"
		assert _modal_visible(text), (
			"dialog.notify did not render a boxen modal.  The legacy "
			"raw-stderr path likely ran instead.  Capture:\n" + text
		)
		assert _text_inside_modal(text, "PhaseTwoNotifyMarker"), (
			"dialog.notify message not visible inside the modal (only "
			"appears on the typed prompt line, if at all).  Capture:\n"
			+ text
		)
		# Dismiss with Enter so the test leaves the REPL clean.
		t.send_key("Enter")
		time.sleep(0.3)


def test_dialog_alert_renders_modal_and_dismisses_on_enter():
	"""dialog.alert("oops") should open a modal that the user dismisses
	with Enter.  Beep (bell character) is sent via boxen rather than
	stderr; we can't easily assert that, but we can assert the modal
	renders and the REPL stays alive.
	"""
	with TUI(boot_wait=1.5) as t:
		t.wait_for(">", timeout=5)
		_eval(t, 'dialog.alert ("PhaseTwoAlertMarker")')
		time.sleep(0.6)
		snapshot(t, "ui-bridge-p2-alert-visible")
		text = t.capture()
		assert t.is_alive(), "REPL exited during dialog.alert"
		assert _modal_visible(text), (
			"dialog.alert did not render a boxen modal.  Capture:\n" + text
		)
		assert _text_inside_modal(text, "PhaseTwoAlertMarker"), (
			"dialog.alert message not visible inside the modal.  "
			"Capture:\n" + text
		)
		t.send_key("Enter")
		time.sleep(0.3)
		# After dismissal, the REPL prompt should be reachable.
		t.wait_for(">", timeout=2)


def test_dialog_twoway_returns_first_button_via_left_arrow_enter():
	"""dialog.twoway("Save?", "Save", "Discard") with Left+Enter selects
	the first button (Save) and returns true (UserTalk: equivalent to
	1).  We confirm via the result printed to scrollback after the
	dialog dismisses.
	"""
	with TUI(boot_wait=1.5) as t:
		t.wait_for(">", timeout=5)
		_eval(t, 'dialog.twoway ("PhaseTwoTwowayQuestion", "Save", "Discard")')
		time.sleep(0.6)
		snapshot(t, "ui-bridge-p2-twoway-prompt")
		text = t.capture()
		assert t.is_alive(), "REPL exited during dialog.twoway"
		assert _modal_visible(text), (
			"dialog.twoway did not render a boxen modal.  Capture:\n"
			+ text
		)
		assert _text_inside_modal(text, "PhaseTwoTwowayQuestion"), (
			"twoway prompt not visible inside the modal.  Capture:\n"
			+ text
		)
		# Both button labels should be on screen inside the modal.
		assert _text_inside_modal(text, "Save") and _text_inside_modal(text, "Discard"), (
			"twoway button labels not visible inside the modal.  "
			"Capture:\n" + text
		)
		# Pick the first button: arrow-left (or stay on default 0) + Enter.
		t.send_key("Left")
		time.sleep(0.1)
		t.send_key("Enter")
		time.sleep(0.4)
		snapshot(t, "ui-bridge-p2-twoway-after-enter")
		# UserTalk prints `true` for a true-returning expression.
		text2 = t.capture()
		assert t.is_alive(), "REPL exited after twoway returned"
		assert "true" in text2, (
			"dialog.twoway did not echo a true result after first-button "
			"selection.  Capture:\n" + text2
		)


def test_dialog_threeway_returns_2_via_right_then_enter():
	"""dialog.threeway returns the 1-based index of the chosen button.
	Default selection starts on button 1; one Right arrow moves to
	button 2; Enter returns 2.
	"""
	with TUI(boot_wait=1.5) as t:
		t.wait_for(">", timeout=5)
		_eval(t, 'dialog.threeway ("PhaseTwoThreewayQuestion", "Yes", "No", "Cancel")')
		time.sleep(0.6)
		text = t.capture()
		assert t.is_alive(), "REPL exited during dialog.threeway"
		assert _modal_visible(text), (
			"dialog.threeway did not render a boxen modal.  Capture:\n"
			+ text
		)
		assert _text_inside_modal(text, "PhaseTwoThreewayQuestion"), (
			"threeway prompt not visible inside the modal.  Capture:\n"
			+ text
		)
		assert (
			_text_inside_modal(text, "Yes")
			and _text_inside_modal(text, "No")
			and _text_inside_modal(text, "Cancel")
		), (
			"threeway button labels not visible inside the modal.  "
			"Capture:\n" + text
		)
		# Move from default (button 1, "Yes") to button 2 ("No").
		t.send_key("Right")
		time.sleep(0.1)
		t.send_key("Enter")
		time.sleep(0.4)
		snapshot(t, "ui-bridge-p2-threeway-after-enter")
		text2 = t.capture()
		assert t.is_alive(), "REPL exited after threeway returned"
		# threeway returns 2 (the integer index of the chosen button).
		assert "2" in text2, (
			"dialog.threeway did not echo 2 after second-button selection.  "
			"Capture:\n" + text2
		)
