# SPDX-License-Identifier: GPL-2.0-or-later
"""Owned AMD64 providers return the identity observed by the launching parent."""
import json
import os
from pathlib import Path
import signal
import struct
import subprocess
import tempfile
import unittest

import ps4elf
from test_module_tls_boot import Code, symbol

ROOT = Path(__file__).resolve().parents[1]
MARKER = b'owned process identity\0'


def image(module=False, pid_name='getpid', pid_library='libkernel'):
    c = Code()
    c.emit('5341544155')  # Preserve rbx/r12/r13; align each guest call.
    c.relative('ff15', 0x1010)  # __error
    c.emit('4989c541c7450044332211')  # r13 = cell; seed all four bytes.
    c.relative('ff15', 0x1000)
    c.emit('4889c3')  # Keep the complete provider result, without asserting RDX.
    c.relative('488905', 0x1200 + len(MARKER))
    c.relative('ff15', 0x1008)  # The second, independently named namespace.
    c.emit('4839d8'); c.require_equal()
    c.emit('41817d0044332211'); c.require_equal()
    c.relative('488d3d', 0x1100)
    c.emit('be0100000031d2')
    c.relative('ff15', 0x1018)  # Exact owned /dev/console output path.
    c.emit('83f803'); c.require_equal()
    c.emit('4189c44489e7')  # Save the guest FD in r12d, pass it to _write.
    c.relative('488d35', 0x1200)
    c.emit('ba' + struct.pack('<I', len(MARKER) + 8).hex())
    c.relative('ff15', 0x1020)
    c.emit('4883f8' + f'{len(MARKER) + 8:02x}'); c.require_equal()
    c.emit('4489e7'); c.relative('ff15', 0x1028)
    c.emit('4883f800'); c.require_equal()
    c.emit('41817d0044332211'); c.require_equal()
    c.emit('31c0415d415c5bc3')
    page = bytearray(0x1000)
    code = c.finish(); page[:len(code)] = code
    data = bytearray(0x400)
    data[0x100:0x10d] = b'/dev/console\0'
    data[0x200:0x200 + len(MARKER)] = MARKER
    first_library = 'A' if pid_library == 'libkernel' else 'C'
    names = [(pid_name, first_library), ('getpid', 'B'), ('__error', 'A'),
             ('_open', 'A'), ('_write', 'A'), ('_close', 'A')]
    imports = [(symbol(name, library), ps4elf.STT_FUNC, None) for name, library in names]
    slots = [(0x1000 + i * 8, ps4elf.R_X86_64_JUMP_SLOT, i + 1, 0) for i in range(6)]
    libraries = [(ps4elf.DT_SCE_IMPORT_LIB, 'libkernel', 0),
                 (ps4elf.DT_SCE_IMPORT_LIB, 'libScePosix', 1)]
    if pid_library != 'libkernel':
        libraries.append((ps4elf.DT_SCE_IMPORT_LIB, pid_library, 2))
    return ps4elf.build([(0, bytes(page), 0x1000, 5), (0x1000, bytes(data), 0x1000, 6)],
                        imports, [], slots, libraries=libraries,
                        elf_type=0xfe18 if module else 0xfe00)


class ProcessIdentityBoot(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pbboot = Path(os.environ.get('BB_PBBOOT', ROOT / 'build/runtime/pbboot'))
        if not cls.pbboot.is_file():
            if 'BB_PBBOOT' in os.environ:
                raise RuntimeError('selected pbboot is not built')
            raise unittest.SkipTest('pbboot not built')

    def boot(self, main=None, module=False, owned=True):
        with tempfile.TemporaryDirectory(prefix='owned-process-identity-') as directory:
            folder = Path(directory); exe = folder / 'owned.elf'; status = folder / 'status.json'
            exe.write_bytes(image() if main is None else main)
            argv = [str(self.pbboot), str(exe), '--timeout', '2', '--status', str(status),
                    '--owned-console']
            if owned:
                argv.append('--owned-process-data')
            if module:
                provider = folder / 'provider.elf'; provider.write_bytes(image(module=True))
                argv += ['--start-system-module', '/owned/identity=' + str(provider)]
            child = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                     start_new_session=True)
            try:
                stdout, stderr = child.communicate(timeout=8)
            except BaseException:
                try:
                    os.killpg(child.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                child.communicate(timeout=5)
                raise
            result = subprocess.CompletedProcess(argv, child.returncode, stdout, stderr)
            return result, json.loads(status.read_text()) if status.exists() else None, child.pid

    def returned(self, p, status, pid, packets):
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertIsNotNone(status, p.stdout)
        self.assertEqual(status['outcome'], 'returned', p.stderr)
        self.assertEqual(status['imports_remaining'], 0)
        self.assertEqual(status['imports_implemented'], 6)
        self.assertEqual(status['imports_called'], 6 * packets)
        self.assertGreater(pid, 0)
        self.assertEqual(p.stdout.count(MARKER), packets, p.stdout)
        at = 0
        for _ in range(packets):
            at = p.stdout.index(MARKER, at) + len(MARKER)
            self.assertGreaterEqual(len(p.stdout) - at, 8)
            self.assertEqual(struct.unpack_from('<Q', p.stdout, at)[0], pid)
            at += 8

    def test_both_namespaces_return_parent_observed_pid_and_preserve_guest_errno(self):
        p, status, pid = self.boot()
        self.returned(p, status, pid, 1)

    def test_eager_module_and_main_share_process_identity(self):
        p, status, pid = self.boot(module=True)
        self.returned(p, status, pid, 2)
        self.assertEqual(status['modules'][0]['state'], 'started')
        self.assertEqual(status['modules'][0]['imports_remaining'], 0)

    def test_missing_owned_profile_stops_by_name(self):
        p, status, _ = self.boot(owned=False)
        self.assertNotEqual(p.returncode, 0)
        self.assertIsNone(status)
        self.assertIn(b'getpid', p.stderr)
        self.assertIn(b'--owned-process-data', p.stderr)
        self.assertNotIn(MARKER, p.stdout)

    def test_wrong_library_and_unimplemented_identity_aliases_stay_named(self):
        for name, library in [('getpid', 'libOwnedOther'), ('_getpid', 'libkernel'),
                              ('getppid', 'libkernel')]:
            with self.subTest(name=name, library=library):
                p, status, _ = self.boot(image(pid_name=name, pid_library=library))
                self.assertEqual(p.returncode, 0, p.stderr)
                self.assertIsNotNone(status, p.stderr)
                self.assertEqual(status['outcome'], 'unimplemented')
                self.assertEqual(status['first_unimplemented']['nid'], symbol(name).split('#')[0])
                self.assertEqual(status['first_unimplemented']['library'], library)
                self.assertNotIn(MARKER, p.stdout)


if __name__ == '__main__':
    unittest.main()
