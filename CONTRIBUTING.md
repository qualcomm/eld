## Contributing to ELD

Hi there!
We’re thrilled that you’d like to contribute to this project.
Your help is essential for keeping this project great and for making it better.

## Branching Strategy

In general, contributors should develop on branches based off of `main` and pull requests should be made against `main`.

## Submitting a pull request

1. Please read our [code of conduct](CODE-OF-CONDUCT.md) and [license](LICENSE.txt).
1. [Fork](https://github.com/qualcomm/eld/fork) and clone the repository.

    ```bash
    git clone https://github.com/<username>/eld.git
    ```

1. Create a new branch based on `main`:

    ```bash
    git checkout -b <my-branch-name> main
    ```

1. Create an upstream `remote` to make it easier to keep your branches up-to-date:

    ```bash
    git remote add upstream https://github.com/qualcomm/eld.git
    ```

1. Make your changes, add tests, and make sure the tests still pass.
1. Commit your changes using the [DCO](http://developercertificate.org/). You can attest to the DCO by commiting with the **-s** or **--signoff** options or manually adding the "Signed-off-by":

    ```bash
    git commit -s -m "Really useful commit message"`
    ```

1. After committing your changes on the topic branch, sync it with the upstream branch:

    ```bash
    git pull --rebase upstream main
    ```

1. Push to your fork.

    ```bash
    git push -u origin <my-branch-name>
    ```

    The `-u` is shorthand for `--set-upstream`. This will set up the tracking reference so subsequent runs of `git push` or `git pull` can omit the remote and branch.

1. [Submit a pull request](https://github.com/qualcomm/eld/pulls) from your branch to `main`.
1. Add the appropiate labels to include these changes in the release notes. Refer to `.github/release.yml` for the list of categories and their labels.
1. Pat yourself on the back and wait for your pull request to be reviewed.

## Requesting pull request checks

After opening a pull request, authorized project members can request the
following checks by commenting the command on the pull request:

| Comment | Check |
| --- | --- |
| `check-llvm-test-suite` | LLVM test suite |
| `check-linux-kernel` | Linux kernel boot with ELD |
| `check-zephyr` | Zephyr with ELD |
| `check-thread-sanitizer` | Thread sanitizer |
| `check-asan-ubsan` | Address and undefined behavior sanitizers |
| `check-memory-sanitizer` | Memory with origins sanitizer |
| `check-windows` | Windows CI |
| `check-macos` | macOS CI |
| `check-all-workflows` | All available checks |

Requests are accepted from users with GitHub `COLLABORATOR`, `MEMBER`, or
`OWNER` association. The command must be the complete comment, with no
additional text.

The router uses the exact pull request head SHA when running a check. If a
matching ELD toolset artifact already exists, it is reused; otherwise the CI
workflow builds the toolset first. Push a new commit to the pull request and
request the check again to test the new SHA.

Maintainers can also run the **PR comment router** through GitHub Actions using
the manual dispatch inputs. Provide the pull request number, its current head
SHA, and one of the commands above.

Here are a few things you can do that will increase the likelihood of your pull request to be accepted:

- Use GitHub issues to report bugs, propose features, suggest architecture adjustments, or make modifications to the core codebase.
- Provide documentation to support any changes that alter existing behavior.
- Follow [LLVM coding conventions](https://llvm.org/docs/CodingStandards.html) when contributing new code to the repository.
> **_NOTE:_** The current codebase incorporates a mix of coding styles, and we are in the process of transitioning to LLVM coding standards for consistency.
- Write unit tests.
- Keep your change as focused as possible.
  If you want to make multiple independent changes, please consider submitting them as separate pull requests.
- Write a [good commit message](http://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html).
- It's a good idea to arrange a discussion with other developers to ensure there is consensus on large features, architecture changes, and other core code changes. PR reviews will go much faster when there are no surprises.
