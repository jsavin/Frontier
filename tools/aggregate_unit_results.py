#!/usr/bin/env python3
"""
Aggregate per-executable unit test JSON results into tests/tmp/unit/last_run.json.

Called by `make -C tests test` after all test executables complete.
"""

import json
import glob
import os
from datetime import datetime, timezone


def main():
    results = []
    total = passed = failed = 0

    for f in sorted(glob.glob('tmp/unit/*.json')):
        # Skip the aggregate file itself
        if os.path.basename(f) == 'last_run.json':
            continue
        try:
            with open(f) as fh:
                d = json.load(fh)
            results.append(d)
            total += d.get('total', 0)
            passed += d.get('passed', 0)
            failed += d.get('failed', 0)
        except (json.JSONDecodeError, OSError):
            pass

    aggregate = {
        'timestamp': datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'),
        'executables': len(results),
        'total': total,
        'passed': passed,
        'failed': failed,
        'results': results,
    }

    with open('tmp/unit/last_run.json', 'w') as fh:
        json.dump(aggregate, fh, indent=2)

    print(f'[test_report] Aggregate: {total} tests, {passed} passed, '
          f'{failed} failed across {len(results)} executables')


if __name__ == '__main__':
    main()
