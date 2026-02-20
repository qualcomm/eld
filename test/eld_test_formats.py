import os

import lit.Test
import lit.formats
import lit.util
from lit.TestRunner import (
    _parseKeywords,
    applySubstitutions,
    getDefaultSubstitutions,
    getTempPaths,
)

from thin_archive_test_format import ThinArchiveTestFormat


class EldShellTestFormat(lit.formats.ShTest):
    def __init__(self, execute_external):
        super().__init__(execute_external)

    def execute(self, test, litConfig):
        if test.getSourcePath().endswith(".sh"):
            return self._execute_shell_script(test, litConfig)
        return super().execute(test, litConfig)

    def _execute_shell_script(self, test, litConfig):
        if test.config.unsupported:
            return lit.Test.Result(lit.Test.UNSUPPORTED, "Test is unsupported")
        script = test.getSourcePath()
        cwd = test.getExecPath()
        execdir = os.path.dirname(cwd)
        if not os.path.isdir(execdir):
            os.makedirs(execdir, exist_ok=True)
        env = dict(test.config.environment)
        shell = litConfig.getBashPath()
        if not shell:
            shell = "bash"
        tmpDir, tmpBase = getTempPaths(test)
        substitutions = getDefaultSubstitutions(test, tmpDir, tmpBase)
        conditions = {feature: True for feature in test.config.available_features}
        with open(script, "r", encoding="utf-8", errors="surrogateescape") as f:
            script_contents = f.read()
        has_trailing_newline = script_contents.endswith("\n")
        start_marker = "__LIT_SCRIPT_LINE_START__"
        end_marker = "__LIT_SCRIPT_LINE_END__"
        script_lines = [
            f"{start_marker}{line}{end_marker}"
            for line in script_contents.splitlines()
        ]
        try:
            expanded_lines = applySubstitutions(
                script_lines,
                substitutions,
                conditions,
                recursion_limit=test.config.recursiveExpansionLimit,
            )
        except ValueError as exc:
            return lit.Test.Result(lit.Test.UNRESOLVED, str(exc))
        expanded_lines = [
            line[len(start_marker) : -len(end_marker)]
            if line.startswith(start_marker) and line.endswith(end_marker)
            else line
            for line in expanded_lines
        ]
        expanded_script = "\n".join(expanded_lines)
        if has_trailing_newline:
            expanded_script += "\n"
        expanded_script_path = tmpBase + ".sh"
        os.makedirs(os.path.dirname(expanded_script_path), exist_ok=True)
        with open(
            expanded_script_path, "w", encoding="utf-8", errors="surrogateescape"
        ) as f:
            f.write(expanded_script)
        try:
            parsed = _parseKeywords(expanded_script_path, [], require_script=False)
        except ValueError as exc:
            return lit.Test.Result(lit.Test.UNRESOLVED, str(exc))
        test.xfails += parsed["XFAIL:"] or []
        if test.exclude_xfail and test.isExpectedToFail():
            return lit.Test.Result(lit.Test.EXCLUDED, "excluding XFAIL tests")
        test.requires += parsed["REQUIRES:"] or []
        test.unsupported += parsed["UNSUPPORTED:"] or []
        if parsed["ALLOW_RETRIES:"]:
            test.allowed_retries = parsed["ALLOW_RETRIES:"][0]
        missing_required_features = test.getMissingRequiredFeatures()
        if missing_required_features:
            msg = ", ".join(missing_required_features)
            return lit.Test.Result(
                lit.Test.UNSUPPORTED,
                "Test requires the following unavailable features: %s" % msg,
            )
        unsupported_features = test.getUnsupportedFeatures()
        if unsupported_features:
            msg = ", ".join(unsupported_features)
            return lit.Test.Result(
                lit.Test.UNSUPPORTED,
                "Test does not support the following features and/or targets: %s" % msg,
            )
        if not test.isWithinFeatureLimits():
            msg = ", ".join(test.config.limit_to_features)
            return lit.Test.Result(
                lit.Test.UNSUPPORTED,
                "Test does not require any of the features specified in limit_to_features: %s"
                % msg,
            )
        if litConfig.noExecute:
            return lit.Test.Result(lit.Test.PASS, "")
        cmd = [shell, expanded_script_path]
        out, err, exitCode = lit.util.executeCommand(
            cmd, env=env, cwd=execdir, timeout=litConfig.maxIndividualTestTime)
        output = out + err
        if exitCode != 0:
            return lit.Test.Result(lit.Test.FAIL, output)
        return lit.Test.Result(lit.Test.PASS, output)


def get_test_format(config, execute_external):
    base_format = EldShellTestFormat(execute_external)
    if config.eld_option_name == "thin_archives":
        return ThinArchiveTestFormat(execute_external, base_format)
    return base_format
