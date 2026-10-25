# SPDX-License-Identifier: GPL-2.0-or-later
"""Owned AMD64 calls exercise console bytes, descriptor ownership and error registers."""
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

import ps4elf
from test_module_tls_boot import Code, symbol

ROOT = Path(__file__).resolve().parents[1]
NAMES = ('_open', '_write', '_close', '__error')
MESSAGE = b'owned console fixture bytes\n'


def image(path='/dev/console', flags=1, module=False, open_name='_open', library='libkernel', unused=False):
    c = Code()
    if unused:
        c.emit('31c0c3')
    else:
        c.emit('5341544155')  # Preserve rbx/r12/r13 and align each guest call.
        c.relative('ff15', 0x1018)
        c.emit('4989c5')  # r13 keeps our writable errno cell.
        c.emit('41c7450044332211')
        c.relative('488d3d', 0x1100)
        c.emit('48be' + struct.pack('<Q', flags).hex())
        c.emit('48ba' + struct.pack('<Q', 0xabcdef1234567890).hex())  # No creation flag: mode ignored.
        c.relative('ff15', 0x1000)
        c.emit('83f803'); c.require_equal()  # Lowest owned guest descriptor, not a host FD.
        c.emit('4883fa00'); c.require_equal()  # Explicit owned secondary-success policy.
        c.emit('89c3')
        c.emit('41817d0044332211'); c.require_equal()  # Success preserves guest errno.
        c.emit('bfffffffff'); c.relative('ff15', 0x1010)  # close(-1)
        c.emit('4883f8ff'); c.require_equal()
        c.emit('4883faff'); c.require_equal()
        c.emit('41837d0009'); c.require_equal()
        c.emit('89df'); c.relative('488d35', 0x1200)
        c.emit('ba' + struct.pack('<I', len(MESSAGE)).hex()); c.relative('ff15', 0x1008)
        c.emit('4883f8' + f'{len(MESSAGE):02x}'); c.require_equal()
        c.emit('4883fa00'); c.require_equal()
        c.emit('89df31f631d2'); c.relative('ff15', 0x1008)  # Valid zero-byte write with NULL buffer.
        c.emit('4883f800'); c.require_equal()
        c.emit('89df'); c.relative('ff15', 0x1010)
        c.emit('4883f800'); c.require_equal()
        c.emit('41837d0009'); c.require_equal()
        c.emit('41c7450088776655')
        c.emit('89df'); c.relative('ff15', 0x1010)  # Retired guest FD is not a host FD.
        c.emit('4883f8ff'); c.require_equal()
        c.emit('4883faff'); c.require_equal()
        c.emit('41837d0009'); c.require_equal()
        c.emit('bf01000000'); c.relative('488d35', 0x1200)
        c.emit('ba01000000'); c.relative('ff15', 0x1008)  # Borrowed host stdout is not registered.
        c.emit('4883f8ff'); c.require_equal()
        c.emit('4883faff'); c.require_equal()
        c.emit('41837d0009'); c.require_equal()
        c.emit('31c0415d415c5bc3')
    page = bytearray(0x1000); code = c.finish(); page[:len(code)] = code
    data = bytearray(0x400)
    pathname = path.encode('ascii') + b'\0'; data[0x100:0x100 + len(pathname)] = pathname
    data[0x200:0x200 + len(MESSAGE)] = MESSAGE
    names = (open_name, *NAMES[1:])
    imports = [(symbol(name), ps4elf.STT_FUNC, None) for name in names]
    slots = [(0x1000 + i * 8, ps4elf.R_X86_64_JUMP_SLOT, i + 1, 0) for i in range(4)]
    return ps4elf.build([(0, bytes(page), 0x1000, 5), (0x1000, bytes(data), 0x1000, 6)],
                        imports, [], slots, libraries=[(ps4elf.DT_SCE_IMPORT_LIB, library, 0)],
                        elf_type=0xfe18 if module else 0xfe00)


