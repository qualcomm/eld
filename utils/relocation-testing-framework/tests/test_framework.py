# ===----------------------------------------------------------------------===
# Part of the eld Project, under the BSD License
# See https://github.com/qualcomm/eld/LICENSE.txt for license information.
# SPDX-License-Identifier: BSD-3-Clause
# ===----------------------------------------------------------------------===

"""Unit tests for relocation test generation and configuration validation."""

from pathlib import Path
import sys
import unittest
from unittest import mock


FRAMEWORK_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(FRAMEWORK_DIR))

import framework
import run_analysis


class AddressTargetConfigTests(unittest.TestCase):
    def test_address_test_parameters_are_validated(self):
        self.assertEqual(
            run_analysis.validate_address_test_parameters(
                "data",
                {"scratch_reg": "r4"},
                {"symbol_binding": "global", "symbol_visibility": "default"},
            ),
            ("r4", "global", "default"),
        )

        cases = [
            ({}, {"symbol_binding": "global", "symbol_visibility": "default"},
             "requires a non-empty 'scratch_reg'"),
            ({"scratch_reg": ""},
             {"symbol_binding": "global", "symbol_visibility": "default"},
             "requires a non-empty 'scratch_reg'"),
            ({"scratch_reg": None},
             {"symbol_binding": "global", "symbol_visibility": "default"},
             "requires a non-empty 'scratch_reg'"),
            ({"scratch_reg": "r4"}, {"symbol_visibility": "default"},
             "missing 'symbol_binding' parameter"),
            ({"scratch_reg": "r4"},
             {"symbol_binding": "", "symbol_visibility": "default"},
             "requires a non-empty 'symbol_binding' parameter"),
            ({"scratch_reg": "r4"},
             {"symbol_binding": "globla", "symbol_visibility": "default"},
             "unknown symbol binding: globla"),
            ({"scratch_reg": "r4"}, {"symbol_binding": "global"},
             "missing 'symbol_visibility' parameter"),
            ({"scratch_reg": "r4"},
             {"symbol_binding": "global", "symbol_visibility": "protected"},
             "unknown symbol visibility: protected"),
        ]
        for config, parameters, message in cases:
            with self.subTest(config=config, parameters=parameters):
                with self.assertRaises(ValueError) as error:
                    run_analysis.validate_address_test_parameters(
                        "data", config, parameters)
                self.assertIn(message, str(error.exception))

    def test_unknown_symbol_visibility_reports_value(self):
        with self.assertRaisesRegex(
                ValueError, "unknown symbol visibility: protected"):
            run_analysis.visibility_directive("protected", "v")

    def test_invalid_symbol_binding_is_reported_for_branch_tests(self):
        with self.assertRaisesRegex(
                ValueError, "unknown symbol binding: globla"):
            run_analysis.generate_branch_test(
                "x86_64",
                {},
                {"usage": "call {{func}}"},
                {"symbol_binding": "globla",
                 "symbol_visibility": "default"},
            )

    def test_missing_address_target_template_reports_target(self):
        with self.assertRaisesRegex(
                ValueError,
                "no address template configured for target: interworking"):
            run_analysis.generate_address_test(
                "arm",
                {"address_target_templates": {}},
                {},
                {"address_target": "interworking"},
            )

    def test_string_config_uses_default_symbol_and_usage(self):
        self.assertEqual(
            run_analysis.parse_address_target_config(
                "data", "address.s.in"),
            (
                "address.s.in",
                {"default": "v"},
                {"usage": {"symbol": "v", "label": "single"}},
            ),
        )

    def test_mapping_config_preserves_symbols_and_usages(self):
        config = {
            "template": "address-interworking.s.in",
            "symbols": {"arm": "arm_v", "thumb": "thumb_v"},
            "usages": {
                "arm_usage": {"symbol": "arm_v", "label": "arm"},
                "thumb_usage": {"symbol": "thumb_v", "label": "thumb"},
            },
        }

        self.assertEqual(
            run_analysis.parse_address_target_config("interworking", config),
            (config["template"], config["symbols"], config["usages"]),
        )

    def test_invalid_configs_report_descriptive_errors(self):
        cases = [
            (None, "must be a template name or mapping"),
            ({}, "missing 'template'"),
            ({"template": ""}, "invalid template name"),
            ({"template": None}, "invalid template name"),
            ({"template": "template", "symbols": []},
             "'symbols' must be a mapping"),
            ({"template": "template", "symbols": {1: "v"}},
             "invalid symbol key"),
            ({"template": "template", "usages": []},
             "'usages' must be a mapping"),
            ({"template": "template", "symbols": {"": "v"}},
             "invalid symbol key"),
            ({"template": "template", "symbols": {"arm": ""}},
             "symbol 'arm' must have a non-empty name"),
            ({"template": "template", "symbols": {"arm": None}},
             "symbol 'arm' must have a non-empty name"),
            ({"template": "template", "usages": {"": {"symbol": "v"}}},
             "invalid usage key"),
            ({"template": "template", "usages": {1: {"symbol": "v"}}},
             "invalid usage key"),
            ({"template": "template", "usages": {"usage": None}},
             "usage 'usage' must be a mapping"),
            ({"template": "template", "usages": {"usage": {}}},
             "usage 'usage' missing 'symbol'"),
            ({"template": "template",
              "usages": {"usage": {"symbol": ""}}},
             "usage 'usage' must have a non-empty symbol name"),
            ({"template": "template",
              "usages": {"usage": {"symbol": None}}},
             "usage 'usage' must have a non-empty symbol name"),
            ({"template": "template",
              "usages": {"usage": {"symbol": "v", "label": 1}}},
             "usage 'usage' label must be a string"),
        ]
        for config, message in cases:
            with self.subTest(config=config):
                with self.assertRaises(ValueError) as error:
                    run_analysis.parse_address_target_config(
                        "interworking", config)
                self.assertIn(message, str(error.exception))

    def test_usage_keys_cannot_shadow_generated_placeholders(self):
        cases = [
            ("scratch_reg", {"arm": "arm_v"}),
            ("binding_arm", {"arm": "arm_v"}),
            ("visibility_arm", {"arm": "arm_v"}),
            ("binding", {"default": "v"}),
            ("visibility", {"default": "v"}),
        ]
        for usage_key, symbols in cases:
            with self.subTest(usage_key=usage_key):
                config = {
                    "template": "address-interworking.s.in",
                    "symbols": symbols,
                    "usages": {usage_key: {"symbol": "v"}},
                }

                with self.assertRaisesRegex(
                        ValueError,
                        f"usage keys collide with generated placeholders: "
                        f"{usage_key}"):
                    run_analysis.parse_address_target_config(
                        "interworking", config)

    def test_unresolved_template_placeholder_fails_generation(self):
        config = {
            "scratch_reg": "r4",
            "address_target_templates": {"data": "address.s.in"},
        }
        reloc = {"usage": "ldr {{reg}}, ={{var}}"}
        parameters = {
            "symbol_binding": "global",
            "symbol_visibility": "default",
        }

        with mock.patch.object(run_analysis, "load_template",
                               return_value="{{misspelled}}"):
            with self.assertRaisesRegex(
                    ValueError,
                    "unresolved placeholders in template address.s.in "
                    "for address target data"):
                run_analysis.generate_address_test(
                    "arm", config, reloc, parameters)


