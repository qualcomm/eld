#"===------------------------------------------------------------------------===
# Part of the eld Project, under the BSD License
# See https://github.com/qualcomm/eld/LICENSE.txt for license information.
# SPDX-License-Identifier: BSD-3-Clause
#"===------------------------------------------------------------------------===

"""Runtime relocation testing."""

from dataclasses import dataclass
from pathlib import Path
import subprocess
import sys


EXPECTED_EXIT_CODE = {
    "address": 42,
    "branch": 7,
}


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
    reloc_dir = report_dir / sanitize(reloc.get("name", "unknown"))
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
    return TestResult(parameters, report_name, reloc["name"], "PASS",
                      invariant_results)


def check_reloc_description(reloc):
    if "name" not in reloc:
        return "missing name"
    if reloc.get("category") not in ("address", "branch"):
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
    """Generate a data-symbol reference test for address relocations."""
    usage = render(reloc["usage"], {
        "var": "v",
        "reg": config["scratch_reg"],
    })
    source = render(load_template(arch_name, "address.s.in"), {
        "binding": parameters["symbol_binding"],
        "scratch_reg": config["scratch_reg"],
        "usage": usage,
        "visibility": visibility_directive(parameters["symbol_visibility"], "v"),
    })
    return {"main.s": source}, "main.s"


def generate_branch_test(arch_name, config, reloc, parameters):
    """Generate caller and target sources for branch relocations."""
    usage = render(reloc["usage"], {"func": "target_func"})
    if parameters["symbol_binding"] == "local":
        source = render(load_template(arch_name, "branch-local.s.in"), {
            "usage": usage,
            "visibility": visibility_directive(parameters["symbol_visibility"],
                                               "target_func"),
        })
        return {"main.s": source}, "main.s"
    caller = render(load_template(arch_name, "branch.s.in"), {"usage": usage})
    target = render(load_template(arch_name, "branch-target.s.in"), {
        "binding": parameters["symbol_binding"],
        "visibility": visibility_directive(parameters["symbol_visibility"],
                                           "target_func"),
    })
    return {"caller.s": caller, "target.s": target}, "caller.s"


def visibility_directive(visibility, symbol):
    if visibility == "default":
        return ""
    if visibility == "hidden":
        return f".hidden {symbol}"
    raise ValueError(f"unknown symbol visibility: {visibility}")


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
    result = run_command(config["run_prefix"] + [str(executable)],
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
    return TestResult(parameters, report_name, reloc.get("name", "<unknown>"),
                      build_status, [InvariantResult("exit_code_check", False,
                                                    "SKIP")])


def result_passed(result):
    return result.build_status == "PASS" and all(
        invariant.passed for invariant in result.invariant_results)


def sanitize(name):
    """Return a filesystem-safe name for generated artifact directories."""
    return "".join(c if c.isalnum() or c in "._-" else "_" for c in name)
