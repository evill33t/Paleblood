#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Synthetic edge cases; helper effects are scripted and are not game recordings.

Usage: configure_viewport_cases.py OUT_DIR. Generate captures outside the repository.
"""
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from harness import PAGE, SANDBOX

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


ADDRESS = "0x014765e0"


def cases():
    values = [0, 1, 4294967295, 2143363909]
    modes = [
        ("direct", 80, 0, False),
        ("boundary", 12, 0, False),
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

    # These float bit patterns encode cursor integers. Only the post-add output
    # addresses are dereferenced: self-8 plus16 becomes the mapped self+8.
    # Sorted other, packet and scale buffers each occupy a payload and guard page.
    self_address = SANDBOX + 6 * PAGE
    for cid, target in (
        ("first_packet_changes_cursor", self_address + 128),
        ("second_packet_overlaps_cursor", self_address - 8),
    ):
        c = packet_case(cid, 3, "direct", 80, 0, False)
        c["buffers"]["self"]["size"] = 512
        c["buffers"]["self"]["bytes"] = "a5" * 512
        c["memory"][0] = {"addr": "buf:self+8", "pointer": "self+512"}
        c["memory"][1] = {"addr": "buf:self+16", "pointer": "self+8"}
        c["args"]["xmm0"] = word(target & 0xFFFFFFFF)
        c["args"]["xmm1"] = word(target >> 32)
        yield c

    c = packet_case("callback_changes_storage_and_sources", 3, "callback", 0, 0x80, True)
    sources = [0x7FC12345, 0x80000000, 0x3F800000, 0xFFC54321, 0, 0xBF800000]
    changed = [0x7FA45678, 0x40000000, 0x80000000, 0x3F000000, 0xFFC98765, 0x40800000]
    c["buffers"]["sources"] = {"size": 24, "bytes": "".join(map(word, sources))}
    c["args"].update({"rdx": "buf:sources", "rcx": "buf:sources+12"})
    c["memory"][3] = {"addr": "buf:self+32", "pointer": "sources"}
    c["stubs"][0].update({"argi": [0, 2], "argu32": [1]})
    c["stubs"][0]["writes"] = [
        {"arg": 0, "offset": 0, "pointer": "other", "size": 8},
        {"arg": 0, "offset": 16, "pointer": "other+64", "size": 8},
        {"arg": 2, "offset": 0, "bytes": "".join(map(word, changed)), "size": 24},
    ]
    yield c

    c = packet_case("borrowed_triplets_overlap_packet_output", 3, "direct", 80, 0, False)
    c["args"].update({"rdx": "buf:packet", "rcx": "buf:packet+12"})
    c["memory"].append({"addr": "buf:packet", "bytes": "".join(map(word, sources))})
    yield c

    c = packet_case("packet_overwrites_command_begin", 3, "direct", 80, 0, False)
    c["buffers"]["self"] = {"size": 512, "bytes": "a5" * 512}
    c["memory"][0] = {"addr": "buf:self+8", "pointer": "self+512"}
    c["memory"][1] = {"addr": "buf:self+16", "pointer": "self"}
    c["memory"].append({"addr": "buf:self", "pointer": "self"})
    yield c


def packet_case(cid, value, label, available, result, redirect):
    c = basic(cid, returns="void", size=64)
    c["buffers"]["packet"] = {"size": 512, "bytes": "6b" * 512}
    c["buffers"]["other"] = {"size": 512, "bytes": "7c" * 512}
    c["args"].update({"r8": "0x89abcdef"})
    c["args"].update(
        {
            "rsi": hex(value),
            "rdx": "buf:scale",
            "rcx": "buf:translation",
            "xmm0": word(value),
            "xmm1": word(value ^ 0x80054321),
        }
    )
    c["buffers"].update(
        {
            "scale": {"size": 12, "bytes": word(0x7FC12345) + word(0x80000000) + word(0x3F800000)},
            "translation": {"size": 12, "bytes": word(0xFFC54321) + word(0) + word(0xBF800000)},
        }
    )
    c["memory"] = [
        {"addr": "buf:self+8", "pointer": f"packet+{available*4}"},
        {"addr": "buf:self+16", "pointer": "packet"},
        {"addr": "buf:self+24", "guest": "0x014712b0"},
        {"addr": "buf:self+32", "bytes": qword(0x123456789ABCDEF0)},
    ]
    if available < 12:
        stub = {"address": "0x014712b0", "argc": 3, "ret": hex(result)}
        if redirect:
            stub["writes"] = [{"arg": 0, "offset": 16, "pointer": "other", "size": 8}]
        c["stubs"] = [stub]
    return c


def main():
    """Write deterministic synthetic fixtures for the viewport writer."""
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
