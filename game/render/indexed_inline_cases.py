#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Synthetic inline-index caller cases, not game recordings.

Usage: indexed_inline_cases.py OUT_DIR
Large opaque-copy cases observe helper arguments only; they do not execute huge copies.
"""
import argparse
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from harness import PAGE, SANDBOX

ADDRESS = '0x014740a0'
CALLBACK = '0x014712b0'
QUERY = '0x01489c40'
COPY = '0x02fbe578'
PREFIX = 64
MASK32 = (1 << 32) - 1
MASK64 = (1 << 64) - 1
SOURCE = bytes((0x11 + i) & 0xff for i in range(PAGE))


def word(value):
    return struct.pack('<I', value & MASK32).hex()


def qword(value):
    return struct.pack('<Q', value & MASK64).hex()


def basic(cid, count=1, byte_count=3, flags=0, query=0, available=None, result=0, redirect=False):
    length = byte_count & MASK32
    words = ((length + 3) & MASK32) // 4
    requested = words + 10
    if available is None:
        available = requested
    opaque = length > 32
    callback = {'address': CALLBACK, 'argc': 3, 'argi': [0, 2], 'argu32': [1], 'ret': hex(result)}
    if redirect:
        callback['writes'] = [
            {'arg': 0, 'offset': 0, 'pointer': 'd_other', 'size': 8},
            {'arg': 0, 'offset': 8, 'pointer': 'd_other+96', 'size': 8},
            {'arg': 0, 'offset': 16, 'pointer': 'd_other+96', 'size': 8},
            {'arg': 0, 'offset': 40, 'bytes': word(flags ^ 3), 'size': 4},
        ]
    copy = {'address': COPY, 'argc': 3, 'argi': [0, 1, 2], 'ret': '0x0'}
    if length and not opaque:
        copy['argmem'] = [{'arg': 1, 'size': min(length, 16)}]
        copy['writes'] = [{'arg': 0, 'bytes': SOURCE[32:32 + length].hex(), 'size': length}]
    return {
        'schema': 1, 'address': ADDRESS, 'id': cid, 'returns': 'void',
        'args': {'rdi': f'buf:c_self+{PREFIX}', 'rsi': hex(count), 'rdx': 'buf:b_source+32', 'rcx': hex(byte_count)},
        'buffers': {name: {'size': PAGE, 'bytes': SOURCE.hex() if name == 'b_source' else 'a5' * PAGE}
                    for name in ('a_packet', 'b_source', 'c_self', 'd_other')},
        'memory': [
            {'addr': f'buf:c_self+{PREFIX}', 'pointer': 'a_packet'},
            {'addr': f'buf:c_self+{PREFIX + 8}', 'pointer': f'a_packet+{32 + available * 4}'},
            {'addr': f'buf:c_self+{PREFIX + 16}', 'pointer': 'a_packet+32'},
            {'addr': f'buf:c_self+{PREFIX + 24}', 'guest': CALLBACK},
            {'addr': f'buf:c_self+{PREFIX + 32}', 'bytes': qword(0x123456789abcdef0)},
            {'addr': f'buf:c_self+{PREFIX + 40}', 'bytes': word(flags)},
        ],
        'stubs': [callback, {'address': QUERY, 'argc': 0, 'ret': hex(query)}, copy],
    }


def self_packet(case, start):
    case['memory'][0] = {'addr': f'buf:c_self+{PREFIX}', 'pointer': 'c_self'}
    case['memory'][1] = {'addr': f'buf:c_self+{PREFIX + 8}', 'pointer': f'c_self+{PAGE}'}
    case['memory'][2] = {'addr': f'buf:c_self+{PREFIX + 16}', 'pointer': f'c_self+{start}'}
    return case


def cases():
    """Distinguish count/length, tag clearing, callback/query bytes and safe aliases."""
    for count in (0, 1, 7):
        for length in (0, 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17):
            yield basic(f'count_{count}_bytes_{length}', count, length)
    for flags in (0, 1, 2, 3, MASK32):
        for query in (0, 1):
            yield basic(f'flags_{flags:08x}_query_{query}', flags=flags, query=query)
    for query in (2, 0x80, 0x100, 0xabcdef0000000100, 0xabcdef0000000080):
        yield basic(f'query_low_byte_{query:016x}', query=query)
    for result in (0, 0x100, 0xabcdef0000000100, 1, 2, 0x80, 0xabcdef0000000080):
        yield basic(f'callback_{result:016x}', available=0, result=result, redirect=True, flags=2, query=1)
    for available in (10, 11, 12):
        yield basic(f'capacity_{available}', available=available)
    for length in (0x8000, 0x80000000, 0xfffffffc, 0xfffffffd, 0xfffffffe, 0xffffffff):
        yield basic(f'opaque_copy_contract_{length:08x}', count=0, byte_count=length, available=0, result=0x80, redirect=True)
    yield basic('upper_dword_input_bits', count=0xabcdef0000000000, byte_count=0xfedcba9800000001)
    # A first-packet initiator store clears cursor's low DWORD; the live ADD then uses a_packet.
    assert SANDBOX & MASK32 == 0
    yield self_packet(basic('first_live_cursor_add_overlap', byte_count=0), PREFIX - 4)
    for query in (0, 1):
        yield self_packet(basic(f'tail_12_query_{query}', byte_count=0, query=query), PREFIX + 12 - 24)
    yield self_packet(basic('tail_16_query_0', byte_count=0), PREFIX + 16 - 24)
    # Zeroing the last word after publication clears cursor's low DWORD in this arrangement.
    yield self_packet(basic('zero_overwrites_published_cursor', byte_count=0), PREFIX + 4 - 24)
    yield self_packet(basic('copy_overwrites_published_cursor', count=0, byte_count=1), PREFIX + 4 - 24)
    # Read the emitted header only when the helper is called, after all packet stores.
    case = basic('source_aliases_emitted_header', byte_count=4, flags=1)
    case['args']['rdx'] = 'buf:a_packet+32'
    case['stubs'][-1]['writes'] = [{'arg': 0, 'bytes': word(0xc0042701), 'size': 4}]
    yield case
    case = basic('source_reads_published_cursor', byte_count=8)
    case['args']['rdx'] = f'buf:c_self+{PREFIX + 16}'
    case['stubs'][-1]['writes'] = [{'arg': 0, 'bytes': qword(SANDBOX + 32 + 40 + 8), 'size': 8}]
    yield case
    case = basic('source_reads_zeroed_last_word', byte_count=1)
    case['args']['rdx'] = 'buf:a_packet+72'
    case['stubs'][-1]['writes'] = [{'arg': 0, 'bytes': '00', 'size': 1}]
    yield case
    # The source observes the narrow zeroing gate even with high-only count bits.
    yield basic('count_high_only_still_copies', count=0xabcdef0000000000, byte_count=3)
    yield self_packet(basic('packet_changes_shader_type_flag', byte_count=0, flags=2, query=1), PREFIX + 32)
    case = basic('unaligned_packet_cursor')
    case['memory'][1] = {'addr': f'buf:c_self+{PREFIX + 8}', 'pointer': 'a_packet+77'}
    case['memory'][2] = {'addr': f'buf:c_self+{PREFIX + 16}', 'pointer': 'a_packet+33'}
    yield case
    for available in (0, 10, 11):
        case = basic(f'capacity_dword_wrap_{available}')
        case['memory'][1] = {'addr': f'buf:c_self+{PREFIX + 8}', 'bytes': qword(SANDBOX + 32 + ((1 << 32) + available) * 4)}
        yield case


def main():
    """Generate unique fixtures and keep repeated in-place output deterministic."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('out', type=Path)
    args = parser.parse_args()
    generated = list(cases())
    if len({case['id'] for case in generated}) != len(generated):
        raise ValueError('duplicate fixture IDs')
    args.out.mkdir(parents=True, exist_ok=True)
    for case in generated:
        (args.out / (case['id'] + '.json')).write_text(json.dumps(case, indent=2) + '\n')
    print(f'render_draw_indexed_inline: {len(generated)} synthetic edge cases written')
    return 0


if __name__ == '__main__':
    sys.exit(main())
