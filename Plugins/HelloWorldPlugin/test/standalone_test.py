#!/usr/bin/env python3
"""Small package-only smoke test for the standalone HelloWorldPlugin.

The test deliberately uses only Python's standard library and the tools from
the selected package. It does not import llvm-lit or FileCheck.
"""

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


def run(command, env, cwd):
    result = subprocess.run(command, cwd=cwd, env=env, text=True,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError("command failed ({}):\n{}\n{}".format(
            result.returncode, " ".join(command), result.stdout))
    return result.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--plugin-dir", required=True)
    parser.add_argument("--tools-dir", required=True)
    parser.add_argument("--library-dir", default="")
    parser.add_argument("--runtime-dir", default="")
    parser.add_argument("--target-cc", default="")
    parser.add_argument("--target-cxx", required=True)
    parser.add_argument("--target-ld", required=True)
    parser.add_argument("--target-triple", default="")
    parser.add_argument("--target-cxx-flags", default="")
    parser.add_argument("--target-ld-flags", default="")
    args = parser.parse_args()

    plugin_dir = Path(args.plugin_dir).resolve()
    tools_dir = Path(args.tools_dir).resolve()
    env = os.environ.copy()
    runtime_var = ("PATH" if os.name == "nt" else
                   "DYLD_LIBRARY_PATH" if sys.platform == "darwin" else
                   "LD_LIBRARY_PATH")
    runtime_dirs = [str(plugin_dir),
                    args.runtime_dir or str(tools_dir / "bin"),
                    args.library_dir or str(tools_dir / "lib"),
                    env.get(runtime_var, "")]
    env[runtime_var] = os.pathsep.join(path for path in runtime_dirs if path)

    with tempfile.TemporaryDirectory(prefix="eld-plugin-") as temp:
        work = Path(temp)
        source = work / "input.c"
        obj = work / "input.o"
        output = work / "image.out"
        source.write_text("""
            extern int target;
            int target_data = 7;
            int target_function(void) { return target_data; }
            int main(void) { return target_function() + target; }
            int target = 1;
        """)
        compile_command = [args.target_cc or args.target_cxx]
        if args.target_triple:
            compile_command.append("--target=" + args.target_triple)
        compile_command += shlex.split(args.target_cxx_flags)
        compile_command += ["-c", str(source), "-o", str(obj),
                            "-ffunction-sections", "-fdata-sections"]
        run(compile_command, env, work)

        config = """GlobalPlugins:\n  - Type: LinkerPlugin\n    Name: HelloWorldPlugin\n    Library: HelloWorldPlugin\n"""
        config_path = work / "plugins.yaml"
        config_path.write_text(config)
        link_command = [args.target_ld]
        link_command += shlex.split(args.target_ld_flags)
        link_command += [str(obj), "-e", "main", "-o", str(output),
                         "--plugin-config", str(config_path),
                         "--trace=plugin"]
        log = run(link_command, env, work)

        if "HelloWorldPlugin::Init" not in log or "HelloWorldPlugin::Destroy" not in log:
            raise RuntimeError("HelloWorldPlugin lifecycle trace was not observed:\n" + log)


if __name__ == "__main__":
    main()
