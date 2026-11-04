#!/usr/bin/env python3
"""Make a separate local copy using compatible replacement Qt frameworks.

No original app, collection, Gatekeeper setting or private signing key is changed.
The output is locally ad-hoc signed and has its own sandbox container.
"""

import argparse
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile


def run(*args):
    subprocess.run([str(x) for x in args], check=True)


def replace(app, replacement, output):
    if app.suffix != ".app" or not (app / "Contents/Info.plist").is_file():
        raise ValueError("Choose a complete source application")
    if output.exists() or output.suffix != ".app":
        raise ValueError("Choose a new .app path; existing files are never replaced")
    if not replacement.is_dir():
        raise ValueError("Choose a directory containing replacement Qt*.framework bundles")
    frameworks = sorted(replacement.glob("Qt*.framework"))
    if not frameworks:
        raise ValueError("No replacement frameworks found")
    for framework in frameworks:
        if not (app / "Contents/Frameworks" / framework.name).is_dir():
            raise ValueError("Replacement is not used by this application: " + framework.name)
    shutil.copytree(app, output, symlinks=True)
    for framework in frameworks:
        target = output / "Contents/Frameworks" / framework.name
        shutil.rmtree(target)
        shutil.copytree(framework, target, symlinks=True)
    (output / "Contents/embedded.provisionprofile").unlink(missing_ok=True)
    info_path = output / "Contents/Info.plist"
    info = plistlib.loads(info_path.read_bytes())
    info["CFBundleIdentifier"] += ".local-relinked"
    info["CFBundleDisplayName"] += " (Local Qt)"
    info["CFBundleName"] = info["CFBundleDisplayName"]
    info_path.write_bytes(plistlib.dumps(info))
    # Keep sandbox confinement and selected-file access. Apple application/team
    # identifiers belong to the original publisher and are intentionally absent.
    entitlements = {"com.apple.security.app-sandbox": True,
                    "com.apple.security.files.user-selected.read-write": True}
    with tempfile.TemporaryDirectory(prefix="qt-local-sign-") as temporary:
        ent = Path(temporary) / "sandbox.plist"
        ent.write_bytes(plistlib.dumps(entitlements))
        for path in sorted(output.rglob("*")):
            if path.is_file() and not path.is_symlink() and path.suffix == ".dylib":
                run("codesign", "--force", "--sign", "-", path)
        for framework in sorted(output.rglob("*.framework"), key=lambda p: len(p.parts), reverse=True):
            if not framework.is_symlink():
                run("codesign", "--force", "--sign", "-", framework)
        run("codesign", "--force", "--sign", "-", "--entitlements", ent, output)
    run("codesign", "--verify", "--deep", "--strict", output)
    print("Created a separate locally signed app:", output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("app", "replacement", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    replace(args.app.expanduser().resolve(), args.replacement.expanduser().resolve(),
            args.output.expanduser().resolve())
