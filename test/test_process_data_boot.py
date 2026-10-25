# SPDX-License-Identifier: GPL-2.0-or-later
"""Owned AMD64 images exercise real data relocations and current-stack startup."""
import csv
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

import ps4elf
from test_module_tls_boot import Code, symbol

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import boot
import runtime_issues
NAMES = ('environ', '__progname', '__stack_chk_guard', '_sigintr')


def image(module=False, with_module=False, unused=False, guard_size=0, library='libkernel', addend=0):
    c = Code()
    if unused:
        c.emit('31c0c3')
    elif module:
        c.relative('488b05', 0x1010)  # address of the complete guard object
        c.emit('48833800')
        c.relative('0f84', 'failed')  # a successful guard has a nonzero first QWORD
        c.relative('488b05', 0x1018)
        for displacement in (0, 4, 8, 12):
            c.emit('8378' + f'{displacement:02x}' + '00'); c.require_equal()
        c.emit('c7400c00000080')
        c.emit('31c0c3')
    else:
        c.emit('53415441554883ec40')  # preserve rbx,r12,r13; keep calls aligned
        c.emit('4989fc49833c2401'); c.require_equal()  # raw argc=1
        c.emit('49837c241000'); c.require_equal()  # argv terminator
        c.relative('488b05', 0x1000)
        c.emit('488b18498d4c24184839cb'); c.require_equal()  # environ=&argv[2]
        c.emit('48833b00'); c.require_equal()
        c.relative('488b05', 0x1008)
        c.emit('488b08498b5424084839d1'); c.require_equal()  # progname aliases argv[0]
        c.emit('48b8' + struct.pack('<Q', int.from_bytes(b'eboot.bi', 'little')).hex())
        c.emit('483902'); c.require_equal()
        c.emit('66837a086e'); c.require_equal()  # final n and NUL
        c.emit('c60145803a45'); c.require_equal()  # argv and progname share writable storage
        c.relative('4c8b2d', 0x1010)
        c.emit('49837d0000'); c.relative('0f84', 'failed')
        for displacement in range(0, 64, 8):
            c.emit('498b45' + f'{displacement:02x}' + '48894424' + f'{displacement:02x}')
        c.relative('488b1d', 0x1018)
        for displacement in (0, 4, 8):
            c.emit('837b' + f'{displacement:02x}' + '00'); c.require_equal()
        c.emit('817b0c' + struct.pack('<I', 0x80000000 if with_module else 0).hex()); c.require_equal()
        if with_module:
            c.relative('ff15', 0x1020)
            c.emit('83f82a'); c.require_equal()
            c.emit('833b01'); c.require_equal()
        for displacement in range(0, 64, 8):
            c.emit('498b45' + f'{displacement:02x}' + '483b4424' + f'{displacement:02x}'); c.require_equal()
        c.emit('4883c440415d415c5bc3')
    page = bytearray(0x1000)
    code = c.finish(); page[:len(code)] = code
    syms = [(symbol(n), ps4elf.STT_OBJECT, None, guard_size if n == '__stack_chk_guard' else 0) for n in NAMES]
    relocs = [(0x1000 + i * 8, ps4elf.R_X86_64_64, i + 1, addend if i == 2 else 0) for i in range(4)]
    slots = []
    libs = [(ps4elf.DT_SCE_IMPORT_LIB, library, 0)]
    if module:
        c = Code(0x400)
        c.relative('488b05', 0x1018)
        c.emit('c70001000000b82a000000c3')
        code = c.finish(); page[0x400:0x400 + len(code)] = code
        syms.append((symbol('ownedProcessCheck', 'B'), ps4elf.STT_FUNC, 0x400, len(code)))
        libs.append((ps4elf.DT_SCE_EXPORT_LIB, 'libOwnedProcessData', 1))
    if with_module:
        syms.append((symbol('ownedProcessCheck', 'B'), ps4elf.STT_FUNC, None))
        slots.append((0x1020, ps4elf.R_X86_64_JUMP_SLOT, len(syms), 0))
        libs.append((ps4elf.DT_SCE_IMPORT_LIB, 'libOwnedProcessData', 1))
    return ps4elf.build([(0, bytes(page), 0x1000, 5), (0x1000, bytes(0x100), 0x1000, 6)],
                        syms, relocs, slots, libraries=libs, elf_type=0xfe18 if module else 0xfe00)


