# TUI test runner — drives `dist/frontier-cli --debug-tui` via tmux.
#
# Usage:
#   make           # build CLI if stale, then run all tui-tests
#   make tests     # run tests only (assumes dist/frontier-cli is current)
#   make rebuild   # force rebuild then run
#   make clean     # remove captured artifacts
#
# Skippable layer per the /auto Test Manifest in the project's CLAUDE.md:
# requires `tmux` (mandatory) and `aha` (optional, for HTML snapshots).

REPO_ROOT := $(shell git -C $(CURDIR) rev-parse --show-toplevel)
CLI       := $(REPO_ROOT)/dist/frontier-cli
PYTHON    := python3

.PHONY: all tests rebuild clean check-tmux check-aha

all: $(CLI) tests

$(CLI):
	$(MAKE) -C $(REPO_ROOT)/frontier-cli

rebuild:
	$(MAKE) -C $(REPO_ROOT)/frontier-cli
	$(MAKE) tests

tests: check-tmux
	@$(PYTHON) $(CURDIR)/tui_harness.py

check-tmux:
	@which tmux > /dev/null || (echo "FATAL: tmux not in PATH (brew install tmux)" && exit 1)

check-aha:
	@which aha > /dev/null || echo "NOTE: aha not installed; HTML snapshots disabled (brew install aha to enable)"

clean:
	rm -rf $(CURDIR)/artifacts
	mkdir -p $(CURDIR)/artifacts
