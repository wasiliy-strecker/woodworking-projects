#!/usr/bin/env python3
"""Check a staged macOS bundle; optionally repair only its distribution modes."""

import argparse
import os
from pathlib import Path
import plistlib
import re
import stat
import sys


def _walk_error(error):
    raise error


def _bundle_paths(app):
    app = Path(app).absolute()
    if app.suffix != ".app" or app.is_symlink() or not app.is_dir():
        raise ValueError("Expected a real staged .app directory: " + str(app))
    contents = app / "Contents"
    info = contents / "Info.plist"
    if contents.is_symlink() or info.is_symlink() or not info.is_file():
        raise ValueError("Missing regular Contents/Info.plist: " + str(app))
    paths = [app]
    for parent, directories, files in os.walk(app, followlinks=False, onerror=_walk_error):
        paths.extend(Path(parent) / name for name in sorted(directories + files))
    root = app.resolve()
    for path in paths:
        if path.is_symlink():
            # Qt frameworks use internal links. Never follow a link into an SDK
            # or private signing directory when changing distribution permissions.
            try:
                path.resolve(strict=True).relative_to(root)
            except (OSError, RuntimeError, ValueError) as error:
                raise ValueError("Broken or external bundle symlink: " + str(path)) from error
        elif not (path.is_dir() or path.is_file()):
            raise ValueError("Unsupported bundle resource: " + str(path))
    return app, paths


def _validate_identifiers(app, paths):
    for path in paths:
        if path.is_symlink() or path.name != "Info.plist" or not path.is_file():
            continue
        info = plistlib.loads(path.read_bytes())
        identifier = info.get("CFBundleIdentifier")
        if identifier is None and path != app / "Contents/Info.plist":
            continue
        if not isinstance(identifier, str) or not re.fullmatch(r"[A-Za-z0-9.-]+", identifier):
            raise ValueError("Invalid bundle identifier in " + str(path))


def _required_mode(path):
    mode = stat.S_IMODE(path.stat().st_mode)
    # Preserve executable files as executable for every user; data stays data.
    return 0o444 | (0o111 if path.is_dir() or mode & 0o111 else 0)


def inspect_bundle(app):
    app, paths = _bundle_paths(app)
    _validate_identifiers(app, paths)
    for path in paths:
        if path.is_symlink():
            continue
        required = _required_mode(path)
        if stat.S_IMODE(path.stat().st_mode) & required != required:
            raise ValueError("Bundle resource is not readable/executable by all users: " + str(path))


def prepare_bundle(app):
    """Normalize only a staging copy. Never rewrite IDs, plist bytes or signatures."""
    app, paths = _bundle_paths(app)
    _validate_identifiers(app, paths)
    for path in paths:
        if not path.is_symlink():
            mode = stat.S_IMODE(path.stat().st_mode)
            path.chmod(mode | _required_mode(path))
    inspect_bundle(app)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("prepare", "inspect"))
    parser.add_argument("--app", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "prepare":
        prepare_bundle(args.app)
    else:
        inspect_bundle(args.app)
    print("Bundle identifiers and distribution permissions checked.")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, plistlib.InvalidFileException) as failure:
        sys.exit(str(failure))
