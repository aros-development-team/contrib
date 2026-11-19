#!/usr/bin/env python3
"""Require positive native results, not just the enclosing Shell return code."""
import argparse
from pathlib import Path
import re

STAGES = {
    'Glyphs': ('FIDELITY_PASS',),
    'Palette': ('PALETTE256_PASS',),
    'Reflow': ('LINEREFLOW_PASS',),
    'Bindings': ('BINDINGS_NATIVE_PASS',),
    'Search': ('SEARCH_ASCII_PASS',),
    'Search-negated': ('SEARCH_ASCII_PASS',),
    'LargeCopy': ('LARGECOPY_PROTOCOL_PASS', 'LARGECOPY_CHILD_EXIT_PASS', 'OSCREAD_PASS'),
    **{s: ('SEARCH_ASCII_PASS',) for s in ('crlf', 'singleline', 'ascii', 'named')},
    'clear': ('CLEAR_SELECTION_PASS',),
    'wheel': ('WHEEL_NATIVE_PASS',),
    **{s: ('SEARCH_ASCII_PASS',) for s in ('special', 'value', 'sets')},
    'nav-bindings': ('NAV_BINDINGS_PASS',),
    'binding-modes': ('BINDING_MODES_PASS',),
    'selection-bindings': ('SELECTION_BINDINGS_PASS',),
    'motion-bindings': ('MOTION_BINDINGS_PASS',),
    'view-bindings': ('VIEW_BINDINGS_PASS',),
    'find-bindings': ('FIND_BINDINGS_PASS',),
    'search-bindings': ('SEARCH_BINDINGS_PASS',),
    'context-bindings': ('CONTEXT_BINDINGS_PASS',),
    'search-mode-bindings': ('SEARCHMODE_BINDINGS_PASS',),
    'mode-report': ('MODE_PASS',),
}

def check(text, baseline=False):
    expected = list(STAGES)[:24 if baseline else 26]
    errors, finished, body = [], [], []
    active = None
    done = False
    for line in text.splitlines():
        if 'BAD handler port' in line or re.search(r'\b\w+_FAIL\b', line):
            errors.append('native failure: ' + line)
        if line.startswith('INTEGRATION_BEGIN '):
            name = line.split(' ', 1)[1]
            if active or name not in expected or name in finished or done:
                errors.append('unexpected start: ' + name)
            if len(finished) < len(expected) and name != expected[len(finished)]:
                errors.append('stage order: ' + name)
            active, body = name, []
        elif line.startswith('INTEGRATION_END '):
            match = re.fullmatch(r'INTEGRATION_END (\S+) rc=(\d+)', line)
            if not match or match[1] != active:
                errors.append('unmatched end: ' + line)
                continue
            if match[2] != '0':
                errors.append('nonzero return: ' + line)
            for marker in STAGES.get(active, ()):
                if not any(re.match(re.escape(marker) + r'(?:\s|$)', x) for x in body):
                    errors.append(active + ': missing ' + marker)
            finished.append(active)
            active, body = None, []
        elif line == 'INTEGRATION_DONE':
            if done or active or finished != expected:
                errors.append('premature or duplicate completion')
            done = True
        elif active:
            body.append(line)
    if active or finished != expected or not done:
        errors.append('incomplete integration run')
    return errors

if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('log', type=Path)
    p.add_argument('--baseline-24', action='store_true', help='Validate the older 24-stage fixture')
    args = p.parse_args()
    failures = check(args.log.read_text(errors='replace'), args.baseline_24)
    for failure in failures:
        print(failure)
    if not failures:
        print('PASS: every native stage has its positive markers and zero return; no failure diagnostics')
    raise SystemExit(bool(failures))
