# SPDX-License-Identifier: GPL-2.0-or-later
"""Owned AMD64 weak references: null means absent, never a callable success stub."""
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
from test_module_boot import module_image
from test_module_tls_boot import Code, symbol

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import boot
import runtime_issues


def main_image(name='x', symbol_type=0, binding=2, other=0, addend=0, relocation=1,
               library='libOwnedWeak', check='zero', definitions=(), export_library=False, as_module=False):
    c = Code()
    if check == 'call':
        c.emit('53')
        c.relative('ff15', 0x1000)
        c.emit('83f82a'); c.require_equal()
        c.emit('5bc3')
    elif check == 'call_absent':
        c.emit('53'); c.relative('ff15', 0x1000); c.emit('5bc3')
    else:
        c.relative('488b05', 0x1000)
        expected = addend & ((1 << 64) - 1) if relocation == 1 else 0
        c.emit('48ba' + struct.pack('<Q', expected).hex() + '4839d0'); c.require_equal()
        c.emit('c3')
    page = bytearray(0x1000)
    code = c.finish(); page[:len(code)] = code
    page[0x200:0x206] = bytes.fromhex('b82a000000c3')
    symbols = [(name, symbol_type, None, 0, binding, other), *definitions]
    relocs, slots = ([], [(0x1000, 7, 1, addend)]) if relocation == 7 else ([(0x1000, relocation, 1, addend)], [])
    return ps4elf.build([(0, bytes(page), 0x1000, 5), (0x1000, bytes(0x100), 0x1000, 6)],
                        symbols, relocs, slots, libraries=[(ps4elf.DT_SCE_IMPORT_LIB, library, 0),
                                                          (ps4elf.DT_SCE_IMPORT_LIB, 'libOwnedModule', 1)]
                        + ([(ps4elf.DT_SCE_EXPORT_LIB, library, 0)] if export_library else []),
                        elf_type=0xfe18 if as_module else 0xfe10)


def plain_provider(symbol_type=2, other=0, section=1, name='x'):
    page = bytearray(0x1000); page[:3] = bytes.fromhex('31c0c3')
    page[0x200:0x206] = bytes.fromhex('b82a000000c3')
    return ps4elf.build([(0, bytes(page), 0x1000, 5)],
                        [(name, symbol_type, 0x200, 6, 1, other, section)], [], elf_type=0xfe18)


