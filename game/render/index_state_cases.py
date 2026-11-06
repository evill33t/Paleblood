#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Synthetic INDEX_TYPE/BASE/BUFFER_SIZE edges, never genuine game recordings.

Usage: index_state_cases.py OUT_DIR [--function FUNCTION]
The default function is the output directory basename, as used by the verification packager.
"""
import argparse
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from harness import PAGE, SANDBOX

FUNCTIONS = {
    'render_set_index_type': ('0x01475270', 2),
    'render_set_index_base': ('0x014752c0', 3),
    'render_set_index_buffer_size': ('0x01475320', 2),
}
CALLBACK = '0x014712b0'
MASK64 = (1 << 64) - 1
BUFFER_NAMES = ('a_packet', 'b_other', 'c_self')


def qword(value):
    return struct.pack('<Q', value & MASK64).hex()


def packet_address(name, offset=0):
    """Match the harness's sorted one-payload-page plus guard-page allocation."""
    return SANDBOX + BUFFER_NAMES.index(name) * 2 * PAGE + offset


def basic(function, cid, value, available=None, result=0, redirect=False, prefix=0, packet_offset=32):
    address, requested = FUNCTIONS[function]
    if available is None:
        available = requested
    case = {
        'schema': 1, 'address': address, 'id': cid, 'returns': 'void',
        'args': {'rdi': f'buf:c_self+{prefix}', 'rsi': hex(value)},
        'buffers': {name: {'size': PAGE, 'bytes': 'a5' * PAGE} for name in BUFFER_NAMES},
        'memory': [
            {'addr': f'buf:c_self+{prefix}', 'pointer': 'a_packet'},
            {'addr': f'buf:c_self+{prefix + 8}', 'pointer': f'a_packet+{packet_offset + available * 4}'},
            {'addr': f'buf:c_self+{prefix + 16}', 'pointer': f'a_packet+{packet_offset}'},
            {'addr': f'buf:c_self+{prefix + 24}', 'guest': CALLBACK},
            {'addr': f'buf:c_self+{prefix + 32}', 'bytes': qword(0x123456789abcdef0)},
        ], 'stubs': [],
    }
    if available < requested:
        stub = {'address': CALLBACK, 'argc': 3, 'argi': [0, 2], 'argu32': [1], 'ret': hex(result)}
        if redirect:
            stub['writes'] = [
                {'arg': 0, 'offset': 0, 'pointer': 'b_other', 'size': 8},
                {'arg': 0, 'offset': 8, 'pointer': 'b_other+96', 'size': 8},
                {'arg': 0, 'offset': 16, 'pointer': 'b_other+96', 'size': 8},
            ]
        case['stubs'] = [stub]
    return case


def cases(function):
    """Distinguish widths, both masks, capacity truncation, callbacks and aliases."""
    _, requested = FUNCTIONS[function]
    words = (0, 1, 2, 0x80, 0x100, 0x80000000, 0xffffffff, 0xabcdef0180010003)
    addresses = (0, 1, 2, 0x100, 0x100000000, 0xff00000000, 0x10000000000,
                 0xabcd12ef12345678, 0xabcd12ef12345679, MASK64)
    values = addresses if function == 'render_set_index_base' else words
    for value in values:
        yield basic(function, f'value_{value:016x}', value)
    for available in (requested - 1, requested, requested + 1):
        yield basic(function, f'capacity_{available}', values[-1], available=available, result=0)
    for result in (0, 0x100, 0xabcdef0000000100, 1, 2, 0x80, 0xabcdef0000000080):
        yield basic(function, f'callback_{result:016x}', values[-1], available=0, result=result, redirect=True)
    for overlap in (0, 4, 8, 12, 16, 20):
        case = basic(function, f'packet_prefix_overlap_{overlap}', values[-1])
        case['memory'][0] = {'addr': 'buf:c_self', 'pointer': 'c_self'}
        case['memory'][1] = {'addr': 'buf:c_self+8', 'pointer': 'c_self+128'}
        case['memory'][2] = {'addr': 'buf:c_self+16', 'pointer': f'c_self+{overlap}'}
        yield case
    yield basic(function, 'unaligned_prefix_and_packet', values[-1], prefix=1, packet_offset=33)
    # The address subtraction wraps at 64 bits; the word quotient is then truncated to DWORD.
    cursor = packet_address('a_packet', 32)
    for words_available in (0, requested - 1, requested, requested + 1):
        case = basic(function, f'capacity_dword_wrap_{words_available}', values[-1])
        wide_bytes = ((1 << 32) + words_available) * 4
        case['memory'][1] = {'addr': 'buf:c_self+8', 'bytes': qword(cursor + wide_bytes)}
        # Provide an observing callback even when the correct path should skip it.
        case['stubs'] = [{'address': CALLBACK, 'argc': 3, 'argi': [0, 2], 'argu32': [1], 'ret': '0x0'}]
        yield case
    case = basic(function, 'capacity_address_subtract_wrap', values[-1])
    case['memory'][1] = {'addr': 'buf:c_self+8', 'bytes': qword(cursor - 4)}
    case['stubs'] = [{'address': CALLBACK, 'argc': 3, 'argi': [0, 2], 'argu32': [1], 'ret': '0x0'}]
    yield case


def main():
    """Generate each unique fixture without stale files or a second schema sidecar."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('out', type=Path)
    parser.add_argument('--function', choices=FUNCTIONS)
    args = parser.parse_args()
    function = args.function or args.out.name
    if function not in FUNCTIONS:
        parser.error('use a function-named output directory or --function')
    generated = list(cases(function))
    ids = [case['id'] for case in generated]
    if len(ids) != len(set(ids)):
        raise ValueError('duplicate case IDs')
    args.out.mkdir(parents=True, exist_ok=True)
    for case in generated:
        (args.out / (case['id'] + '.json')).write_text(json.dumps(case, indent=2) + '\n')
    print(f'{function}: {len(generated)} synthetic edge cases written')
    return 0


if __name__ == '__main__':
    sys.exit(main())
