# SPDX-License-Identifier: GPL-2.0-or-later
"""Owned boot policy fixtures; no shipped executable or CPU scheduling changes."""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

from test_boot_darwin import image, import_call

ROOT = Path(__file__).resolve().parents[1]
QUERY_MARKER = 'owned test: Darwin count query called'
IMPORTS = ('scePthreadSelf', 'scePthreadAttrInit', 'scePthreadAttrGet',
           'scePthreadAttrGetaffinity', 'scePthreadAttrDestroy')


def attribute_fixture(count):
    """Own AMD64 code checks fresh unrestricted and captured logical masks through public imports."""
    code = bytearray(b'\x53\x48\x83\xec\x20')  # save RBX; aligned call stack + two local words
    import_call(code, 0)
    code += b'\x48\x89\xc3\x48\x8d\x3c\x24'  # RBX=self, RDI=&attribute
    import_call(code, 1)

    def success():
        code.extend(b'\x85\xc0\x74\x02\x0f\x0b')  # nonzero EAX faults

    def affinity(expected):
        code.extend(b'\x48\x8d\x3c\x24\x48\x8d\x74\x24\x08')
        import_call(code, 3)
        success()
        code.extend(b'\x48\xba' + struct.pack('<Q', expected))
        code.extend(b'\x48\x39\x54\x24\x08\x74\x02\x0f\x0b')

    success()
    affinity((1 << 64) - 1)
    code += b'\x48\x89\xdf\x48\x8d\x34\x24'  # RDI=self, RSI=&attribute
    import_call(code, 2)
    success()
    affinity((1 << count) - 1)
    code += b'\x48\x8d\x3c\x24'
    import_call(code, 4)
    success()
    code += b'\x48\x83\xc4\x20\x5b\xc3'
    return bytes(code)


@unittest.skipUnless(sys.platform == 'darwin', 'Darwin explicit logical boot policy')
class LogicalCpuBoot(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.normal = Path(os.environ.get('BB_PBBOOT', ROOT / 'build/runtime/pbboot'))
        cls.mock = Path(os.environ.get('BB_PBBOOT_LOGICAL_QUERY_TEST',
                                      ROOT / 'build/test_boot_logical_cpu'))
        if not cls.normal.is_file() or not cls.mock.is_file():
            raise RuntimeError('selected normal and test-only boot targets must both be built')
        cls.temp = tempfile.TemporaryDirectory(prefix='pb-logical-cpu-')
        cls.addClassCleanup(cls.temp.cleanup)
        cls.folder = Path(cls.temp.name)
        cls.serial = 0

    def boot(self, code=b'\xc3', args=(), mock=True, env_extra=None, imports=()):
        type(self).serial += 1
        folder = self.folder / str(self.serial)
        folder.mkdir()
        executable = folder / 'owned.elf'
        executable.write_bytes(image(code, imports))
        status = folder / 'status.json'
        env = dict(os.environ)
        env.pop('BB_RUNTIME_CPU_MAP', None)
        env.pop('PB_TEST_COUNT_FAILURE', None)
        env.update(env_extra or {})
        command = [str(self.mock if mock else self.normal), str(executable),
                   '--timeout', '1', '--status', str(status), *args]
        result = subprocess.run(command, env=env, text=True, capture_output=True, timeout=8)
        return result, json.loads(status.read_text()) if status.exists() else None

    def returned(self, result, status):
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIsNotNone(status)
        self.assertEqual(status['outcome'], 'returned')
        self.assertIn('Darwin owned TLS and signal state released', result.stderr)
        self.assertIn('native affinity unavailable', result.stderr)

    def test_explicit_counts_through_normal_public_imports(self):
        for count in (1, 3, 63, 64):
            with self.subTest(count=count):
                result, status = self.boot(attribute_fixture(count),
                    args=('--guest-cpu-count', str(count)), mock=False, imports=IMPORTS)
                self.returned(result, status)
                self.assertIn(f'count {count}, source explicit configuration', result.stderr)
                self.assertNotIn(QUERY_MARKER, result.stderr)

    def test_host_count_is_resolved_once_then_snapshotted(self):
        result, status = self.boot(attribute_fixture(3),
            args=('--guest-cpu-count', 'host'), imports=IMPORTS)
        self.returned(result, status)
        self.assertEqual(result.stderr.count(QUERY_MARKER), 1)
        self.assertIn('count 3, source hw.logicalcpu snapshot', result.stderr)

    def test_explicit_count_does_not_call_even_a_failing_query(self):
        result, status = self.boot(args=('--guest-cpu-count', '64'),
                                  env_extra={'PB_TEST_COUNT_FAILURE': '1'})
        self.returned(result, status)
        self.assertNotIn(QUERY_MARKER, result.stderr)

    def test_failed_host_query_stops_before_startup(self):
        result, status = self.boot(args=('--guest-cpu-count', 'host'),
                                  env_extra={'PB_TEST_COUNT_FAILURE': '1'})
        self.assertEqual(result.returncode, 3)
        self.assertIsNone(status)
        self.assertEqual(result.stderr.count(QUERY_MARKER), 1)
        self.assertIn('Darwin count query rejected', result.stderr)
        self.assertNotIn('TSD qualified', result.stderr)

    def test_import_listing_never_queries_or_registers_policy(self):
        result, status = self.boot(b'\x0f\x0b',
            args=('--guest-cpu-count', 'host', '--imports'),
            env_extra={'PB_TEST_COUNT_FAILURE': '1', 'BB_RUNTIME_CPU_MAP': 'not-a-logical-map'})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIsNone(status)
        self.assertNotIn(QUERY_MARKER, result.stderr)
        self.assertNotIn('TSD qualified', result.stderr)
        self.assertNotIn('unpinned logical CPU', result.stderr)

    def test_invalid_duplicate_and_missing_counts(self):
        invalid = ('0', '65', '4294967296', '18446744073709551616', '-1', '+1',
                   '1.0', ' 1', '1 ', '0x2', 'HOST', '')
        arguments = [('--guest-cpu-count', value) for value in invalid]
        arguments += [('--guest-cpu-count',),
                      ('--guest-cpu-count', '1', '--guest-cpu-count', 'host')]
        for args in arguments:
            with self.subTest(args=args):
                result, status = self.boot(args=args)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIsNone(status)
                self.assertNotIn(QUERY_MARKER, result.stderr)
                self.assertNotIn('TSD qualified', result.stderr)

    def test_native_mapping_environment_conflicts_before_query(self):
        for value in ('3', 'host'):
            with self.subTest(value=value):
                result, status = self.boot(args=('--guest-cpu-count', value),
                                          env_extra={'BB_RUNTIME_CPU_MAP': ''})
                self.assertEqual(result.returncode, 2)
                self.assertIsNone(status)
                self.assertIn('cannot be combined with BB_RUNTIME_CPU_MAP', result.stderr)
                self.assertNotIn(QUERY_MARKER, result.stderr)
                self.assertNotIn('TSD qualified', result.stderr)


if __name__ == '__main__':
    unittest.main()
