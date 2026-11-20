#"===------------------------------------------------------------------------===
# Part of the eld Project, under the BSD License
# See https://github.com/qualcomm/eld/LICENSE.txt for license information.
# SPDX-License-Identifier: BSD-3-Clause
#"===------------------------------------------------------------------------===

"""Runtime relocation testing."""

from dataclasses import dataclass
import math
from pathlib import Path
import subprocess
import sys


EXPECTED_EXIT_CODE = {
    "address": 42,
    "branch": 7,
}


def validate_run_config(config):
    for tool in ("compiler", "linker", "readelf", "objdump"):
        command = config.get(tool)
        if not isinstance(command, list) or not command or any(
                not isinstance(argument, str) or not argument
                for argument in command):
            raise ValueError(
                f"run mode requires '{tool}' to be a non-empty list of "
                "command arguments"
            )

    run_prefix = config.get("run_prefix", [])
    if not isinstance(run_prefix, list) or any(
            not isinstance(argument, str) or not argument
            for argument in run_prefix):
        raise ValueError("'run_prefix' must be a list of command arguments")

    timeout = config.get("run_timeout_seconds")
    if (isinstance(timeout, bool) or not isinstance(timeout, (int, float)) or
            not math.isfinite(timeout) or timeout <= 0):
        raise ValueError("'run_timeout_seconds' must be a positive number")


@dataclass
class CommandResult:
    argv: list
    returncode: int
    stdout: str
    stderr: str
    timed_out: bool = False


@dataclass
class InvariantResult:
    name: str
    passed: bool
    detail: str = ""


@dataclass
class TestResult:
    parameters: dict
    report_name: str
    relocation: str
    build_status: str
    invariant_results: list


def run(arch_name, config, combination, report_name, report_dir, verbose=False):
    reloc = combination["relocation"]
    parameters = {key: value for key, value in combination.items()
                  if key != "relocation"}
    return validate_relocation(arch_name, config, reloc, parameters, report_name,
                               Path(report_dir), verbose)


def load_template(arch_name, template_name):
    base = Path(__file__).resolve().parent
    return (base / "templates" / arch_name / template_name).read_text()


def validate_relocation(arch_name, config, reloc, parameters, report_name,
                        report_dir, verbose):
    category = reloc.get("category", "")
    case_name = relocation_case_name(reloc, parameters)
    reloc_dir = report_dir / sanitize(case_name)
    reloc_dir.mkdir(parents=True, exist_ok=True)

    error = check_reloc_description(reloc)
    if error:
        return make_result(parameters, report_name, reloc, "GEN FAIL: " + error)

    try:
        sources, primary_source = generate_sources(arch_name, config, reloc,
                                                   parameters)
    except Exception as exc:
        return make_result(parameters, report_name, reloc,
                           "GEN FAIL: " + str(exc))

    objects = []
    primary_obj = None
    for source_name, source_text in sources.items():
        source_path = reloc_dir / source_name
        source_path.write_text(source_text)
        obj_path = source_path.with_suffix(".o")
        result = run_command(config["compiler"] + ["-c", str(source_path),
                                                   "-o", str(obj_path)],
                             reloc_dir, verbose=verbose)
        append_log(reloc_dir / "assemble.log", result)
        if result.returncode != 0:
            return make_result(parameters, report_name, reloc, "ASM FAIL")
        objects.append(obj_path)
        if source_name == primary_source:
            primary_obj = obj_path

    if not check_object_has_relocation(primary_obj, reloc["name"], config,
                                       reloc_dir, verbose):
        return make_result(parameters, report_name, reloc, "RELOC MISSING")

    executable = reloc_dir / "a.out"
    result = run_command(config["linker"] + [str(obj) for obj in objects] +
                         ["-o", str(executable)], reloc_dir, verbose=verbose)
    append_log(reloc_dir / "link.log", result)
    if result.returncode != 0:
        return make_result(parameters, report_name, reloc, "LINK FAIL")

    context = {
        "category": category,
        "config": config,
        "expected_exit_code": EXPECTED_EXIT_CODE[category],
        "log_dir": reloc_dir,
        "parameters": parameters,
        "reloc": reloc,
        "verbose": verbose,
    }
    invariant_results = [exit_code_check(executable, context)]
    return TestResult(parameters, report_name, case_name, "PASS",
                      invariant_results)


def check_reloc_description(reloc):
    if "name" not in reloc:
        return "missing name"
    if "category" not in reloc:
        return "missing category"
    if reloc["category"] not in ("address", "branch"):
        return "unknown category"
    if "usage" not in reloc:
        return "missing usage"
    return ""


def generate_sources(arch_name, config, reloc, parameters):
    """Generate assembly sources needed to exercise one relocation."""
    if reloc["category"] == "address":
        return generate_address_test(arch_name, config, reloc, parameters)
    return generate_branch_test(arch_name, config, reloc, parameters)