class WeakBoot(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pbboot = Path(os.environ.get('BB_PBBOOT', ROOT / 'build/runtime/pbboot'))
        if not cls.pbboot.is_file():
            if 'BB_PBBOOT' in os.environ:
                raise RuntimeError('selected pbboot target is not built')
            raise unittest.SkipTest('pbboot not built')

    def boot(self, main=None, provider=None, extra=(), managed=True, legacy_provider=False):
        with tempfile.TemporaryDirectory(prefix='owned-weak-boot-') as d:
            folder = Path(d); exe, status = folder / 'owned.elf', folder / 'status.json'
            exe.write_bytes(main if main is not None else main_image())
            path = folder / 'provider.elf'
            path.write_bytes(provider if provider is not None else module_image())
            command = [str(self.pbboot), str(exe), '--timeout', '1', '--status', str(status), *extra]
            if legacy_provider:
                command += ['--module', str(path)]
            elif managed:
                command += ['--start-system-module', '/owned/weak=' + str(path)]
            p = subprocess.run(command, capture_output=True, text=True, timeout=8)
            return p, json.loads(status.read_text()) if status.exists() else None

    def absent(self, p, status):
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(status['outcome'], 'returned')
        self.assertEqual(status['imports_implemented'], 0)
        self.assertEqual(status['imports_remaining'], 1)
        self.assertEqual(status['imports_called'], 0)

    def rejected(self, p, status, message):
        self.assertEqual(p.returncode, 3, p.stdout + p.stderr)
        self.assertIn(message, p.stderr)
        self.assertIsNone(status)

    def test_missing_plain_and_encoded_ordinary_weak_references_are_zero(self):
        for name in ('x', symbol('ownedMissing')):
            for kind in (0, 1, 2):
                for relocation in (1, 6, 7):
                    with self.subTest(name=name, kind=kind, relocation=relocation):
                        self.absent(*self.boot(main_image(name, kind, relocation=relocation)))

        self.absent(*self.boot(managed=False))
        self.absent(*self.boot(main_image(symbol('ownedMissing'), 2), managed=False))

    def test_absent_r64_keeps_addends_for_untyped_objects_and_functions(self):
        for kind in (0, 1, 2):
            for addend in (17, -7):
                with self.subTest(kind=kind, addend=addend):
                    self.absent(*self.boot(main_image(symbol('ownedMissing'), kind, addend=addend)))

    def test_available_typed_weak_function_binds_and_runs_after_initializer(self):
        p, status = self.boot(main_image(symbol('ownedFunction', 'B'), 2, check='call', relocation=7))
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(status['outcome'], 'returned')
        self.assertEqual(status['imports_bundled'], 1)
        self.assertEqual(status['imports_remaining'], 0)
        self.assertEqual(status['imports_called'], 1)
        self.assertEqual(status['modules'][0]['state'], 'started')
        for binding in (1, 2):
            for legacy in (False, True):
                with self.subTest(binding=binding, legacy=legacy):
                    self.rejected(*self.boot(main_image(symbol('ownedFunction', 'B'), 2, binding=binding,
                                                         addend=1, check='call'), legacy_provider=legacy),
                                  'function-address relocation addends are unsupported')

    def test_visible_plain_definitions_are_candidates_even_if_special_or_untyped(self):
        for kind, other, section in ((2, 0, 1), (0, 0, 1), (6, 0, 1), (1, 0, 0xfff1), (2, 3, 1), (2, 5, 1), (2, 0x82, 1)):
            with self.subTest(kind=kind, other=other, section=section):
                self.rejected(*self.boot(provider=plain_provider(kind, other, section)), 'plain weak definition')

    def test_main_or_self_definitions_prevent_false_absence(self):
        self.rejected(*self.boot(main_image(definitions=[('x', 2, 0x200, 6)])), 'plain weak definition')
        name = symbol('ownedLocalDefinition')
        definitions = [(name, 2, 0x200, 6)]
        self.rejected(*self.boot(main_image(name, 2, definitions=definitions, export_library=True)),
                      'weak definition in main or importer')
        provider = main_image(name, 2, definitions=definitions, export_library=True, as_module=True)
        self.rejected(*self.boot(provider=provider), 'weak definition in main or importer')

    def test_hidden_or_internal_plain_definitions_are_not_external_candidates(self):
        for visibility in (1, 2):
            with self.subTest(visibility=visibility):
                self.absent(*self.boot(provider=plain_provider(other=visibility)))

    def test_known_plain_runtime_provider_is_not_treated_as_absent(self):
        self.rejected(*self.boot(main_image('sceKernelGetProcParam', 2)), 'plain weak runtime provider')

    def test_required_plain_and_unsupported_weak_kinds_never_become_optional(self):
        self.rejected(*self.boot(main_image(symbol_type=2, binding=1)), 'malformed import NID')
        for kind, other in ((6, 0), (10, 0), (0, 2), (1, 3), (2, 0x80)):
            with self.subTest(kind=kind, other=other):
                p, status = self.boot(main_image(symbol('ownedMissing'), kind, other=other))
                self.assertEqual(p.returncode, 3, p.stdout + p.stderr)
                self.assertIsNone(status)
                self.assertNotIn('weak-absent', p.stdout)

    def test_present_wrong_kind_or_unprepared_runtime_data_never_becomes_zero(self):
        self.rejected(*self.boot(main_image(symbol('ownedFunction', 'B'), 1)), 'provider symbol kind')
        self.rejected(*self.boot(main_image(symbol('sceKernelGetProcParam'), 1, library='libkernel')),
                      'runtime function provider does not match')
        self.rejected(*self.boot(main_image(symbol('environ'), 2, library='libkernel')),
                      'runtime data provider does not match')
        self.rejected(*self.boot(main_image(symbol('environ'), 1, library='libkernel')),
                      'runtime data is not prepared')
        self.rejected(*self.boot(main_image(symbol('environ'), 1, library='libkernel'), managed=False),
                      'runtime data is not prepared')
        p, status = self.boot(main_image(symbol('environ'), 1, library='libkernel'), extra=['--imports'])
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertIn('\tdata\truntime-data-pending\t', p.stdout)
        self.assertNotIn('weak-absent', p.stdout)
        self.assertIsNone(status)

    def test_malformed_weak_nid_or_missing_namespace_is_not_plain_absence(self):
        self.rejected(*self.boot(main_image('x#A#A', 2)), 'malformed weak import NID')
        self.rejected(*self.boot(main_image(symbol('ownedMissing', 'D'), 2)), 'weak NID import has no library')

    def test_short_plain_listing_and_coverage_keep_absence_unimplemented(self):
        p, status = self.boot(extra=['--imports'])
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertIsNone(status)
        self.assertIn('\tx\tuntyped\tweak-absent\t', p.stdout)
        with tempfile.TemporaryDirectory(prefix='weak-listing-') as folder:
            (Path(folder) / 'symbols').mkdir()
            boot.write_listing(p.stdout, folder)
            rows = runtime_issues.read_imports(folder)
            self.assertEqual(len(rows), 0)  # plain symbol has no library namespace
            with (Path(folder) / 'symbols/module_functions.csv').open() as f:
                self.assertEqual(list(csv.DictReader(f)), [])
        p, _ = self.boot(main_image(symbol('ownedMissing'), 2), extra=['--imports'])
        with tempfile.TemporaryDirectory(prefix='weak-encoded-listing-') as folder:
            (Path(folder) / 'symbols').mkdir()
            boot.write_listing(p.stdout, folder)
            rows = runtime_issues.read_imports(folder)
            self.assertEqual(len(rows), 1)
            self.assertEqual(rows[0]['implemented'], 'no')
            with (Path(folder) / 'symbols/module_functions.csv').open() as f:
                self.assertEqual(list(csv.DictReader(f)), [])

    def test_calling_absent_weak_function_faults_instead_of_running_a_stub(self):
        p, status = self.boot(main_image(symbol('ownedMissing'), 2, relocation=7, check='call_absent'))
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(status['outcome'], 'fault')
        self.assertEqual(status['fault']['signal'], 11)
        self.assertIsNone(status['first_unimplemented'])
        self.assertEqual(status['imports_implemented'], 0)
        self.assertEqual(status['imports_called'], 0)


if __name__ == '__main__':
    unittest.main()