class RunMatrixTests(unittest.TestCase):
    def test_address_targets_come_from_template_config(self):
        arch_info = {
            "config": {
                "address_target_templates": {
                    "data": "address.s.in",
                    "interworking": {"template": "address-interworking.s.in"},
                },
            },
            # A stale list must not control which targets are generated.
            "address_targets": ["stale"],
            "relocs": [
                {"name": "R_ARM_ABS32", "category": "address"},
                {"name": "R_ARM_CALL", "category": "branch"},
            ],
        }

        combinations = list(framework.build_run_matrix(arch_info))
        address_targets = {
            combination["address_target"]
            for combination in combinations
            if combination["relocation"]["category"] == "address"
        }
        branch_combinations = [
            combination for combination in combinations
            if combination["relocation"]["category"] == "branch"
        ]

        self.assertEqual(address_targets, {"data", "interworking"})
        self.assertEqual(len(combinations), 18)
        self.assertEqual(len(branch_combinations), 6)
        self.assertTrue(all("address_target" not in combination
                            for combination in branch_combinations))

    def test_missing_relocation_category_does_not_crash_matrix(self):
        arch_info = {
            "config": {},
            "relocs": [{"name": "R_ARM_ABS32", "usage": "ldr"}],
        }

        combinations = list(framework.build_run_matrix(arch_info))

        self.assertEqual(len(combinations), 6)
        self.assertEqual(
            run_analysis.check_reloc_description(
                combinations[0]["relocation"]),
            "missing category",
        )