def generate_address_test(arch_name, config, reloc, parameters):
    """Generate an address test using the configured target template."""
    address_target = parameters.get("address_target", "data")
    address_templates = config.get("address_target_templates", {})
    target_config = address_templates.get(address_target)
    if target_config is None:
        raise ValueError("no address template configured for target: "
                         f"{address_target}")

    template_name, symbols, usages = parse_address_target_config(
        address_target, target_config)
    scratch_reg, symbol_binding, symbol_visibility = (
        validate_address_test_parameters(address_target, config, parameters)
    )

    # Supply binding and visibility values for every symbol named by the
    # template configuration.
    template_values = {"scratch_reg": scratch_reg}
    for name, symbol in symbols.items():
        suffix = "" if name == "default" else f"_{name}"
        template_values[f"binding{suffix}"] = symbol_binding
        template_values[f"visibility{suffix}"] = visibility_directive(
            symbol_visibility, symbol)

    # Render the relocation usage separately for each configured symbol,
    # scratch register, and label.
    for placeholder, usage_config in usages.items():
        template_values[placeholder] = render(reloc["usage"], {
            "var": usage_config["symbol"],
            "reg": scratch_reg,
            "label": usage_config.get("label", placeholder),
        })

    # The architecture template composes the populated usages and symbols
    # into the test source.
    source = render(load_template(arch_name, template_name), template_values)
    if "{{" in source:
        raise ValueError(
            f"unresolved placeholders in template {template_name} "
            f"for address target {address_target}"
        )
    return {"main.s": source}, "main.s"


def validate_address_test_parameters(address_target, config, parameters):
    """Validate architecture and matrix values required by address tests."""
    scratch_reg = config.get("scratch_reg")
    if not isinstance(scratch_reg, str) or not scratch_reg.strip():
        raise ValueError(
            f"address target '{address_target}' requires a non-empty "
            "'scratch_reg' in the architecture config"
        )

    if "symbol_binding" not in parameters:
        raise ValueError(
            f"address target '{address_target}' missing 'symbol_binding' "
            "parameter"
        )
    symbol_binding = parameters["symbol_binding"]
    if not isinstance(symbol_binding, str) or not symbol_binding.strip():
        raise ValueError(
            f"address target '{address_target}' requires a non-empty "
            "'symbol_binding' parameter"
        )
    validate_symbol_binding(symbol_binding)

    if "symbol_visibility" not in parameters:
        raise ValueError(
            f"address target '{address_target}' missing 'symbol_visibility' "
            "parameter"
        )
    symbol_visibility = parameters["symbol_visibility"]
    validate_symbol_visibility(symbol_visibility)

    return scratch_reg, symbol_binding, symbol_visibility


def parse_address_target_config(address_target, target_config):
    """Validate an address target config and return its template metadata."""
    # String entries retain the simple one-symbol configuration. Mapping
    # entries can name several symbols and usage placeholders for richer tests.
    if isinstance(target_config, str):
        template_name = target_config
        symbols = {"default": "v"}
        usages = {"usage": {"symbol": "v", "label": "single"}}
    else:
        if not isinstance(target_config, dict):
            raise ValueError(
                f"address target '{address_target}' must be a template name "
                "or mapping"
            )
        if "template" not in target_config:
            raise ValueError(
                f"address target '{address_target}' missing 'template'"
            )
        template_name = target_config["template"]
        symbols = target_config.get("symbols", {"default": "v"})
        usages = target_config.get(
            "usages", {"usage": {"symbol": "v", "label": "single"}})

    if not isinstance(template_name, str) or not template_name:
        raise ValueError(
            f"address target '{address_target}' has an invalid template name"
        )
    if not isinstance(symbols, dict):
        raise ValueError(
            f"address target '{address_target}' 'symbols' must be a mapping"
        )
    if not isinstance(usages, dict):
        raise ValueError(
            f"address target '{address_target}' 'usages' must be a mapping"
        )
    for name, symbol in symbols.items():
        if not isinstance(name, str) or not name:
            raise ValueError(
                f"address target '{address_target}' has an invalid symbol key"
            )
        if not isinstance(symbol, str) or not symbol:
            raise ValueError(
                f"address target '{address_target}' symbol '{name}' must have "
                "a non-empty name"
            )
    for placeholder, usage_config in usages.items():
        if not isinstance(placeholder, str) or not placeholder:
            raise ValueError(
                f"address target '{address_target}' has an invalid usage key"
            )
        if not isinstance(usage_config, dict):
            raise ValueError(
                f"address target '{address_target}' usage '{placeholder}' "
                "must be a mapping"
            )
        if "symbol" not in usage_config:
            raise ValueError(
                f"address target '{address_target}' usage '{placeholder}' "
                "missing 'symbol'"
            )
        if (not isinstance(usage_config["symbol"], str)
                or not usage_config["symbol"]):
            raise ValueError(
                f"address target '{address_target}' usage '{placeholder}' "
                "must have a non-empty symbol name"
            )
        if ("label" in usage_config
                and not isinstance(usage_config["label"], str)):
            raise ValueError(
                f"address target '{address_target}' usage '{placeholder}' "
                "label must be a string"
            )

    reserved_placeholders = {"scratch_reg"}
    for name in symbols:
        suffix = "" if name == "default" else f"_{name}"
        reserved_placeholders.add(f"binding{suffix}")
        reserved_placeholders.add(f"visibility{suffix}")
    collisions = sorted(reserved_placeholders.intersection(usages))
    if collisions:
        raise ValueError(
            f"address target '{address_target}' usage keys collide with "
            "generated placeholders: " + ", ".join(collisions)
        )

    return template_name, symbols, usages


