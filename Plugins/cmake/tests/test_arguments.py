#!/usr/bin/env python3
# ===----------------------------------------------------------------------===
# Part of the eld Project, under the BSD License
# See https://github.com/qualcomm/eld/LICENSE.txt for license information.
# SPDX-License-Identifier: BSD-3-Clause
# ===----------------------------------------------------------------------===

"""Check native flag parsing and argument transport without target tools."""

import argparse
import json
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import sys
from types import SimpleNamespace
import unittest
from unittest import mock


class TestArguments(unittest.TestCase):
    def parse_flags(self, flags, windows):
        result = subprocess.run([
            cmake, "-DARGUMENT_MODULE=" + str(plugins / "cmake/ELDStandaloneTestArguments.cmake"),
            "-DTEST_WINDOWS=" + ("ON" if windows else "OFF"),
            "-DTEST_FLAGS=" + flags, "-P", str(plugins / "cmake/tests/TestArguments.cmake")],
            check=True, capture_output=True, text=True)
        return json.loads(result.stderr)

    def smoke(self):
        return runpy.run_path(str(plugins / "HelloWorldPlugin/test/standalone_test.py"))

    def lit(self, parameters):
        config = SimpleNamespace(environment=os.environ.copy(), available_features=set(),
                                 substitutions=[])
        parameters = {"eld_obj_root": str(plugins), "llvm_tools_dir": str(plugins),
                      "llvm_libs_dir": str(plugins), "eld_test_target": "Hexagon",
                      **parameters}

        def fatal(message):
            raise ValueError(message)

        lit_config = SimpleNamespace(params=parameters, fatal=fatal, note=lambda _: None)
        formats = SimpleNamespace(ShTest=lambda **_: None)
        with mock.patch.dict(sys.modules, {"lit": SimpleNamespace(formats=formats),
                                           "lit.formats": formats}):
            runpy.run_path(str(plugins / "lit.cfg"),
                           init_globals={"config": config, "lit_config": lit_config})
        return dict(config.substitutions)

    def test_native_syntax(self):
        cases = [
            (True, r'-I"C:\Program Files\SDK\include" -LC:\SDK\lib "-DNAME=\"two words\""',
             [r'-IC:\Program Files\SDK\include', r'-LC:\SDK\lib', '-DNAME="two words"']),
            (False, r"-I'/opt/SDK directory/include' '-DNAME=\"two words\"' '-LC:\\SDK\\lib'",
             ['-I/opt/SDK directory/include', '-DNAME="two words"', r'-LC:\SDK\lib']),
        ]
        for windows, flags, expected in cases:
            with self.subTest(windows=windows):
                self.assertEqual(self.parse_flags(flags, windows), expected)
                self.assertEqual(self.parse_flags('', windows), [])
                self.assertEqual(self.parse_flags('"" "a;b"', windows), ['', 'a;b'])
                self.assertEqual(self.parse_flags('"a\nb\rc\td"', windows), ['a\nb\rc\td'])
        self.assertEqual(self.parse_flags(r'"C:\SDK directory\\"', True),
                         ['C:\\SDK directory\\'])

    def test_smoke_and_lit_receive_same_arguments(self):
        compiler = [r'-IC:\Program Files\SDK\include', '-DNAME="two words"', '-DSET=a;b']
        linker = [r'-LC:\Program Files\SDK\lib', '--no-threads']
        main = self.smoke()['main']
        command = ['standalone_test.py', '--plugin-dir', str(plugins), '--tools-dir', str(plugins),
                   '--target-cxx', 'compiler', '--target-ld', 'linker',
                   '--target-cxx-args=' + json.dumps(compiler),
                   '--target-ld-args=' + json.dumps(linker)]
        with mock.patch.object(sys, 'argv', command), mock.patch.dict(main.__globals__, {
                'run': mock.Mock(return_value='HelloWorldPlugin::Init HelloWorldPlugin::Destroy')
        }):
            main()
            calls = main.__globals__['run'].call_args_list
            self.assertEqual(calls[0].args[0][1:1 + len(compiler)], compiler)
            self.assertEqual(calls[1].args[0][1:1 + len(linker)], linker)
        substitutions = self.lit({'target_cxx_args': json.dumps(compiler),
                                  'target_ld_args': json.dumps(linker)})
        self.assertEqual(shlex.split(substitutions['%clangopts'])[1:], compiler)
        self.assertEqual(shlex.split(substitutions['%linkopts'])[3:], linker)

    def test_direct_smoke_arguments(self):
        main = self.smoke()['main']
        with mock.patch.object(sys, 'argv', [
                'standalone_test.py', '--plugin-dir', str(plugins), '--tools-dir', str(plugins),
                '--target-cxx', 'compiler', '--target-ld', 'linker',
                r'--target-cxx-arg=-IC:\Program Files\SDK', '--target-ld-arg=--no-threads'
        ]), mock.patch.dict(main.__globals__, {
                'run': mock.Mock(return_value='HelloWorldPlugin::Init HelloWorldPlugin::Destroy')
        }):
            main()
            calls = main.__globals__['run'].call_args_list
            self.assertEqual(calls[0].args[0][1], r'-IC:\Program Files\SDK')
            self.assertEqual(calls[1].args[0][1], '--no-threads')

    def test_legacy_direct_lit_flags(self):
        substitutions = self.lit({'target_cxx_flags': "-DNAME='two words'",
                                  'target_ld_flags': "-L'/opt/SDK directory/lib'"})
        self.assertEqual(shlex.split(substitutions['%clangopts'])[1:], ['-DNAME=two words'])
        self.assertEqual(shlex.split(substitutions['%linkopts'])[3:], ['-L/opt/SDK directory/lib'])

    def test_argument_arrays_override_legacy_flags(self):
        substitutions = self.lit({'target_cxx_args': '[]', 'target_ld_args': '[]',
                                  'target_cxx_flags': '-G0', 'target_ld_flags': '--no-threads'})
        self.assertEqual(len(shlex.split(substitutions['%clangopts'])), 1)
        self.assertEqual(shlex.split(substitutions['%linkopts']),
                         ['--thread-count', '4', '--emit-relocs'])

    def test_reject_invalid_argument_arrays(self):
        for encoded in ('null', '"-G0"', '[1]', '{', '[null]'):
            with self.subTest(encoded=encoded):
                with self.assertRaises(argparse.ArgumentTypeError):
                    self.smoke()['argument_list'](encoded)
                with self.assertRaises(ValueError):
                    self.lit({'target_cxx_args': encoded})
                with self.assertRaises(ValueError):
                    self.lit({'target_ld_args': encoded})


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--cmake', required=True)
    parser.add_argument('--plugins-dir', required=True)
    options = parser.parse_args()
    cmake = options.cmake
    plugins = Path(options.plugins_dir).resolve()
    unittest.main(argv=[sys.argv[0]])
