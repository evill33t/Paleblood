# SPDX-License-Identifier: GPL-2.0-or-later
"""Direct-memory CLI and registry calls from an owned synthetic AMD64 entry."""
import base64
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

import ps4elf

ROOT = Path(__file__).resolve().parents[1]
PAGE = 16384
CAPACITY = 8 * PAGE
SYMBOLS = ('sceKernelGetDirectMemorySize', 'sceKernelAllocateDirectMemory',
           'sceKernelMapDirectMemory', 'sceKernelReleaseDirectMemory')


def nid(symbol):
    suffix = bytes.fromhex('518d64a635ded8c1e6b039b1c3e55230')
    digest = hashlib.sha1(symbol.encode('ascii') + suffix).digest()[:8][::-1]
    return base64.b64encode(digest).decode('ascii').rstrip('=').replace('/', '-').replace('=', '')


def owned_entry(map_allocation):
    """Own instructions only: query, allocate, then map or release and return.

    The entry has an aligned call stack, two eight-byte output locals and no target bytes.
    A failed status or wrong configured capacity deliberately faults at an owned UD2.
    """
    code = bytearray(b'\x48\x83\xec\x28')  # SysV RSP+8 -> 16-byte call alignment
    fixups = []

    def call(index):
        code.extend(b'\xff\x15\0\0\0\0')
        fixups.append((len(code) - 4, index))

    def success():
        code.extend(b'\x85\xc0\x74\x02\x0f\x0b')  # TEST EAX; JE past UD2

    call(0)
    code.extend(b'\x48\x3d' + struct.pack('<I', CAPACITY) + b'\x74\x02\x0f\x0b')
    code.extend(b'\x48\x89\x04\x24')  # save capacity
    code.extend(b'\x31\xff\x48\x8b\x34\x24')  # start=0,end=capacity
    code.extend(b'\xba' + struct.pack('<I', 2 * PAGE))
    code.extend(b'\xb9' + struct.pack('<I', PAGE))
    code.extend(b'\x45\x31\xc0\x4c\x8d\x4c\x24\x08')  # type=0, &offset
    call(1)
    success()
    if map_allocation:
        code.extend(b'\x48\xc7\x44\x24\x10\0\0\0\0')  # address hint=NULL
        code.extend(b'\x48\x8d\x7c\x24\x10\xbe' + struct.pack('<I', 2 * PAGE))
        code.extend(b'\xba\x03\0\0\0\x31\xc9\x4c\x8b\x44\x24\x08')
        code.extend(b'\x41\xb9' + struct.pack('<I', PAGE))
        call(2)
        success()
        code.extend(b'\x48\x8b\x44\x24\x10\x48\x85\xc0\x75\x02\x0f\x0b')
        code.extend(b'\xc6\x00\x5a\xc6\x80' + struct.pack('<I', 2 * PAGE - 1) + b'\x6b')
    else:
        code.extend(b'\x48\x8b\x7c\x24\x08\xbe' + struct.pack('<I', 2 * PAGE))
        call(3)
        success()
    code.extend(b'\x48\x83\xc4\x28\xc3')
    got = 0x300
    for displacement, index in fixups:
        struct.pack_into('<i', code, displacement, got + index * 8 - (displacement + 4))
    image = bytes(code).ljust(got + 8 * len(SYMBOLS), b'\0')
    return ps4elf.build([(0, image, 4096, 7)],
                        [(nid(name) + '#A#A', ps4elf.STT_FUNC, None) for name in SYMBOLS],
                        [(got + 8 * i, ps4elf.R_X86_64_GLOB_DAT, i + 1, 0) for i in range(len(SYMBOLS))],
                        libraries=[(ps4elf.DT_SCE_IMPORT_LIB, 'libkernel', 0)])


class DirectMemoryBoot(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pbboot = Path(os.environ.get('BB_PBBOOT', ROOT / 'build/runtime/pbboot'))
        if not cls.pbboot.is_file():
            raise unittest.SkipTest('pbboot not built')
        cls.workspace = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.workspace.cleanup)
        cls.root = Path(cls.workspace.name)
        cls.mapped = cls.root / 'owned-map.elf'
        cls.released = cls.root / 'owned-release.elf'
        cls.mapped.write_bytes(owned_entry(True))
        cls.released.write_bytes(owned_entry(False))

    def boot(self, *arguments, image=None):
        status = self.root / (self.id().split('.')[-1] + '.json')
        status.unlink(missing_ok=True)
        result = subprocess.run([str(self.pbboot), str(image or self.mapped), '--status', str(status),
                                 '--timeout', '3', *arguments], capture_output=True, text=True, timeout=15)
        return result, json.loads(status.read_text()) if status.exists() else None

    def test_owned_query_allocate_map(self):
        result, status = self.boot('--direct-memory-size', str(CAPACITY))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(status['outcome'], 'returned')

    def test_owned_query_allocate_release(self):
        result, status = self.boot('--direct-memory-size', str(CAPACITY), image=self.released)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(status['outcome'], 'returned')

    def test_unconfigured_call_fails_by_name(self):
        result, status = self.boot()
        self.assertNotEqual(result.returncode, 0)
        self.assertIsNone(status)
        self.assertIn('sceKernelGetDirectMemorySize', result.stderr)
        self.assertIn('--direct-memory-size', result.stderr)

    def test_invalid_capacity(self):
        for value in ('0', '-16384', '+16384', '16385', '0x4000', '18446744073709551616',
                      '9223372036854775808', ' 16384', '16384 ', ''):
            with self.subTest(value=value):
                result, status = self.boot('--direct-memory-size', value)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIsNone(status)
                self.assertIn('--direct-memory-size', result.stderr)

    def test_duplicate_capacity(self):
        result, status = self.boot('--direct-memory-size', str(CAPACITY), '--direct-memory-size', str(CAPACITY))
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIsNone(status)

    def test_missing_capacity(self):
        result, status = self.boot('--direct-memory-size')
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIsNone(status)

    def test_listing_does_not_initialize_even_huge_backing(self):
        result, status = self.boot('--imports', '--direct-memory-size', '9223372036854759424')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIsNone(status)
        self.assertEqual(result.stdout.count('\truntime\t'), len(SYMBOLS))
        self.assertNotIn('cannot initialize', result.stderr)
