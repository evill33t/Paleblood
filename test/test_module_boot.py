# SPDX-License-Identifier: GPL-2.0-or-later
"""Owned AMD64 module fixtures: actual initializers, typed objects and outer recovery."""
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
KERNEL = [(ps4elf.DT_SCE_IMPORT_LIB, 'libkernel', 0)]
LOAD = symbol('sceKernelLoadStartModuleForSysmodule')
OWN = 'libOwnedModule'


def load_call(c):
    c.relative('488d3d', 0x1080)  # configured virtual path
    c.emit('31f631d231c94531c0')  # args size/pointer, flags and options are zero
    c.relative('4c8d0d', 0x1043)  # unaligned four-byte result, surrounded by canaries
    c.relative('ff15', 0x1000)


def main_image(before=False, arguments=False, dependency=False, repeat=False, function_addend=0, load_name=LOAD):
    c = Code()
    c.emit('53')  # preserve rbx and align calls
    if before:
        c.relative('ff15', 0x1008)
    else:
        load_call(c)
        c.emit('83f802'); c.require_equal()  # owned object handle, independent of init result
        c.relative('488b05', 0x1040)
        c.emit('48ba' + struct.pack('<Q', 0xa500000007a5a5a5).hex())
        c.emit('4839d0'); c.require_equal()
        if repeat:
            load_call(c)
        if arguments:
            c.emit('4883ec10')
            c.emit('48c704240700000048c744240808000000')
            c.emit('bf01000000be02000000ba03000000b90400000041b80500000041b906000000')
            c.emit('48b8' + struct.pack('<Q', 0x1122334455667788).hex() + '66480f6ec0')
            c.emit('48b8' + struct.pack('<Q', 0x8877665544332211).hex() + '66480f6ec8b002')
            c.relative('ff15', 0x1008)
            c.emit('4883c410')
        else:
            c.relative('ff15', 0x1008)
        c.emit('83f82a'); c.require_equal()
        if dependency:
            c.relative('ff15', 0x1010)  # shared dependency remains started exactly once
            c.emit('83f866'); c.require_equal()
    c.emit('5bc3')
    data = bytearray(0x100)
    data[0x40:0x48] = bytes([0xa5]) * 8
    data[0x80:0x83] = b'/A\0'
    syms = [(load_name, ps4elf.STT_FUNC, None), (symbol('ownedFunction', 'B'), ps4elf.STT_FUNC, None)]
    slots = [(0x1000, 7, 1, 0), (0x1008, 7, 2, 0)]
    libs = KERNEL + [(ps4elf.DT_SCE_IMPORT_LIB, OWN, 1)]
    relocs = []
    if function_addend:
        slots = [slot for slot in slots if slot[0] != 0x1008]
        relocs.append((0x1008, 1, 2, function_addend))
    if dependency:
        syms.append((symbol('ownedDependencyFunction', 'C'), ps4elf.STT_FUNC, None))
        slots.append((0x1010, 7, 3, 0))
        libs.append((ps4elf.DT_SCE_IMPORT_LIB, 'libOwnedDependency', 2))
    return ps4elf.build([(0, c.finish().ljust(0x1000, b'\0'), 0x1000, 5),
                        (0x1000, bytes(data), 0x1000, 6)], syms, relocs, slots, libraries=libs)


