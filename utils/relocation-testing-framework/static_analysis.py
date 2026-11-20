#"===------------------------------------------------------------------------===
# Part of the eld Project, under the BSD License
# See https://github.com/qualcomm/eld/LICENSE.txt for license information.
# SPDX-License-Identifier: BSD-3-Clause
#"===------------------------------------------------------------------------===

"""Static relocation testing."""

from pathlib import Path
import re
import sys

import run_analysis


def run(arch_name, arch_info, output_dir):
    if arch_name != "arm":
        print(f"static-pass mode not yet implemented for arch '{arch_name}'",
              file=sys.stderr)
        return True

    reloc = next(
        (reloc for reloc in arch_info["relocs"]
         if reloc.get("name") == "R_ARM_ABS32"),
        None,
    )
    if reloc is None:
        print("R_ARM_ABS32 is not configured for ARM", file=sys.stderr)
        return False

    try:
        return validate_arm_abs32(arch_name, arch_info, reloc, output_dir)
    except Exception as exc:
        print(f"R_ARM_ABS32 static analysis: FAIL: {exc}", file=sys.stderr)
        return False


def validate_arm_abs32(arch_name, arch_info, reloc, output_dir):
    """Validate the happy-path R_ARM_ABS32 relocation result."""

    config = arch_info["config"]
    reloc_dir = Path(output_dir) / "static" / reloc["name"]
    reloc_dir.mkdir(parents=True, exist_ok=True)

    source_path = reloc_dir / "main.s"
    usage = run_analysis.render(
        reloc["static_usage"],
        {"var": "v"},
    )
    source = run_analysis.render(
        run_analysis.load_template(arch_name, "address-static.s.in"),
        {"usage": usage},
    )
    source_path.write_text(source)

    primary_obj = source_path.with_suffix(".o")

    result = run_analysis.run_command(
        config["compiler"] + [
            "-c",
            str(source_path),
            "-o",
            str(primary_obj),
        ],
        reloc_dir,
    )
    run_analysis.append_log(reloc_dir / "assemble.log", result)

    if result.returncode != 0:
        print(f"{reloc['name']}: ASM FAIL", file=sys.stderr)
        return False

    reloc_offset = find_relocation_offset(
        primary_obj, reloc["name"], config, reloc_dir
    )

    _, input_text_offset = find_section_info(
        primary_obj, ".text", config, reloc_dir
    )

    # R_ARM_ABS32 uses the word at the relocation location as the implicit
    # addend A.
    addend = read_word(
        primary_obj,
        input_text_offset + reloc_offset,
        arch_info["bits"],
        arch_info["endianness"],
    )

    executable = reloc_dir / "a.out"

    result = run_analysis.run_command(
        config["linker"] + [
            str(primary_obj),
            "-o",
            str(executable),
        ],
        reloc_dir,
    )
    run_analysis.append_log(reloc_dir / "link.log", result)

    if result.returncode != 0:
        print(f"{reloc['name']}: LINK FAIL", file=sys.stderr)
        return False

    symbol_value = find_symbol_value(
        executable, "v", config, reloc_dir
    )

    reloc_site = find_symbol_value(
        executable, "reloc_site", config, reloc_dir
    )

    output_text_addr, output_text_offset = find_section_info(
        executable, ".text", config, reloc_dir
    )

    relocated_file_offset = (
        output_text_offset + (reloc_site - output_text_addr)
    )

    actual = read_word(
        executable,
        relocated_file_offset,
        arch_info["bits"],
        arch_info["endianness"],
    )

    mask = (1 << arch_info["bits"]) - 1
    expected = (symbol_value + addend) & mask

    passed = actual == expected

    report = "\n".join([
        f"# {reloc['name']} static analysis",
        "",
        f"- Relocation offset: `0x{reloc_offset:x}`",
        f"- Relocation site: `0x{reloc_site:08x}`",
        f"- Symbol S: `0x{symbol_value:08x}`",
        f"- Addend A: `0x{addend:08x}`",
        f"- Expected S + A: `0x{expected:08x}`",
        f"- Actual relocated value: `0x{actual:08x}`",
        f"- Result: {'PASS' if passed else 'FAIL'}",
        "",
    ])

    (reloc_dir / "report.md").write_text(report)
    print(report)

    return passed


def run_readelf(config, args, path, log_dir):
    result = run_analysis.run_command(
        config["readelf"] + args + [str(path)],
        log_dir,
    )
    run_analysis.append_log(log_dir / "readelf.log", result)

    if result.returncode != 0:
        raise RuntimeError(f"llvm-readelf failed for {path}")

    return result.stdout


def find_relocation_offset(obj, reloc_name, config, log_dir):
    output = run_readelf(config, ["-rW"], obj, log_dir)

    for line in output.splitlines():
        if reloc_name not in line:
            continue

        fields = line.split()
        if fields:
            return int(fields[0], 16)

    raise RuntimeError(
        f"{reloc_name} relocation not found in {obj}"
    )


def find_section_info(path, section_name, config, log_dir):
    output = run_readelf(config, ["-SW"], path, log_dir)

    pattern = re.compile(
        r"^\s*\[\s*\d+\]\s+"
        + re.escape(section_name)
        + r"\s+\S+\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+"
    )

    for line in output.splitlines():
        match = pattern.match(line)
        if match:
            return int(match.group(1), 16), int(match.group(2), 16)

    raise RuntimeError(
        f"section {section_name} not found in {path}"
    )


def find_symbol_value(path, symbol_name, config, log_dir):
    output = run_readelf(config, ["-sW"], path, log_dir)

    for line in output.splitlines():
        fields = line.split()

        if len(fields) < 8:
            continue

        if fields[-1] != symbol_name:
            continue

        if not fields[0].endswith(":"):
            continue

        return int(fields[1], 16)

    raise RuntimeError(
        f"symbol {symbol_name} not found in {path}"
    )


def read_word(path, file_offset, bits, endianness):
    size = bits // 8

    with Path(path).open("rb") as file:
        file.seek(file_offset)
        data = file.read(size)

    if len(data) != size:
        raise RuntimeError(
            f"could not read {size} bytes at file offset 0x{file_offset:x} "
            f"from {path}"
        )

    return int.from_bytes(data, byteorder=endianness)