class ProcessDataBoot(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pbboot = Path(os.environ.get('BB_PBBOOT', ROOT / 'build/runtime/pbboot'))
        if not cls.pbboot.is_file():
            if 'BB_PBBOOT' in os.environ:
                raise RuntimeError('selected pbboot target is not built')
            raise unittest.SkipTest('pbboot not built')

    def boot(self, main=None, module=None, extra=(), legacy=False):
        with tempfile.TemporaryDirectory(prefix='owned-process-data-') as d:
            folder = Path(d)
            exe, status = folder / 'owned.elf', folder / 'status.json'
            exe.write_bytes(main if main is not None else image())
            command = [str(self.pbboot), str(exe), '--timeout', '1', '--status', str(status)]
            if module is not None:
                path = folder / 'module.elf'; path.write_bytes(module)
                command += ['--module', str(path)] if legacy else ['--start-system-module', '/owned/data=' + str(path)]
            result = subprocess.run(command + list(extra), capture_output=True, text=True, timeout=8)
            return result, json.loads(status.read_text()) if status.exists() else None

    def test_copied_parameters_and_all_four_real_object_relocations(self):
        p, s = self.boot(extra=['--owned-process-data'])
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(s['outcome'], 'returned')
        self.assertEqual(s['imports_implemented'], 4)
        self.assertEqual(s['imports_remaining'], 0)

    def test_constructor_and_main_share_writable_objects_and_stable_guard(self):
        p, s = self.boot(image(with_module=True), image(module=True), ['--owned-process-data'])
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(s['outcome'], 'returned')
        self.assertEqual(s['imports_implemented'], 4)
        self.assertEqual(s['imports_bundled'], 1)
        self.assertEqual(s['imports_remaining'], 0)
        self.assertEqual(s['modules'][0]['state'], 'started')
        self.assertEqual(s['modules'][0]['imports_remaining'], 0)

    def test_unprepared_system_objects_fail_before_any_guest_frame(self):
        p, s = self.boot(image(with_module=True), image(module=True))
        self.assertEqual(p.returncode, 3)
        self.assertIn('runtime data is not prepared', p.stderr)
        self.assertIn('--owned-process-data', p.stderr)
        self.assertIsNone(s)

    def test_listing_keeps_objects_pending_even_with_opt_in(self):
        for flags in (['--imports'], ['--imports', '--owned-process-data']):
            with self.subTest(flags=flags):
                p, s = self.boot(image(with_module=True), image(module=True), flags)
                self.assertEqual(p.returncode, 0, p.stderr)
                self.assertEqual(sum('\tdata\truntime-data-pending\t' in row for row in p.stdout.splitlines()), 8)
                self.assertNotIn('\tdata\truntime\t', p.stdout)
                self.assertIsNone(s)
                with tempfile.TemporaryDirectory(prefix='pending-data-listing-') as folder:
                    (Path(folder) / 'symbols').mkdir()
                    boot.write_listing(p.stdout, folder)
                    rows = runtime_issues.read_imports(folder)
                    data = [row for row in rows if row['kind'] == 'data']
                    self.assertEqual(len(data), 4)
                    self.assertTrue(all(row['implemented'] == 'no' for row in data))
                    with (Path(folder) / 'symbols/module_functions.csv').open() as f:
                        functions = list(csv.DictReader(f))
                    self.assertEqual(len(functions), 1)  # actual owned module function only
                    self.assertEqual(functions[0]['module'], 'module.elf')

    def test_unflagged_legacy_data_remains_unimplemented(self):
        p, s = self.boot(image(unused=True), image(module=True, unused=True), legacy=True)
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(s['outcome'], 'returned')
        self.assertEqual(s['imports_implemented'], 0)
        self.assertEqual(s['imports_remaining'], 4)
        self.assertEqual(s['modules'][0]['imports_remaining'], 4)

    def test_complete_guard_extent_is_enforced(self):
        p, s = self.boot(image(guard_size=65), extra=['--owned-process-data'])
        self.assertEqual(p.returncode, 3)
        self.assertIn('insufficient backing', p.stderr)
        self.assertIsNone(s)

    def test_pending_listing_still_checks_declared_extent(self):
        p, s = self.boot(image(guard_size=65), extra=['--imports'])
        self.assertEqual(p.returncode, 3)
        self.assertIn('insufficient backing', p.stderr)
        self.assertIsNone(s)

    def test_exact_library_identity_is_required(self):
        p, s = self.boot(image(library='libOtherKernel'), image(module=True), ['--owned-process-data'])
        self.assertEqual(p.returncode, 3)
        self.assertIn('data import has no admitted backing', p.stderr)
        self.assertIsNone(s)

    def test_negative_object_addend_cannot_escape_backing(self):
        p, s = self.boot(image(addend=-1), extra=['--owned-process-data'])
        self.assertEqual(p.returncode, 3)
        self.assertIn('insufficient backing', p.stderr)
        self.assertIsNone(s)


if __name__ == '__main__':
    unittest.main()
