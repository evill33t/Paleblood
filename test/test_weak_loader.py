# SPDX-License-Identifier: GPL-2.0-or-later
"""Owned symbol/relocation fixtures through the built loader; no guest code is executed."""
import contextlib
import ctypes
import os
import struct
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
sys.path.insert(0, os.path.join(ROOT, 'test'))

import harness
import ps4elf


class Symbol(ctypes.Structure):
    _fields_ = [('binding', ctypes.c_uint8), ('type', ctypes.c_uint8),
                ('other', ctypes.c_uint8), ('section', ctypes.c_uint16)]


def symbol_fields(pointer):
    symbol = pointer.contents
    return symbol.binding, symbol.type, symbol.other, symbol.section


class WeakLoader(unittest.TestCase):
    """The caller supplies resolution; the loader preserves metadata and relocation arithmetic."""

    @contextlib.contextmanager
    def opened(self, fixture):
        with tempfile.TemporaryDirectory(prefix='weak-loader-') as folder:
            path = os.path.join(folder, 'owned.elf')
            with open(path, 'wb') as stream:
                stream.write(fixture)
            exe = harness.open_executable(path)
            lib = exe['loader']
            for name in ('rt_elf_import_symbol', 'rt_elf_export_symbol'):
                getattr(lib, name).argtypes = [ctypes.c_void_p, ctypes.c_size_t]
                getattr(lib, name).restype = ctypes.POINTER(Symbol)
            lib.rt_elf_export_count.argtypes = [ctypes.c_void_p]
            lib.rt_elf_export_count.restype = ctypes.c_size_t
            for name in ('rt_elf_import_size', 'rt_elf_export_value', 'rt_elf_export_size'):
                getattr(lib, name).argtypes = [ctypes.c_void_p, ctypes.c_size_t]
                getattr(lib, name).restype = ctypes.c_uint64
            for name in ('rt_elf_import_kind', 'rt_elf_export_kind', 'rt_elf_import_has_function_addend'):
                getattr(lib, name).argtypes = [ctypes.c_void_p, ctypes.c_size_t]
                getattr(lib, name).restype = ctypes.c_int
            for name in ('rt_elf_export_name', 'rt_elf_import_library', 'rt_elf_export_library'):
                getattr(lib, name).argtypes = [ctypes.c_void_p, ctypes.c_size_t]
                getattr(lib, name).restype = ctypes.c_char_p
            lib.rt_elf_close.argtypes = [ctypes.c_void_p]
            lib.rt_elf_close.restype = None
            try:
                yield exe
            finally:
                lib.rt_elf_close(exe['handle'])

    def mapped(self, exe, resolve):
        guard = 16
        image = ctypes.create_string_buffer(exe['size'] + 2 * guard)
        ctypes.memset(ctypes.addressof(image), 0xa5, len(image))
        base = ctypes.addressof(image) + guard
        calls = []
        def bind(user, index, kind):
            calls.append((index, kind))
            return resolve(index, kind)
        callback = harness.LOADER_BIND(bind)
        error = ctypes.create_string_buffer(256)
        result = exe['loader'].rt_elf_map(exe['handle'], base, callback, None, error, len(error))
        self.assertEqual(image.raw[:guard], b'\xa5' * guard)
        self.assertEqual(image.raw[-guard:], b'\xa5' * guard)
        return result, error.value, image.raw[guard:-guard], calls

    def test_raw_import_metadata_and_existing_tuple_defaults(self):
        symbols = [('three_fields', 1, None), ('four_fields', 2, None, 19),
                   ('weak_plain', 0, None, 7, 2), ('raw_other', 6, None, 11, 2, 0x82, 0),
                   ('function#A#B', 2, None, 9, 1, 0, 0)]
        fixture = ps4elf.build([(0, bytes(0x40), 0x80, 6)], symbols,
                              [(i * 8, 1, i + 1, 0) for i in range(len(symbols))],
                              libraries=[(ps4elf.DT_SCE_IMPORT_LIB, 'owned_library', 0)])
        with self.opened(fixture) as exe:
            lib, handle = exe['loader'], exe['handle']
            self.assertEqual(exe['names'], [s[0] for s in symbols])
            self.assertEqual([symbol_fields(lib.rt_elf_import_symbol(handle, i)) for i in range(5)],
                             [(1, 1, 0, 0), (1, 2, 0, 0), (2, 0, 0, 0), (2, 6, 0x82, 0), (1, 2, 0, 0)])
            self.assertEqual([lib.rt_elf_import_size(handle, i) for i in range(5)], [0, 19, 7, 11, 9])
            self.assertEqual([lib.rt_elf_import_kind(handle, i) for i in range(5)], [1, 0, 2, 2, 0])
            self.assertIsNone(lib.rt_elf_import_library(handle, 2))
            self.assertEqual(lib.rt_elf_import_library(handle, 4), b'owned_library')
            self.assertFalse(lib.rt_elf_import_symbol(handle, 5))
            self.assertFalse(lib.rt_elf_import_symbol(handle, ctypes.c_size_t(-1).value))

    def test_import_identity_and_aggregation_do_not_cross_metadata(self):
        symbols = [('same', 2, None, 4, 2), ('same', 2, None, 9, 1),
                   ('same', 0, None, 99, 2), ('same', 2, None, 80, 2, 2),
                   ('same', 2, None, 24, 2), ('same', 2, None, 10, 2),
                   ('same', 2, None, 999, 1)]
        fixture = ps4elf.build([(0, bytes(0x40), 0x80, 6)], symbols,
                              [(0, 1, 1, 0), (8, 1, 2, 0), (16, 1, 3, 3), (24, 1, 4, 0),
                               (32, 1, 5, 4), (40, 1, 6, -3), (48, 1, 7, 0)])
        with self.opened(fixture) as exe:
            lib, handle = exe['loader'], exe['handle']
            self.assertEqual(exe['names'], ['same'] * 4)
            self.assertEqual([symbol_fields(lib.rt_elf_import_symbol(handle, i)) for i in range(4)],
                             [(2, 2, 0, 0), (1, 2, 0, 0), (2, 0, 0, 0), (2, 2, 2, 0)])
            self.assertEqual([lib.rt_elf_import_size(handle, i) for i in range(4)], [24, 999, 99, 80])
            self.assertEqual([lib.rt_elf_import_has_function_addend(handle, i) for i in range(4)], [1, 0, 0, 0])
            result, error, image, calls = self.mapped(exe, lambda index, kind: 0x8000 + index * 0x100)
            self.assertEqual(result, 0, error)
            self.assertEqual(calls, [(0, 0), (1, 0), (2, 2), (3, 0)])
            self.assertEqual(struct.unpack_from('<7Q', image),
                             (0x8000, 0x8100, 0x8203, 0x8300, 0x8004, 0x7ffd, 0x8100))

    def test_weak_and_required_same_name_resolve_independently(self):
        fixture = ps4elf.build([(0, bytes(16), 32, 6)],
                              [('shared', 2, None, 0, 2), ('shared', 2, None)],
                              [(0, 6, 1, 0)], jump_slots=[(8, 7, 2, 0)])
        with self.opened(fixture) as exe:
            self.assertEqual(exe['names'], ['shared', 'shared'])
            result, error, image, calls = self.mapped(exe, lambda index, kind: 0 if index == 0 else 0x12345678)
            self.assertEqual(result, 0, error)
            self.assertEqual(calls, [(0, 0), (1, 0)])
            self.assertEqual(struct.unpack_from('<2Q', image), (0, 0x12345678))
            result, error, _, calls = self.mapped(exe, lambda index, kind: 0)
            self.assertEqual(result, -1)
            self.assertIn(b'import shared was not bound', error)
            self.assertEqual(calls, [(0, 0), (1, 0)])

    def test_basic_weak_relocations_preserve_r64_addends_at_zero_and_nonzero(self):
        for symbol_type, kind in ((0, 2), (1, 1), (2, 0)):
            fixture = ps4elf.build([(0, bytes(0x30), 0x60, 6)], [('optional', symbol_type, None, 0, 2)],
                                  [(0, 1, 1, 0), (8, 1, 1, 9), (16, 1, 1, -7), (24, 6, 1, 91)],
                                  jump_slots=[(32, 7, 1, -91)])
            for binding in (0, 0x123456789abcdef0):
                with self.subTest(symbol_type=symbol_type, binding=binding), self.opened(fixture) as exe:
                    result, error, image, calls = self.mapped(exe, lambda index, kind: binding)
                    self.assertEqual(result, 0, error)
                    self.assertEqual(calls, [(0, kind)])
                    self.assertEqual(struct.unpack_from('<5Q', image),
                                     (binding, binding + 9, (binding - 7) & ((1 << 64) - 1), binding, binding))
                    self.assertEqual(image[40:], bytes(0x60 - 40))

    def test_zero_binding_rejects_required_or_unsupported_raw_metadata(self):
        rejected = [(binding, 2, 0) for binding in (0, 1, 3, 10, 15)]
        rejected += [(2, symbol_type, 0) for symbol_type in (3, 4, 5, 6, 10, 15)]
        rejected += [(2, symbol_type, other) for symbol_type in (0, 1, 2) for other in (1, 2, 3, 0x80)]
        for binding, symbol_type, other in rejected:
            with self.subTest(binding=binding, symbol_type=symbol_type, other=other):
                fixture = ps4elf.build([(0, bytes(8), 32, 6)],
                                      [('must_bind', symbol_type, None, 0, binding, other)], [(0, 1, 1, 5)])
                with self.opened(fixture) as exe:
                    result, error, _, calls = self.mapped(exe, lambda index, kind: 0)
                    self.assertEqual(result, -1)
                    self.assertIn(b'import must_bind was not bound', error)
                    self.assertEqual(len(calls), 1)

    def test_export_catalogue_retains_plain_special_and_visibility_metadata(self):
        symbols = [('same_plain', 2, 0, 7), ('same_plain', 0, 0x10, 3, 2),
                   ('absolute', 1, 0xfedcba9876543210, 8, 1, 0, 0xfff1),
                   ('tls', 6, 5, 16, 1, 0, 4), ('hidden', 1, 0x20, 4, 1, 2, 3),
                   ('raw_other', 15, 0x28, 9, 2, 0x80, 0xfff2),
                   ('export#A#B', 2, 0x30, 6), ('undefined', 2, None),
                   ('local', 2, 0x38, 1, 0), ('unknown_binding', 2, 0x38, 1, 3)]
        fixture = ps4elf.build([(0, bytes(0x40), 0x80, 5)], symbols, [],
                              libraries=[(ps4elf.DT_SCE_EXPORT_LIB, 'owned_library', 0)])
        with self.opened(fixture) as exe:
            lib, handle = exe['loader'], exe['handle']
            self.assertEqual(lib.rt_elf_export_count(handle), 7)
            self.assertEqual([lib.rt_elf_export_name(handle, i).decode() for i in range(7)],
                             [s[0] for s in symbols[:7]])
            self.assertEqual([symbol_fields(lib.rt_elf_export_symbol(handle, i)) for i in range(7)],
                             [(1, 2, 0, 1), (2, 0, 0, 1), (1, 1, 0, 0xfff1), (1, 6, 0, 4),
                              (1, 1, 2, 3), (2, 15, 0x80, 0xfff2), (1, 2, 0, 1)])
            self.assertEqual([lib.rt_elf_export_value(handle, i) for i in range(7)], [s[2] for s in symbols[:7]])
            self.assertEqual([lib.rt_elf_export_size(handle, i) for i in range(7)], [s[3] for s in symbols[:7]])
            self.assertEqual([lib.rt_elf_export_kind(handle, i) for i in range(7)], [0, 2, 1, 2, 1, 2, 0])
            self.assertEqual([lib.rt_elf_export_library(handle, i) for i in range(6)], [None] * 6)
            self.assertEqual(lib.rt_elf_export_library(handle, 6), b'owned_library')
            self.assertFalse(lib.rt_elf_export_symbol(handle, 7))
            self.assertFalse(lib.rt_elf_export_symbol(handle, ctypes.c_size_t(-1).value))
            self.assertIsNone(lib.rt_elf_export_name(handle, 7))

        # A malformed declared external definition cannot disappear from the
        # candidate catalogue and turn an optional reference into false absence.
        fixture = ps4elf.build([(0, bytes(16), 32, 5)], [('candidate', 2, 0, 1)], [])
        phoff = struct.unpack_from('<Q', fixture, 32)[0]
        stride, count = struct.unpack_from('<HH', fixture, 54)
        headers = [struct.unpack_from('<II6Q', fixture, phoff + i * stride) for i in range(count)]
        dyn = next(row for row in headers if row[0] == 2)
        carrier = next(row for row in headers if row[0] == 0x61000000)[2]
        tags = dict(struct.unpack_from('<QQ', fixture, pos) for pos in range(dyn[2], dyn[2] + dyn[5], 16))
        first_symbol = carrier + tags[0x61000039] + 24
        strings = carrier + tags[0x61000035]
        string_size = tags[0x61000037]
        for corruption in ('out-of-range', 'unterminated'):
            broken = bytearray(fixture)
            if corruption == 'out-of-range':
                struct.pack_into('<I', broken, first_symbol, string_size)
            else:
                start = struct.unpack_from('<I', broken, first_symbol)[0]
                broken[strings + start:strings + string_size] = b'Q' * (string_size - start)
            with self.subTest(corruption=corruption), tempfile.TemporaryDirectory(prefix='invalid-export-') as folder:
                path = os.path.join(folder, 'owned.elf')
                with open(path, 'wb') as stream:
                    stream.write(broken)
                with self.assertRaisesRegex(harness.BadInput, 'invalid exported symbol name'):
                    harness.open_executable(path)


if __name__ == '__main__':
    unittest.main()
