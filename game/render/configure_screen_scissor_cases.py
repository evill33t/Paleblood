#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Synthetic edge cases; helper effects are scripted and are not game recordings.

Usage: configure_screen_scissor_cases.py OUT_DIR. Generate captures outside the repository.
"""
import json
from pathlib import Path
import struct
import sys

MASK64 = (1 << 64) - 1


def word(value):
    return struct.pack("<I", value & 0xFFFFFFFF).hex()


def qword(value):
    return struct.pack("<Q", value & MASK64).hex()


def memory(offset, value):
    return {"addr": f"buf:self+{offset}", "bytes": word(value)}


def basic(cid, returns="i64", size=512):
    return {
        "schema": 1,
        "address": ADDRESS,
        "id": cid,
        "returns": returns,
        "args": {"rdi": "buf:self"},
        "buffers": {"self": {"size": size, "bytes": "a5" * size}},
        "memory": [],
        "stubs": [],
    }


ADDRESS = "0x01476790"


def cases():
    values = [0, 1, 65535, 4294967295, 2143363909, 2147483648]
    modes = [
        ("direct", 80, 0, False),
        ("boundary", 4, 0, False),
        ("reject_zero", 0, 0, False),
        ("reject_high", 1, 0xABCDEF0000000100, False),
        ("accept_low_byte", 0, 0xABCDEF0000000001, False),
        ("accept_refresh_cursor", 0, 0x101, True),
    ]
    for value in values:
        c = packet_case(f"input_{value:016x}", value, "direct", 80, 0, False)
        yield c
    for label, available, result, redirect in modes[1:]:
        yield packet_case(label, values[-1], label, available, result, redirect)

    for offset in (12, 16):
        c = packet_case(f"output_overlaps_cursor_{offset}", 0x12345678, "direct", 80, 0, False)
        c["buffers"]["self"]["size"] = 512
        c["buffers"]["self"]["bytes"] = "a5" * 512
        c["memory"][0] = {"addr": "buf:self+8", "pointer": "self+512"}
        c["memory"][1] = {"addr": "buf:self+16", "pointer": f"self+{offset}"}
        yield c

    c = packet_case("callback_changes_command_begin_and_cursor", 3, "callback", 0, 0x80, True)
    c["stubs"][0].update({"argi": [0, 2], "argu32": [1]})
    c["stubs"][0]["writes"] = [
        {"arg": 0, "offset": 0, "pointer": "other", "size": 8},
        {"arg": 0, "offset": 16, "pointer": "other+64", "size": 8},
    ]
    yield c

    c = packet_case("cursor_publication_overwrites_bottom_right", 1, "direct", 80, 0, False)
    c["buffers"]["self"] = {"size": 512, "bytes": "a5" * 512}
    c["args"].update({"rdx": "0x2", "rcx": "0x9", "r8": "0x7"})
    c["memory"][0] = {"addr": "buf:self+8", "pointer": "self+512"}
    c["memory"][1] = {"addr": "buf:self+16", "pointer": "self+4"}
    c["memory"].append({"addr": "buf:self", "pointer": "self"})
    yield c

    # Clear Y independently so its high half cannot hide a missing X mask.
    c = packet_case("high_x_bits_with_zero_y", 0xFFFF0001, "direct", 80, 0, False)
    c["args"].update({"rdx": "0x0", "rcx": "0x80000002", "r8": "0x0"})
    yield c

    c = packet_case("signed_raw_coordinate_words", 0xFFFF8000, "direct", 80, 0, False)
    c["args"].update({"rdx": "0x8000ffff", "rcx": "0xaaaa4000", "r8": "0xbbbb4000"})
    yield c


def packet_case(cid, value, label, available, result, redirect):
    c = basic(cid, returns="void", size=64)
    c["buffers"]["packet"] = {"size": 512, "bytes": "6b" * 512}
    c["buffers"]["other"] = {"size": 512, "bytes": "7c" * 512}
    c["args"].update({"rdx": "0xa5a51234", "rcx": "0x89abcdef", "r8": "0x12345678"})
    c["args"]["rsi"] = hex(value)
    c["memory"] = [
        {"addr": "buf:self+8", "pointer": f"packet+{available*4}"},
        {"addr": "buf:self+16", "pointer": "packet"},
        {"addr": "buf:self+24", "guest": "0x014712b0"},
        {"addr": "buf:self+32", "bytes": qword(0x123456789ABCDEF0)},
    ]
    if available < 4:
        stub = {"address": "0x014712b0", "argc": 3, "ret": hex(result)}
        if redirect:
            stub["writes"] = [{"arg": 0, "offset": 16, "pointer": "other", "size": 8}]
        c["stubs"] = [stub]
    return c


def main():
    """Write deterministic synthetic fixtures for the screen-scissor writer."""
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    generated = list(cases())
    for entry in generated:
        (out / (entry["id"] + ".json")).write_text(json.dumps(entry, indent=2) + "\n")
    print(f"{len(generated)} synthetic edge cases written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