def module_image(arguments=False, dependency=False, override=False, mode='normal', exported_kind=ps4elf.STT_FUNC, dependency_addend=0, exported_size=64):
    c = Code()
    c.emit('53')
    c.emit('4885ff'); c.require_equal()
    c.emit('4885f6'); c.require_equal()
    if override:
        c.relative('488d05', 0x300)
        c.emit('4839c2'); c.require_equal()
        c.emit('ffd283f809'); c.require_equal()
    else:
        c.emit('4885d2'); c.require_equal()
    # This constructor must execute once, at load time.
    c.relative('803d', 0x1080 - 1)  # compensate for the trailing immediate byte in the instruction PC
    c.emit('00'); c.require_equal()
    c.relative('c605', 0x1080 - 1); c.emit('01')
    if dependency:
        c.relative('488b05', 0x1000)
        c.emit('803855'); c.require_equal()
        c.emit('c60066')  # writes through the exact provider object address
    if mode == 'fault':
        c.emit('0f0b')
    elif mode == 'timeout':
        c.emit('ebfe')
    elif mode == 'unimplemented':
        c.relative('ff15', 0x1008)
    c.emit('b8070000005bc3')
    page = bytearray(0x1000)
    code = c.finish()
    page[:len(code)] = code
    c = Code(0x200)
    if arguments:
        c.emit('3c02'); c.require_equal()
        for op, value in [('83ff', 1), ('83fe', 2), ('83fa', 3), ('83f9', 4), ('4183f8', 5), ('4183f9', 6)]:
            c.emit(op + f'{value:02x}'); c.require_equal()
        c.emit('48837c240807'); c.require_equal()
        c.emit('48837c241008'); c.require_equal()
        for op, expected in [('66480f7ec0', 0x1122334455667788), ('66480f7ec8', 0x8877665544332211)]:
            c.emit(op + '48ba' + struct.pack('<Q', expected).hex() + '4839d0'); c.require_equal()
    c.relative('803d', 0x1080 - 1); c.emit('01'); c.require_equal()
    c.emit('b82a000000c3')
    code = c.finish()
    page[0x200:0x200 + len(code)] = code
    page[0x300:0x306] = bytes.fromhex('b809000000c3')
    symbols = [(symbol('ownedFunction', 'B'), exported_kind, 0x200, exported_size)]
    relocs, slots = [], []
    libs = [(ps4elf.DT_SCE_EXPORT_LIB, OWN, 1)]
    if dependency:
        symbols.append((symbol('ownedValue', 'C'), ps4elf.STT_OBJECT, None, 8))
        relocs.append((0x1000, 1, len(symbols), dependency_addend))
        libs.append((ps4elf.DT_SCE_IMPORT_LIB, 'libOwnedDependency', 2))
    if override:
        symbols.append((symbol('module_start'), ps4elf.STT_FUNC, 0x300, 6))
        libs.append((ps4elf.DT_SCE_EXPORT_LIB, 'libOwnedEntry', 0))
    if mode == 'unimplemented':
        symbols.append((symbol('ownedMissing'), ps4elf.STT_FUNC, None))
        slots.append((0x1008, 7, len(symbols), 0))
        libs.append((ps4elf.DT_SCE_IMPORT_LIB, 'libkernel', 0))
    return ps4elf.build([(0, bytes(page), 0x1000, 5), (0x1000, bytes(0x100), 0x1000, 6)],
                        symbols, relocs, slots, libraries=libs, elf_type=0xfe18)


def dependency_image(size=8, kind=ps4elf.STT_OBJECT, storage_vaddr=0x1000, object_value=0x1080):
    c = Code()
    c.relative('803d', 0x1080 - 1); c.emit('00'); c.require_equal()
    c.relative('c605', 0x1080 - 1); c.emit('55')
    c.emit('31c0c3')
    page = bytearray(0x1000)
    code = c.finish(); page[:len(code)] = code
    c = Code(0x200)
    c.relative('0fb605', 0x1080)
    c.emit('c3')
    code = c.finish(); page[0x200:0x200 + len(code)] = code
    return ps4elf.build([(0, bytes(page), 0x1000, 5), (storage_vaddr, bytes(0x100), 0x1000, 6)],
        [(symbol('ownedValue', 'C'), kind, object_value, size),
         (symbol('ownedDependencyFunction', 'C'), ps4elf.STT_FUNC, 0x200, 8)], [],
        libraries=[(ps4elf.DT_SCE_EXPORT_LIB, 'libOwnedDependency', 2)], elf_type=0xfe18)


def tls_module_image():
    page = bytearray(0x1000)
    for start, expected in ((0, 0), (0x200, 0x55)):
        c = Code(start)
        c.emit('53')
        c.relative('488d3d', 0x1020)
        c.relative('ff15', 0x1000)
        c.emit('80382b'); c.require_equal()
        c.emit('807810' + f'{expected:02x}'); c.require_equal()
        if not start:
            c.emit('c6401055')
        c.emit('b8' + struct.pack('<I', 7 if not start else 42).hex() + '5bc3')
        code = c.finish(); page[start:start + len(code)] = code
    data = bytearray(0x110); data[0x100] = 0x2b
    return ps4elf.build([(0, bytes(page), 0x1000, 5), (0x1000, bytes(data), 0x1000, 6)],
        [(symbol('__tls_get_addr'), ps4elf.STT_FUNC, None),
         (symbol('ownedFunction', 'B'), ps4elf.STT_FUNC, 0x200, 64)],
        [(0x1020, 16, 0, 0)], [(0x1000, 7, 1, 0)],
        libraries=KERNEL + [(ps4elf.DT_SCE_EXPORT_LIB, OWN, 1)],
        tls=(0x1100, 1, 17, 16), elf_type=0xfe18)


