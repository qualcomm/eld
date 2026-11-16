# Documentation tests

The documentation tests run against built eld-userguide HTML documentation.
They open the HTML files directly using file URLs; no HTTP server or deployed
site is required.

Common pytest options and fixtures live in `test/docs/conftest.py`.

Run the following commands from the ELD source root to install the test
dependencies and Chromium in the Python environment used to run the tests:

```sh
python3 -m pip install -r test/docs/requirements.txt
python3 -m playwright install chromium
```

To run the tests:

```sh
cmake --build /path/to/build --target check-eld-docs
```

This target builds `eld-docs-multiversion`, then tests every version listed in
the generated site's `versions.json`. Each test is parametrized with the version
label (for example, `main` or `23.x`); the `stable` alias is not tested again.
The shared `docs_dir` fixture provides the current version's HTML directory.

During each run, the log identifies the documentation branch being tested
whenever the selected version changes.
At the end of each run, a branch-wise summary reports the status and passed,
failed, skipped, and total test counts. Setup and teardown errors count as
failures, with each test counted only once. Versions with no selected tests
appear as `NO TESTS`. Failed and skipped test names (full pytest node IDs) are
listed by branch after the counts. The same logging and summary are printed
when running pytest directly.

Each test module defines `TEST_BRANCH` as a string:

```python
TEST_BRANCH = "all"
```

Use `"all"` to run against every generated version (also the default if the label
is omitted), `"main"` to test only `site/main`, or `"23.x"` to test only
`site/23.x`. Values match output directory labels from `ELD_DOC_VERSIONS`, not
Git branch names such as `release/23.x`. Tests targeting a version not present
in the manifest are skipped.

To run all docs tests directly against a multiversion site:

```sh
python3 -E -m pytest test/docs --docs-site-dir /path/to/eld/docs/multiversion/site
```

To run a docs test directly against a single HTML directory:

```sh
python3 -E -m pytest test/docs/<test-dir> --docs-dir /path/to/<eld-docs-html-root>
```

`--docs-dir` runs each test once against the explicitly selected directory,
without filtering by `TEST_BRANCH`. It cannot be combined with `--docs-site-dir`.
