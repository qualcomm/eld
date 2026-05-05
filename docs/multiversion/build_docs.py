#!/usr/bin/env python3
"""
Build ELD documentation for a specific branch using git worktrees.

This script creates git worktrees for both llvm-project and eld repositories,
configures a CMake build, and builds the eld-docs target. The resulting HTML
documentation is copied to the specified output directory.

Usage:
    build_docs.py --branch <branch> \
                  --eld-repo <path> --llvm-repo <path> \
                  --work-dir <path> --output-dir <path>

Example:
    build_docs.py --branch main \
                  --eld-repo /path/to/eld --llvm-repo /path/to/llvm-project \
                  --work-dir /tmp/docs-build --output-dir /tmp/site
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def run(cmd: list, cwd: Path | None = None, check: bool = True) -> subprocess.CompletedProcess:
    """Run a command and print it for visibility."""
    print(f"+ {' '.join(str(c) for c in cmd)}", flush=True)
    return subprocess.run(cmd, cwd=cwd, check=check)


def create_worktree(repo_path: Path, worktree_path: Path, branch: str) -> None:
    """Create a git worktree for the specified branch (detached)."""
    run(["git", "worktree", "prune"], cwd=repo_path, check=False)

    if worktree_path.exists():
        # Check if it's already a worktree for this repo
        result = subprocess.run(
            ["git", "worktree", "list", "--porcelain"],
            cwd=repo_path,
            capture_output=True,
            text=True,
            check=False,
        )
        if str(worktree_path) in result.stdout:
            print(f"Worktree already exists: {worktree_path}")
            run(["git", "fetch", "origin", branch], cwd=worktree_path, check=False)
            run(["git", "checkout", "--detach", f"origin/{branch}"], cwd=worktree_path)
            return
        else:
            print(f"Removing stale directory: {worktree_path}")
            shutil.rmtree(worktree_path)

    worktree_path.parent.mkdir(parents=True, exist_ok=True)

    run(["git", "fetch", "origin", branch], cwd=repo_path, check=False)
    run(["git", "worktree", "add", "--detach", str(worktree_path), f"origin/{branch}"], cwd=repo_path)


def build_docs(
    eld_branch: str,
    eld_repo: Path,
    llvm_repo: Path,
    work_dir: Path,
    output_dir: Path,
) -> Path:
    """
    Build documentation for a specific ELD branch.

    Args:
        eld_branch: Git branch of ELD to build docs for
        eld_repo: Path to the ELD git repository
        llvm_repo: Path to the llvm-project git repository
        work_dir: Working directory for worktrees and builds
        output_dir: Output directory for the final HTML docs

    Returns:
        Path to the output HTML directory
    """
    # Paths - use fixed names within work_dir
    llvm_worktree = work_dir / "llvm-project"
    eld_worktree = llvm_worktree / "llvm" / "tools" / "eld"
    build_dir = work_dir / "obj"

    print(f"\n{'=' * 70}")
    print(f"Building docs for ELD branch '{eld_branch}'")
    print(f"  Work dir: {work_dir}")
    print(f"  Output dir: {output_dir}")
    print(f"{'=' * 70}\n")

    # Step 1: Create LLVM worktree (detached, always use main)
    print("\n[Step 1/5] Creating LLVM worktree...")
    create_worktree(llvm_repo, llvm_worktree, "main")

    # Step 2: Create ELD worktree inside LLVM
    print("\n[Step 2/5] Creating ELD worktree...")
    eld_tools_dir = llvm_worktree / "llvm" / "tools"
    eld_tools_dir.mkdir(parents=True, exist_ok=True)
    create_worktree(eld_repo, eld_worktree, eld_branch)

    # Step 3: Configure CMake
    print("\n[Step 3/5] Configuring CMake...")
    build_dir.mkdir(parents=True, exist_ok=True)

    cmake_args = [
        "cmake",
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DLLVM_ENABLE_SPHINX=ON",
        "-DLLVM_TARGETS_TO_BUILD=ARM;AArch64;RISCV;Hexagon",
        "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++",
        str(llvm_worktree / "llvm"),
    ]

    run(cmake_args, cwd=build_dir)

    # Step 4: Build eld-docs
    print("\n[Step 4/5] Building eld-docs...")
    run(["ninja", "eld-docs"], cwd=build_dir)

    # Step 5: Copy output
    print("\n[Step 5/5] Copying output...")
    html_src = build_dir / "tools" / "eld" / "docs" / "userguide" / "html"

    if not html_src.exists():
        raise RuntimeError(f"HTML output not found: {html_src}")

    output_dir.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(html_src, output_dir, dirs_exist_ok=True)

    print(f"\n{'=' * 70}")
    print(f"SUCCESS: Docs built at: {output_dir}")
    print(f"{'=' * 70}\n")

    return output_dir


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build ELD documentation for a specific branch using git worktrees",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--branch", required=True,
        help="ELD branch to build (e.g., 'main', 'release/22.x')"
    )
    parser.add_argument(
        "--eld-repo", required=True, type=Path,
        help="Path to ELD git repository"
    )
    parser.add_argument(
        "--llvm-repo", required=True, type=Path,
        help="Path to llvm-project git repository"
    )
    parser.add_argument(
        "--work-dir", required=True, type=Path,
        help="Working directory for worktrees and builds"
    )
    parser.add_argument(
        "--output-dir", required=True, type=Path,
        help="Output directory for HTML docs"
    )

    args = parser.parse_args()

    try:
        build_docs(
            eld_branch=args.branch,
            eld_repo=args.eld_repo.resolve(),
            llvm_repo=args.llvm_repo.resolve(),
            work_dir=args.work_dir.resolve(),
            output_dir=args.output_dir.resolve(),
        )
        return 0
    except subprocess.CalledProcessError as e:
        print(f"\nERROR: Command failed with exit code {e.returncode}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"\nERROR: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