def generate_branch_test(arch_name, config, reloc, parameters):
    """Generate caller and target sources for branch relocations."""
    symbol_binding = parameters["symbol_binding"]
    validate_symbol_binding(symbol_binding)
    usage = render(reloc["usage"], {"func": "target_func"})
    if symbol_binding == "local":
        source = render(load_template(arch_name, "branch-local.s.in"), {
            "usage": usage,
            "visibility": visibility_directive(parameters["symbol_visibility"],
                                               "target_func"),
        })
        return {"main.s": source}, "main.s"
    caller = render(load_template(arch_name, "branch.s.in"), {"usage": usage})
    target = render(load_template(arch_name, "branch-target.s.in"), {
        "binding": symbol_binding,
        "visibility": visibility_directive(parameters["symbol_visibility"],
                                           "target_func"),
    })
    return {"caller.s": caller, "target.s": target}, "caller.s"


def visibility_directive(visibility, symbol):
    validate_symbol_visibility(visibility)
    if visibility == "default":
        return ""
    return f".hidden {symbol}"


def validate_symbol_visibility(visibility):
    if visibility not in ("default", "hidden"):
        raise ValueError(f"unknown symbol visibility: {visibility}")


def validate_symbol_binding(binding):
    if binding not in ("global", "local", "weak"):
        raise ValueError(f"unknown symbol binding: {binding}")


def render(template, values):
    for key, value in values.items():
        template = template.replace("{{" + key + "}}", value)
    return template


def run_command(argv, cwd, timeout=None, verbose=False):
    if verbose:
        print("$ " + " ".join(argv), file=sys.stderr)
    try:
        completed = subprocess.run(argv, cwd=cwd, text=True,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, timeout=timeout)
        result = CommandResult(argv, completed.returncode, completed.stdout,
                               completed.stderr)
    except subprocess.TimeoutExpired as exc:
        result = CommandResult(argv, -1, exc.stdout or "", exc.stderr or "",
                               timed_out=True)
    if verbose:
        if result.stdout:
            print(result.stdout, file=sys.stderr, end="")
        if result.stderr:
            print(result.stderr, file=sys.stderr, end="")
    return result


def append_log(path, result):
    with path.open("a") as log:
        log.write("$ " + " ".join(result.argv) + "\n")
        if result.timed_out:
            log.write("timed out\n")
        if result.stdout:
            log.write(result.stdout)
        if result.stderr:
            log.write(result.stderr)
        log.write(f"exit: {result.returncode}\n")


def check_object_has_relocation(obj, reloc_name, config, log_dir, verbose):
    result = run_command(config["readelf"] + ["-r", str(obj)], log_dir,
                         verbose=verbose)
    append_log(log_dir / "readelf.log", result)
    return result.returncode == 0 and reloc_name in result.stdout


def exit_code_check(executable, context):
    config = context["config"]
    result = run_command(config.get("run_prefix", []) + [str(executable)],
                         context["log_dir"],
                         timeout=config["run_timeout_seconds"],
                         verbose=context["verbose"])
    append_log(context["log_dir"] / "run.log", result)
    expected = context["expected_exit_code"]
    if result.timed_out:
        return InvariantResult("exit_code_check", False, "timeout")
    if result.returncode != expected:
        return InvariantResult("exit_code_check", False,
                               f"expected {expected}, got {result.returncode}")
    return InvariantResult("exit_code_check", True)


def make_result(parameters, report_name, reloc, build_status):
    return TestResult(parameters, report_name,
                      relocation_case_name(reloc, parameters),
                      build_status, [InvariantResult("exit_code_check", False,
                                                    "SKIP")])


def relocation_case_name(reloc, parameters):
    name = reloc.get("name", "<unknown>")
    address_target = parameters.get("address_target")
    if address_target not in (None, "data"):
        return f"{name} ({address_target.title()} target)"
    return name


def result_passed(result):
    return result.build_status == "PASS" and all(
        invariant.passed for invariant in result.invariant_results)


def sanitize(name):
    """Return a filesystem-safe name for generated artifact directories."""
    return "".join(c if c.isalnum() or c in "._-" else "_" for c in name)