class RunConfigurationTests(unittest.TestCase):
    def test_missing_or_invalid_tool_commands_are_reported(self):
        invalid_configs = [
            ({}, "compiler"),
            ({"compiler": []}, "compiler"),
            ({"compiler": ["clang", ""]}, "compiler"),
            ({"compiler": "clang"}, "compiler"),
            ({"compiler": ["clang"], "linker": []}, "linker"),
            ({"compiler": ["clang"], "linker": ["ld.eld", ""]},
             "linker"),
            ({"compiler": ["clang"], "linker": "ld.eld"}, "linker"),
            ({"compiler": ["clang"], "linker": ["ld.eld"],
              "readelf": []}, "readelf"),
            ({"compiler": ["clang"], "linker": ["ld.eld"],
              "readelf": [""]}, "readelf"),
            ({"compiler": ["clang"], "linker": ["ld.eld"],
              "readelf": "llvm-readelf"}, "readelf"),
            ({"compiler": ["clang"], "linker": ["ld.eld"],
              "readelf": ["llvm-readelf"], "objdump": []}, "objdump"),
            ({"compiler": ["clang"], "linker": ["ld.eld"],
              "readelf": ["llvm-readelf"], "objdump": [""]}, "objdump"),
            ({"compiler": ["clang"], "linker": ["ld.eld"],
              "readelf": ["llvm-readelf"],
              "objdump": "llvm-objdump"}, "objdump"),
        ]

        for config, missing_tool in invalid_configs:
            with self.subTest(config=config):
                with self.assertRaisesRegex(
                        ValueError,
                        f"run mode requires '{missing_tool}'"):
                    run_analysis.validate_run_config(config)

    def test_tool_commands_are_accepted(self):
        run_analysis.validate_run_config({
            "compiler": ["clang", "-target", "armv7a-none-linux-gnueabi"],
            "linker": ["ld.eld", "-m", "armelf_linux_eabi"],
            "readelf": ["llvm-readelf"],
            "objdump": ["llvm-objdump"],
            "run_timeout_seconds": 5,
        })

    def test_run_prefix_is_optional_or_a_command_list(self):
        config = {
            "compiler": ["clang"],
            "linker": ["ld.eld"],
            "readelf": ["llvm-readelf"],
            "objdump": ["llvm-objdump"],
            "run_timeout_seconds": 5,
        }
        # x86_64 runs the executable directly, without an emulator prefix.
        run_analysis.validate_run_config(config)

        # Cross-architecture runs can provide a prefix such as qemu-arm.
        config["run_prefix"] = ["qemu-arm"]
        run_analysis.validate_run_config(config)

    def test_invalid_run_prefix_is_reported(self):
        invalid_prefixes = [None, "qemu-arm", [""], [1]]
        for run_prefix in invalid_prefixes:
            with self.subTest(run_prefix=run_prefix):
                config = {
                    "compiler": ["clang"],
                    "linker": ["ld.eld"],
                    "readelf": ["llvm-readelf"],
                    "objdump": ["llvm-objdump"],
                    "run_prefix": run_prefix,
                    "run_timeout_seconds": 5,
                }
                with self.assertRaisesRegex(ValueError, "'run_prefix'"):
                    run_analysis.validate_run_config(config)

    def test_invalid_run_timeout_is_reported(self):
        invalid_timeouts = [
            None, 0, -1, True, "5", float("nan"), float("inf"),
            float("-inf"),
        ]
        for timeout in invalid_timeouts:
            with self.subTest(timeout=timeout):
                config = {
                    "compiler": ["clang"],
                    "linker": ["ld.eld"],
                    "readelf": ["llvm-readelf"],
                    "objdump": ["llvm-objdump"],
                    "run_timeout_seconds": timeout,
                }
                with self.assertRaisesRegex(
                        ValueError, "'run_timeout_seconds'"):
                    run_analysis.validate_run_config(config)


if __name__ == "__main__":
    unittest.main()
