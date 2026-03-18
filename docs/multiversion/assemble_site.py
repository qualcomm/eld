#!/usr/bin/env python3
"""
Assemble multi-version ELD documentation site.

Creates the landing page (index.html), version manifest (versions.json),
and stable symlink for the multi-version documentation site.

Usage:
    assemble_site.py --site-dir <path> --versions-spec <specs> --stable <version>

Example:
    assemble_site.py --site-dir ./site \
        --versions-spec "main:main:Main (dev);release/22.x:22.x:Release 22.x" \
        --stable "22.x"

Version spec format: "branch:label:display_name" (display_name is optional, defaults to label)
Multiple specs are separated by semicolons.
"""

import argparse
import json
import os
import sys
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).parent
TEMPLATES_DIR = SCRIPT_DIR / "templates"


def load_template(name: str) -> str:
    """Load a template file from the templates directory."""
    template_path = TEMPLATES_DIR / name
    return template_path.read_text()


def parse_version_specs(specs_str: str, stable_version: str) -> list[dict[str, Any]]:
    """
    Parse version specs from CMake format into version dictionaries.

    Args:
        specs_str: Semicolon-separated "branch:label:display_name" specs
        stable_version: Which label should be marked as stable

    Returns:
        List of version dicts with path, display_name, stable, and dev flags
    """
    versions = []
    for spec in specs_str.split(";"):
        spec = spec.strip()
        if not spec:
            continue

        parts = spec.split(":")
        if len(parts) < 2:
            print(f"WARNING: Invalid version spec (need at least branch:label): {spec}")
            continue

        branch = parts[0]
        label = parts[1]
        display_name = parts[2] if len(parts) >= 3 else label

        version: dict[str, Any] = {
            "path": label,
            "display_name": display_name,
        }

        if label == stable_version:
            version["stable"] = True
        if label == "main":
            version["dev"] = True

        versions.append(version)

    return versions


def find_stable_version(versions: list[dict[str, Any]]) -> str | None:
    """Find the version marked as stable in the versions list."""
    for v in versions:
        if v.get("stable"):
            return v["path"]
    return None


def generate_index_html(redirect_target: str, output_path: Path) -> None:
    """Generate index.html that instantly redirects to the main docs."""
    if output_path.is_symlink():
        output_path.unlink()
    elif output_path.exists():
        output_path.unlink()

    html = f'''<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<script>window.location.replace("./{redirect_target}/");</script>
<noscript><meta http-equiv="refresh" content="0; url=./{redirect_target}/"></noscript>
</head>
</html>
'''
    output_path.write_text(html)
    print(f"Generated: {output_path} (redirects to {redirect_target}/)")


def generate_versions_html(versions: list[dict[str, Any]], output_path: Path) -> None:
    """Generate the version selector page with links to all versions."""
    versions_template = load_template("versions.html")
    version_link_template = load_template("version_link.html")

    version_links = []
    for v in versions:
        badge = ""
        if v.get("stable"):
            badge = '\n                        <span class="badge badge-stable">stable</span>'
        elif v.get("dev"):
            badge = '\n                        <span class="badge badge-dev">dev</span>'

        version_links.append(version_link_template.format(
            path=v["path"],
            display_name=v["display_name"],
            badge=badge,
        ))

    html = versions_template.format(version_links="".join(version_links))
    output_path.write_text(html)
    print(f"Generated: {output_path}")


def generate_versions_json(versions: list[dict[str, Any]], stable: str | None, output_path: Path) -> None:
    """Generate the versions manifest JSON file."""
    manifest = {
        "versions": versions,
        "stable": stable,
        "default": "stable" if stable else versions[0]["path"] if versions else None,
    }
    output_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Generated: {output_path}")


def create_stable_symlink(site_dir: Path, stable_target: str) -> None:
    """Create the 'stable' symlink pointing to the stable version directory."""
    stable_link = site_dir / "stable"

    if stable_link.is_symlink():
        stable_link.unlink()
    elif stable_link.exists():
        raise RuntimeError(f"'stable' exists but is not a symlink: {stable_link}")

    target_dir = site_dir / stable_target
    if not target_dir.exists():
        print(f"WARNING: Target directory does not exist yet: {target_dir}")

    os.symlink(stable_target, stable_link)
    print(f"Created symlink: stable -> {stable_target}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Assemble multi-version ELD documentation site",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--site-dir", required=True, type=Path,
        help="Site output directory containing version subdirectories"
    )
    parser.add_argument(
        "--versions-spec", required=True,
        help="Semicolon-separated 'branch:label:display_name' specs"
    )
    parser.add_argument(
        "--stable", required=True,
        help="Which label should be marked as stable"
    )

    args = parser.parse_args()

    try:
        site_dir = args.site_dir.resolve()
        versions = parse_version_specs(args.versions_spec, args.stable)

        if not versions:
            raise ValueError("No valid version specs provided")

        stable = find_stable_version(versions)

        site_dir.mkdir(parents=True, exist_ok=True)

        redirect_target = "main"
        generate_index_html(redirect_target, site_dir / "index.html")
        generate_versions_html(versions, site_dir / "versions.html")
        generate_versions_json(versions, stable, site_dir / "versions.json")

        if stable:
            create_stable_symlink(site_dir, stable)
        else:
            print("WARNING: No version marked as stable, skipping stable symlink")

        nojekyll = site_dir / ".nojekyll"
        nojekyll.touch()
        print(f"Created: {nojekyll}")

        print(f"\nSite assembled successfully: {site_dir}")
        return 0

    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
