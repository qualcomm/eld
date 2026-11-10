import json
from pathlib import Path

import pytest

# Test modules override this with a generated version label, such as "main" or
# "23.x". Modules without TEST_BRANCH run against every generated version.
TEST_BRANCH = "all"


def pytest_addoption(parser):
    group = parser.getgroup("eld documentation")
    group.addoption(
        "--docs-dir",
        help="Path to a single built eld-userguide HTML directory",
    )
    group.addoption(
        "--docs-site-dir",
        help="Path to the multiversion site containing versions.json",
    )


def pytest_configure(config):
    docs_dir_option = config.getoption("--docs-dir")
    docs_site_option = config.getoption("--docs-site-dir")
    # Select either one explicit HTML root or a complete multiversion site.
    if bool(docs_dir_option) == bool(docs_site_option):
        raise pytest.UsageError("Pass exactly one of --docs-dir or --docs-site-dir")

    if docs_dir_option:
        docs_dir = Path(docs_dir_option).resolve()
        versions = [(docs_dir.name, docs_dir)]
    else:
        site_dir = Path(docs_site_option).resolve()
        manifest_path = site_dir / "versions.json"
        # Use the generated manifest instead of scanning directories, so aliases
        # such as "stable" and stale outputs are not tested as extra versions.
        try:
            manifest = json.loads(manifest_path.read_text())
            versions = [
                (version["path"], site_dir / version["path"])
                for version in manifest["versions"]
            ]
        except (OSError, ValueError, KeyError, TypeError) as error:
            raise pytest.UsageError(
                f"Cannot read documentation versions from {manifest_path}: {error}"
            ) from error
        if not versions:
            raise pytest.UsageError(f"No documentation versions found in {manifest_path}")

    # Report missing build outputs before collecting or launching browser tests.
    for label, docs_dir in versions:
        if not docs_dir.is_dir():
            raise pytest.UsageError(f"Documentation directory not found: {docs_dir}")
    config.eld_docs_versions = versions
    config.eld_docs_results = {}
    config.eld_docs_current_branch = None


def pytest_generate_tests(metafunc):
    if "docs_dir" not in metafunc.fixturenames:
        return

    # Branch selection is shared by every test in the declaring test module.
    test_branch = getattr(metafunc.module, "TEST_BRANCH", TEST_BRANCH)
    if not isinstance(test_branch, str) or not test_branch:
        raise pytest.UsageError(
            f"{metafunc.module.__file__}: TEST_BRANCH must be a nonempty string "
            "containing a version label or 'all'"
        )

    versions = metafunc.config.eld_docs_versions
    # An explicit --docs-dir takes precedence over the module's branch label.
    # Site mode matches output labels, not Git names such as "release/23.x".
    if metafunc.config.getoption("--docs-site-dir") and test_branch != "all":
        versions = [
            (label, docs_dir) for label, docs_dir in versions if label == test_branch
        ]
    # Version IDs distinguish results and browser artifacts. An empty selection
    # uses pytest's default behavior of skipping the parametrized test.
    metafunc.parametrize(
        "docs_dir",
        [docs_dir for label, docs_dir in versions],
        ids=[label for label, docs_dir in versions],
        indirect=True,
        scope="session",
    )


def _docs_branch(item):
    if not hasattr(item, "callspec") or "docs_dir" not in item.callspec.params:
        return None

    # Resolve the parametrized HTML root to its manifest label. Empty branch
    # selections use the requested TEST_BRANCH label for their skipped result.
    version_dir = item.callspec.params["docs_dir"]
    return next(
        (
            label
            for label, docs_dir in item.config.eld_docs_versions
            if docs_dir == version_dir
        ),
        getattr(item.module, "TEST_BRANCH", TEST_BRANCH),
    )


@pytest.hookimpl(tryfirst=True)
def pytest_runtest_setup(item):
    branch = _docs_branch(item)
    if branch is None or branch == item.config.eld_docs_current_branch:
        return
    item.config.eld_docs_current_branch = branch
    terminalreporter = item.config.pluginmanager.getplugin("terminalreporter")
    if terminalreporter is not None:
        terminalreporter.write_line(f"Testing documentation branch: {branch}")


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item):
    hook_result = yield
    report = hook_result.get_result()
    branch = _docs_branch(item)
    if branch is None:
        return

    if report.failed:
        outcome = "failed"
    elif report.skipped:
        outcome = "skipped"
    elif report.when == "call" and report.passed:
        outcome = "passed"
    else:
        return

    # Count each test once, retaining the most severe setup/call/teardown result.
    # A teardown error changes a passing test to failed rather than adding a test.
    priorities = {"passed": 0, "skipped": 1, "failed": 2}
    previous = item.config.eld_docs_results.get(item.nodeid)
    if previous is None or priorities[outcome] > priorities[previous[1]]:
        item.config.eld_docs_results[item.nodeid] = (branch, outcome)


def pytest_terminal_summary(terminalreporter, exitstatus, config):
    if config.option.collectonly:
        return

    # Include every generated version, even when branch filters select no tests.
    counts_by_branch = {
        label: {"passed": 0, "failed": 0, "skipped": 0}
        for label, docs_dir in config.eld_docs_versions
    }
    for branch, outcome in config.eld_docs_results.values():
        counts = counts_by_branch.setdefault(
            branch, {"passed": 0, "failed": 0, "skipped": 0}
        )
        counts[outcome] += 1

    branch_width = max(len("Branch"), *(len(branch) for branch in counts_by_branch))
    terminalreporter.write_sep("=", "Documentation tests by branch")
    terminalreporter.write_line(
        f"{'Branch':<{branch_width}} {'Status':<8} {'Passed':>6} "
        f"{'Failed':>6} {'Skipped':>7} {'Total':>6}"
    )
    for branch, counts in counts_by_branch.items():
        if counts["failed"]:
            status = "FAIL"
        elif counts["passed"]:
            status = "PASS"
        elif counts["skipped"]:
            status = "SKIP"
        else:
            status = "NO TESTS"
        terminalreporter.write_line(
            f"{branch:<{branch_width}} {status:<8} {counts['passed']:>6} "
            f"{counts['failed']:>6} {counts['skipped']:>7} {sum(counts.values()):>6}"
        )

    for branch in counts_by_branch:
        for outcome in ("failed", "skipped"):
            test_names = [
                nodeid
                for nodeid, result in config.eld_docs_results.items()
                if result == (branch, outcome)
            ]
            if test_names:
                terminalreporter.write_line("")
                terminalreporter.write_line(f"{branch} {outcome} tests:")
                for test_name in test_names:
                    terminalreporter.write_line(f"  {test_name}")


@pytest.fixture(scope="session", autouse=True)
def docs_dir(request):
    # Autouse parametrizes even tests that do not request docs_dir directly;
    # page-specific fixtures can depend on this root for the selected version.
    return request.param