class ConsoleBoot(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pbboot = Path(os.environ.get('BB_PBBOOT', ROOT / 'build/runtime/pbboot'))
        if not cls.pbboot.is_file():
            if 'BB_PBBOOT' in os.environ:
                raise RuntimeError('selected pbboot is not built')
            raise unittest.SkipTest('pbboot not built')

    def boot(self, main=None, module=None, extra=(), stdout=None):
        with tempfile.TemporaryDirectory(prefix='owned-console-boot-') as directory:
            folder = Path(directory); exe = folder / 'owned.elf'; status = folder / 'status.json'
            exe.write_bytes(main if main is not None else image())
            argv = [str(self.pbboot), str(exe), '--timeout', '1', '--status', str(status), *extra]
            if module is not None:
                provider = folder / 'provider.elf'; provider.write_bytes(module)
                argv += ['--start-system-module', '/owned/console=' + str(provider)]
            p = subprocess.run(argv, stdout=stdout if stdout is not None else subprocess.PIPE,
                               stderr=subprocess.PIPE, timeout=8)
            return p, json.loads(status.read_text()) if status.exists() else None

    def test_registered_calls_emit_real_bytes_and_preserve_full_error_pair(self):
        p, status = self.boot(extra=['--owned-console'])
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(status['outcome'], 'returned')
        self.assertEqual(status['imports_implemented'], 4)
        self.assertEqual(status['imports_remaining'], 0)
        self.assertEqual(status['imports_called'], 4)
        self.assertEqual(p.stdout.count(MESSAGE), 1)

    def test_eager_initializer_and_main_share_console_lifetime(self):
        p, status = self.boot(module=image(module=True), extra=['--owned-console'])
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(status['outcome'], 'returned')
        self.assertEqual(status['modules'][0]['state'], 'started')
        self.assertEqual(p.stdout.count(MESSAGE), 2)

    def test_without_opt_in_open_stops_instead_of_inventing_a_descriptor(self):
        p, status = self.boot()
        self.assertNotEqual(p.returncode, 0)
        self.assertIn(b'_open', p.stderr)
        self.assertIn(b'--owned-console', p.stderr)
        self.assertNotIn(MESSAGE, p.stdout)
        self.assertIsNone(status)

    def test_unknown_paths_and_access_modes_remain_named(self):
        for path, flags in [('/owned/other', 1), ('/dev/console/', 1), ('/dev/console', 0),
                            ('/dev/console', 2), ('/dev/console', 0x201), ('/dev/console', (1 << 32) | 1)]:
            with self.subTest(path=path, flags=flags):
                p, status = self.boot(image(path=path, flags=flags), extra=['--owned-console'])
                self.assertNotEqual(p.returncode, 0)
                self.assertIn(b'_open', p.stderr)
                self.assertNotIn(MESSAGE, p.stdout)
                self.assertIsNone(status)

    def test_distinct_open_entrypoints_are_not_registered_as_aliases(self):
        for name in ('open', 'sceKernelOpen'):
            with self.subTest(name=name):
                p, status = self.boot(image(open_name=name), extra=['--owned-console'])
                self.assertEqual(p.returncode, 0, p.stderr)
                self.assertEqual(status['outcome'], 'unimplemented')
                self.assertEqual(status['first_unimplemented']['nid'], symbol(name).split('#')[0])
                self.assertNotIn(MESSAGE, p.stdout)

    def test_library_namespace_is_exact(self):
        p, status = self.boot(image(library='libOwnedOther'), extra=['--owned-console'])
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(status['outcome'], 'unimplemented')
        self.assertEqual(status['imports_implemented'], 0)
        self.assertEqual(status['first_unimplemented']['library'], 'libOwnedOther')
        self.assertNotIn(MESSAGE, p.stdout)

    def test_import_listing_does_not_prepare_the_output_endpoint(self):
        # A read-only fd1 would make actual console preparation fail.
        with tempfile.NamedTemporaryFile('w+b') as source:
            source.write(b'owned listing output guard'); source.flush()
            with open(source.name, 'rb') as read_only:
                p, status = self.boot(image(unused=True), extra=['--imports', '--owned-console'], stdout=read_only)
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertIsNone(status)
        self.assertNotIn(b'owned console:', p.stderr)


if __name__ == '__main__':
    unittest.main()