class ModuleBoot(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pbboot = Path(os.environ.get('BB_PBBOOT', ROOT / 'build/runtime/pbboot'))
        if not cls.pbboot.is_file():
            if 'BB_PBBOOT' in os.environ:
                raise RuntimeError('selected pbboot target is not built')
            raise unittest.SkipTest('pbboot not built')

    def boot(self, main=None, module=None, dependency=None, extra=(), eager=False, duplicate=False):
        with tempfile.TemporaryDirectory(prefix='owned-module-start-') as d:
            folder = Path(d)
            exe, status = folder / 'owned.elf', folder / 'status.json'
            exe.write_bytes(main if main is not None else main_image())
            command = [str(self.pbboot), str(exe), '--timeout', '1', '--status', str(status)]
            if module is not None:
                path = folder / 'module.elf'; path.write_bytes(module)
                command += ['--start-system-module' if eager else '--system-module', '/A=' + str(path)]
            if dependency is not None:
                path = folder / 'dependency.elf'; path.write_bytes(dependency)
                command += ['--system-module', '/B=' + str(path)]
                if duplicate:
                    command += ['--system-module', '/C=' + str(path)]
            result = subprocess.run(command + list(extra), capture_output=True, text=True, timeout=8)
            return result, json.loads(status.read_text()) if status.exists() else None

    def returned(self, result, status):
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIsNotNone(status, result.stdout)
        self.assertEqual(status['outcome'], 'returned', result.stdout + result.stderr)
        self.assertEqual(status['imports_remaining'], 0)

    def test_deferred_initializer_zero_entry_and_exact_four_byte_result(self):
        p, s = self.boot(module=module_image())
        self.returned(p, s)
        self.assertEqual(s['modules'][0]['state'], 'started')

    def test_gate_preserves_gp_vector_stack_and_variadic_arguments(self):
        p, s = self.boot(main_image(arguments=True), module_image(arguments=True))
        self.returned(p, s)

    def test_actual_module_start_export_is_the_third_initializer_argument(self):
        p, s = self.boot(module=module_image(override=True))
        self.returned(p, s)

    def test_dependencies_initialize_once_before_consumer_and_share_real_data(self):
        p, s = self.boot(main_image(dependency=True), module_image(dependency=True), dependency_image())
        self.returned(p, s)
        self.assertEqual([m['state'] for m in s['modules']], ['started', 'started'])

    def test_deferred_provider_function_stops_before_initializer(self):
        p, s = self.boot(main_image(before=True), module_image())
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(s['outcome'], 'module not ready')
        self.assertEqual(s['modules'][0]['state'], 'prepared')

    def test_eager_provider_is_started_before_main(self):
        p, s = self.boot(main_image(before=True), module_image(), eager=True)
        self.returned(p, s)
        self.assertEqual(s['modules'][0]['state'], 'started')

    def test_faulted_initializer_unwinds_outer_scope_and_marks_failed(self):
        p, s = self.boot(module=module_image(mode='fault'))
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual((s['outcome'], s['modules'][0]['state']), ('fault', 'failed'))

    def test_timed_out_initializer_unwinds_outer_scope_and_marks_failed(self):
        p, s = self.boot(module=module_image(mode='timeout'))
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual((s['outcome'], s['modules'][0]['state']), ('timeout', 'failed'))

    def test_unimplemented_initializer_unwinds_outer_scope_and_marks_failed(self):
        p, s = self.boot(module=module_image(mode='unimplemented'))
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual((s['outcome'], s['modules'][0]['state']), ('unimplemented', 'failed'))

    def test_repeated_direct_load_stops_by_name(self):
        p, s = self.boot(main_image(repeat=True), module_image())
        self.assertNotEqual(p.returncode, 0)
        self.assertIsNone(s)
        self.assertIn('direct repeated module loads are not supported', p.stderr)

    def test_data_import_requires_actual_backing(self):
        p, s = self.boot(module=module_image(dependency=True))
        self.assertEqual(p.returncode, 3, p.stderr)
        self.assertIsNone(s)
        self.assertIn('data import has no admitted backing', p.stderr)

    def test_object_cannot_bind_to_function_export(self):
        p, s = self.boot(main_image(dependency=True), module_image(dependency=True), dependency_image(kind=ps4elf.STT_FUNC))
        self.assertEqual(p.returncode, 3, p.stderr)
        self.assertIn('provider symbol kind does not match import', p.stderr)
        self.assertIsNone(s)

    def test_selected_tls_symbol_cannot_masquerade_as_function(self):
        p, s = self.boot(module=module_image(exported_kind=6))
        self.assertEqual(p.returncode, 3, p.stderr)
        self.assertIn('provider symbol kind does not match import', p.stderr)
        self.assertIsNone(s)

    def test_declared_object_extent_is_enforced(self):
        p, s = self.boot(main_image(dependency=True), module_image(dependency=True), dependency_image(size=4))
        self.assertEqual(p.returncode, 3, p.stderr)
        self.assertIn('no valid declared object or executable extent', p.stderr)
        self.assertIsNone(s)

    def test_ambiguous_module_providers_are_refused(self):
        p, s = self.boot(main_image(dependency=True), module_image(dependency=True),
                         dependency_image(), duplicate=True)
        self.assertEqual(p.returncode, 3, p.stderr)
        self.assertIn('ambiguous module providers', p.stderr)
        self.assertIsNone(s)

    def test_unused_tls_exports_do_not_reject_an_otherwise_valid_module(self):
        # An unused export does not take part in binding. STT_TLS is not treated as FUNC.
        # Build a minimal provider with the same owned initializer and an extra TLS export.
        image = ps4elf.build([(0, bytes.fromhex('31c0c3').ljust(0x1000, b'\0'), 0x1000, 5)],
            [(symbol('unusedTls', 'B'), 6, 0, 8)], [],
            libraries=[(ps4elf.DT_SCE_EXPORT_LIB, OWN, 1)], elf_type=0xfe18)
        main = ps4elf.build([(0, b'\xc3', 0x1000, 5)], [], [])
        p, s = self.boot(main, image, eager=True)
        self.returned(p, s)
        self.assertEqual(s['modules'][0]['state'], 'started')

    def test_managed_constructor_retains_owned_module_tls(self):
        p, s = self.boot(main_image(), tls_module_image())
        self.returned(p, s)
        self.assertEqual(s['modules'][0]['state'], 'started')

    def test_data_export_in_an_image_hole_is_not_backing(self):
        p, s = self.boot(main_image(dependency=True), module_image(dependency=True),
                         dependency_image(storage_vaddr=0x2000, object_value=0x1800))
        self.assertEqual(p.returncode, 3, p.stderr)
        self.assertIn('no valid declared object or executable extent', p.stderr)
        self.assertIsNone(s)

    def test_data_addends_are_bounded_with_one_past_pointer_allowed(self):
        for addend in (-1, 9):
            with self.subTest(addend=addend):
                p, s = self.boot(main_image(dependency=True),
                    module_image(dependency=True, dependency_addend=addend), dependency_image())
                self.assertEqual(p.returncode, 3, p.stderr)
                self.assertIn('no valid declared object or executable extent', p.stderr)
                self.assertIsNone(s)
        # Admission permits one-past identity; no dereference is claimed or executed.
        p, s = self.boot(main_image(dependency=True),
            module_image(dependency=True, dependency_addend=8), dependency_image(), extra=('--imports',))
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertIsNone(s)

    def test_declared_function_extent_must_be_file_backed_executable(self):
        p, s = self.boot(module=module_image(exported_size=0xe01))
        self.assertEqual(p.returncode, 3, p.stderr)
        self.assertIn('no valid declared object or executable extent', p.stderr)
        self.assertIsNone(s)
        p, s = self.boot(module=module_image(exported_size=0))
        self.returned(p, s)

    def test_function_address_addends_stop_before_mapping_a_trampoline(self):
        for addend in (-1, 1):
            with self.subTest(addend=addend):
                p, s = self.boot(main_image(function_addend=addend), module_image())
                self.assertEqual(p.returncode, 3, p.stderr)
                self.assertIn('function-address relocation addends are unsupported', p.stderr)
                self.assertIsNone(s)

    def test_runtime_nid_prefix_extensions_and_short_tokens_are_refused(self):
        for name in (LOAD[:11] + 'X' + LOAD[11:], 'x#A#A'):
            with self.subTest(name=name):
                p, s = self.boot(main_image(load_name=name), module_image())
                self.assertEqual(p.returncode, 3, p.stderr)
                self.assertIn('malformed import NID', p.stderr)
                self.assertIsNone(s)

    def test_module_type_and_entry_are_admitted_before_execution(self):
        for change in ('type', 'machine', 'entry'):
            with self.subTest(change=change):
                image = bytearray(module_image())
                offset, value, fmt = {'type': (16, 0xfe10, '<H'), 'machine': (18, 183, '<H'),
                                      'entry': (24, 0x1000, '<Q')}[change]
                struct.pack_into(fmt, image, offset, value)
                p, s = self.boot(module=bytes(image))
                self.assertEqual(p.returncode, 3, p.stderr)
                self.assertIn('system module needs AMD64 ET_SCE_DYNAMIC', p.stderr)
                self.assertIsNone(s)


if __name__ == '__main__':
    unittest.main()
