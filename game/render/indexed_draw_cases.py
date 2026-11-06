#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Synthetic indexed-draw edges; query scripts have no invented pointer arguments.

Usage: indexed_draw_cases.py OUT_DIR [--function FUNCTION]
Default selection is the output directory basename used by the native packager.
"""
import argparse
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from harness import PAGE, SANDBOX

FUNCTIONS = {'render_draw_indexed': ('0x01474360', 8, 24),
             'render_draw_indexed_offset': ('0x01474420', 7, 20)}
PREFIX = 64
CALLBACK = '0x014712b0'
QUERY = '0x01489c40'
MASK64 = (1 << 64) - 1


def qword(value):
    return struct.pack('<Q', value & MASK64).hex()


def word(value):
    return struct.pack('<I', value & 0xffffffff).hex()


def basic(function, cid, first=None, second=None, flags=0, query=0, available=None, result=0, redirect=False):
    address, requested, _ = FUNCTIONS[function]
    relative = function.endswith("_offset")
    if first is None:
        first = 7 if relative else 3
    if second is None:
        second = 3 if relative else 0xabcdef1212345679
    if available is None:
        available = requested
    callback = {'address': CALLBACK, 'argc': 3, 'argi': [0, 2], 'argu32': [1], 'ret': hex(result)}
    if redirect:
        callback['writes'] = [
            {'arg': 0, 'offset': 0, 'pointer': 'b_other', 'size': 8},
            {'arg': 0, 'offset': 8, 'pointer': 'b_other+96', 'size': 8},
            {'arg': 0, 'offset': 16, 'pointer': 'b_other+96', 'size': 8},
            {'arg': 0, 'offset': 40, 'bytes': word(flags ^ 3), 'size': 4},
        ]
    return {
        'schema': 1, 'address': address, 'id': cid, 'returns': 'void',
        'args': {'rdi': f'buf:c_self+{PREFIX}', 'rsi': hex(first), 'rdx': hex(second)},
        'buffers': {name: {'size': PAGE, 'bytes': 'a5' * PAGE} for name in ('a_packet', 'b_other', 'c_self')},
        'memory': [
            {'addr': f'buf:c_self+{PREFIX}', 'pointer': 'a_packet'},
            {'addr': f'buf:c_self+{PREFIX + 8}', 'pointer': f'a_packet+{32 + available * 4}'},
            {'addr': f'buf:c_self+{PREFIX + 16}', 'pointer': 'a_packet+32'},
            {'addr': f'buf:c_self+{PREFIX + 24}', 'guest': CALLBACK},
            {'addr': f'buf:c_self+{PREFIX + 32}', 'bytes': qword(0x123456789abcdef0)},
            {'addr': f'buf:c_self+{PREFIX + 40}', 'bytes': word(flags)},
        ],
        # Both calls have observing stubs even on branches that skip them.
        'stubs': [callback, {'address': QUERY, 'argc': 0, 'ret': hex(query)}],
    }


def with_self_packet(case, start):
    case['memory'][0] = {'addr': f'buf:c_self+{PREFIX}', 'pointer': 'c_self'}
    case['memory'][1] = {'addr': f'buf:c_self+{PREFIX + 8}', 'pointer': f'c_self+{PAGE}'}
    case['memory'][2] = {'addr': f'buf:c_self+{PREFIX + 16}', 'pointer': f'c_self+{start}'}
    return case


def cases(function):
    """Vary independent payload fields, both flags, widths and safe publication aliases."""
    _, requested, first_bytes = FUNCTIONS[function]
    relative = function.endswith('_offset')
    inputs = [(0, 0), (1, 0), (0, 1), (7, 3), (0xffffffff, 0x80000000),
              (0xabcdef0187654321, 0xfedcba9876543210)] if relative else [
              (0, 0), (1, 0), (1, 1), (1, 2), (0x80000000, 0x100000000),
              (0xffffffff, 0x10000000000), (3, 0xabcdef1212345678), (3, 0xabcdef1212345679),
              (0xabcdef0187654321, MASK64)]
    for first, second in inputs:
        yield basic(function, f'payload_{first:016x}_{second:016x}', first, second)
    for flags in (0, 1, 2, 3, 0xffffffff):
        for query in (0, 1):
            yield basic(function, f'flags_{flags:08x}_query_{query}', flags=flags, query=query)
    for query in (2, 0x80, 0x100, 0xabcdef0000000100, 0xabcdef0000000080):
        yield basic(function, f'query_low_byte_{query:016x}', query=query)
    for available in (requested - 1, requested, requested + 1):
        yield basic(function, f'capacity_{available}', available=available)
    for result in (0, 0x100, 0xabcdef0000000100, 1, 2, 0x80, 0xabcdef0000000080):
        yield basic(function, f'callback_{result:016x}', available=0, result=result, redirect=True, flags=2, query=1)
    # The low DWORD is zero in the first owned allocation. Clearing only cursor's low
    # DWORD redirects its live ADD into a_packet, rather than unmapped storage.
    assert SANDBOX & 0xffffffff == 0
    alias_start = PREFIX if relative else PREFIX - 4
    yield with_self_packet(basic(function, 'first_live_cursor_add_overlap'), alias_start)
    for tail_start in (12, 16):
        for query in (0, 1):
            start = PREFIX + tail_start - first_bytes
            yield with_self_packet(basic(function, f'tail_{tail_start}_query_{query}', query=query), start)
    first, second = (2, 3) if relative else (3, 0xabcdef121234567a)
    yield with_self_packet(basic(function, 'packet_changes_shader_type_flag', first, second, query=1), PREFIX + 32)
    cursor = SANDBOX + 32
    for available in (0, requested - 1, requested, requested + 1):
        case = basic(function, f'capacity_dword_wrap_{available}')
        case['memory'][1] = {'addr': f'buf:c_self+{PREFIX + 8}', 'bytes': qword(cursor + ((1 << 32) + available) * 4)}
        yield case
    case = basic(function, 'capacity_address_subtract_wrap')
    case['memory'][1] = {'addr': f'buf:c_self+{PREFIX + 8}', 'bytes': qword(cursor - 4)}
    yield case


def main():
    """Generate the selected function deterministically into one directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('out', type=Path)
    parser.add_argument('--function', choices=FUNCTIONS)
    args = parser.parse_args()
    function = args.function or args.out.name
    if function not in FUNCTIONS:
        parser.error('use a function-named output directory or --function')
    generated = list(cases(function))
    if len({case['id'] for case in generated}) != len(generated):
        raise ValueError('duplicate fixture IDs')
    args.out.mkdir(parents=True, exist_ok=True)
    for case in generated:
        (args.out / (case['id'] + '.json')).write_text(json.dumps(case, indent=2) + '\n')
    print(f'{function}: {len(generated)} synthetic edge cases written')
    return 0


if __name__ == '__main__':
    sys.exit(main())
