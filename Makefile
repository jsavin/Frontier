# Top-level Makefile — thin proxies for the project's common workflows.
#
# The per-area Makefiles in tests/ and frontier-cli/ continue to own the
# real build logic. This file exists as a convergence point so contributors
# don't have to remember whether a target lives in `make -C tests` or as
# a ./tools/ script — they can just `make <target>` at the repo root.
#
# Targets:
#   build              Build frontier-cli.
#   unit               Run headless unit tests.
#   integration        Run integration tests.
#   verify-odb-sync    Verify Virgin.root and the .ut corpus stay in sync.
#   help               List available targets.

.PHONY: help build unit integration verify-odb-sync

help:
	@echo "Available targets:"
	@echo "  build             - Build frontier-cli (make -C frontier-cli)"
	@echo "  unit              - Run unit tests (./tools/run_headless_tests.sh)"
	@echo "  integration       - Run integration tests (make -C tests test-integration)"
	@echo "  verify-odb-sync   - Verify Virgin.root <-> .ut corpus sync (full walk)"

build:
	$(MAKE) -C frontier-cli

unit:
	./tools/run_headless_tests.sh

integration:
	$(MAKE) -C tests test-integration

verify-odb-sync:
	./tools/verify_virgin_root_sync.sh --full
