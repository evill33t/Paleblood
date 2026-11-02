# SPDX-License-Identifier: GPL-2.0-or-later
"""Owned AMD64 ELF startup only; no shipped game, SDK binary or guest capture."""
import base64
import hashlib
import json
import os
from pathlib import Path
import signal
import struct
import subprocess
import sys
import tempfile
import unittest

import ps4elf

ROOT = Path(__file__).resolve().parents[1]
FS0 = bytes.fromhex('64488b042500000000')


def nid(symbol):
    suffix = bytes.fromhex('518d64a635ded8c1e6b039b1c3e55230')
    digest = hashlib.sha1(symbol.encode('ascii') + suffix).digest()[:8][::-1]
    return base64.b64encode(digest).decode('ascii').rstrip('=').replace('/', '-')


def image(code, imports=(), tls=False):
    """Own segment plus optional checked TLS header placed in unused header padding."""
    data = bytes(code).ljust(0x300, b'\0')
    if tls:
        data = data[:0x100] + b'\x12\x34\x56\x78' + data[0x104:]
    result = bytearray(ps4elf.build([(0, data, 4096, 7)],
        [(nid(name) + '#A#A', ps4elf.STT_FUNC, None) for name in imports],
        [(0x280 + i * 8, ps4elf.R_X86_64_GLOB_DAT, i + 1, 0) for i in range(len(imports))],
        libraries=[(ps4elf.DT_SCE_IMPORT_LIB, 'libkernel', 0)] if imports else []))
    if tls:
        count = struct.unpack_from('<H', result, 56)[0]
        segment_offset = struct.unpack_from('<Q', result, 64 + 8)[0]
        assert 64 + 56 * (count + 1) <= segment_offset
        struct.pack_into('<IIQQQQQQ', result, 64 + 56 * count,
                         7, 4, segment_offset + 0x100, 0x100, 0x100, 4, 24, 16)
        struct.pack_into('<H', result, 56, count + 1)
    return bytes(result)


def import_call(code, index):
    code.extend(b'\xff\x15' + struct.pack('<i', 0x280 + index * 8 - (len(code) + 6)))


@unittest.skipUnless(sys.platform == 'darwin', 'Darwin owned boot integration')
class DarwinBoot(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pbboot = Path(os.environ.get('BB_PBBOOT', ROOT / 'build/runtime/pbboot'))
        if not cls.pbboot.is_file():
            raise RuntimeError('selected Darwin pbboot target is not built')
        cls.temporary = tempfile.TemporaryDirectory(prefix='pb-owned-darwin-')
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.folder = Path(cls.temporary.name)

    def boot(self, code, *args, imports=(), tls=False, module=None):
        folder = self.folder / self.id().split('.')[-1]
        folder.mkdir()
        executable = folder / 'owned.elf'
        executable.write_bytes(image(code, imports, tls))
        status = folder / 'status.json'
        command = [str(self.pbboot), str(executable), '--status', str(status), '--timeout', '1']
        if module is not None:
            path = folder / 'module.elf'
            path.write_bytes(image(module))
            command += ['--module', str(path)]
        result = subprocess.run([*command, *args], capture_output=True, text=True, timeout=8)
        return result, json.loads(status.read_text()) if status.exists() else None

    def clean(self, result, status, outcome):
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(status['outcome'], outcome)
        self.assertIn('Darwin main-thread TSD qualified;', result.stderr)
        self.assertIn('Darwin owned TLS and signal state released', result.stderr)

    def test_return_on_owned_stack(self):
        result, status = self.boot(b'\x48\x83\xec\x08\x48\xc7\x04\x24\x23\x01\0\0'
                                   b'\x48\x81\x3c\x24\x23\x01\0\0\x74\x02\x0f\x0b'
                                   b'\x48\x83\xc4\x08\xc3')
        self.clean(result, status, 'returned')

    def test_tls_rewrite_self_template_and_zero_fill(self):
        code = FS0 + b'\x48\x85\xc0\x75\x02\x0f\x0b'  # nonnull TCB
        code += b'\x48\x39\x00\x74\x02\x0f\x0b'  # self word
        code += b'\x81\x78\xe0\x12\x34\x56\x78\x74\x02\x0f\x0b'  # template at -32
        code += b'\x80\x78\xe4\0\x74\x02\x0f\x0b\xc3'  # first zero-fill byte
        result, status = self.boot(code, tls=True)
        self.clean(result, status, 'returned')
        self.assertIn('TLS patterns rewritten 1', result.stderr)

    def test_modules_precede_entry(self):
        module = b'\x48\xb8' + struct.pack('<Q', 0x800000200) + b'\xc6\x00\x5a\xc3'
        code = b'\x80\x3d' + struct.pack('<i', 0x200 - 7) + b'\x5a\x74\x02\x0f\x0b\xc3'
        result, status = self.boot(code, module=module)
        self.clean(result, status, 'returned')

    def test_fault_saved_pc(self):
        result, status = self.boot(b'\x0f\x0b')
        self.clean(result, status, 'fault')
        self.assertEqual(status['fault']['signal'], signal.SIGILL)
        self.assertEqual(status['fault']['at'], 'eboot+0x0')

    def test_timeout_cleans_tls_and_signal_state(self):
        result, status = self.boot(b'\xeb\xfe')
        self.clean(result, status, 'timeout')

    def test_guest_exit_callback(self):
        result, status = self.boot(b'\x48\x89\xf0\xbf\x11\0\0\0\x48\x83\xec\x08\xff\xd0\x0f\x0b')
        self.clean(result, status, 'exited')
        self.assertIn('exited with code 17', result.stdout)

    def test_unimplemented_import_unwinds_owned_scope(self):
        code = bytearray(b'\x48\x83\xec\x08')
        import_call(code, 0)
        code += b'\x48\x83\xc4\x08\xc3'
        result, status = self.boot(code, imports=('ownedUnimplemented',))
        self.clean(result, status, 'unimplemented')
        self.assertEqual(status['imports_called'], 1)

    def test_affinity_remains_named_unsupported(self):
        code = bytearray(b'\x53\x48\x83\xec\x10')
        import_call(code, 0)  # self
        code += b'\x48\x89\xc3\x48\x8d\x3c\x24'
        import_call(code, 1)  # AttrInit(&slot)
        code += b'\x48\x89\xdf\x48\x8d\x34\x24'
        import_call(code, 2)  # AttrGet(self, &slot)
        code += b'\x0f\x0b'
        result, status = self.boot(code, imports=('scePthreadSelf', 'scePthreadAttrInit', 'scePthreadAttrGet'))
        self.assertNotEqual(result.returncode, 0)
        self.assertIsNone(status)
        self.assertIn('scePthreadAttrGet: Darwin native CPU affinity backend is unsupported', result.stderr)

    def test_import_listing_has_no_tls_execution(self):
        result, status = self.boot(FS0 + b'\xc3', '--imports')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIsNone(status)
        self.assertNotIn('TSD qualified', result.stderr)


if __name__ == '__main__':
    unittest.main()
