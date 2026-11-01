#!/usr/bin/env python3
"""Register this app's existing icons and Linux launcher for the current user."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

APP_ID = "de.appfabrik.holzwerkprojekt"
APP_NAME = "Woodworking Projects"
BINARY = "holzwerkprojekt"
PACKAGE_LAUNCHER = "WoodworkingProjects.sh"
MARKER = "X-AppFabrik-Managed=true"
SIZES = (16, 32, 48, 64, 128, 256, 512, 1024)


def desktop_value(value):
    return (
        value.replace("\\", "\\\\")
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )


def exec_argument(value):
    value = value.replace("%", "%%")
    value = "".join("\\" + char if char in '\\"`$' else char for char in value)
    return desktop_value('"' + value + '"')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument(
        "--development", action="store_true", help="Register this checkout's built Dev app"
    )
    mode.add_argument("--app-dir", type=Path, help="Register an unpacked Linux app directory")
    parser.add_argument(
        "--data-dir", type=Path, help="Optional existing collection for this launcher"
    )
    args = parser.parse_args()
    if not sys.platform.startswith("linux"):
        parser.error("Desktop registration is only available on Linux")
    if args.development:
        root = Path(__file__).resolve().parents[1]
        launcher = root / "scripts/launch.sh"
        binary = root / "build/dev" / BINARY
        icons = root / "assets/branding"
    else:
        root = (args.app_dir or Path(__file__).resolve().parent).expanduser().resolve()
        launcher = root / PACKAGE_LAUNCHER
        binary = root / "bin" / BINARY
        icons = root / "share/icons"
    for executable in (launcher, binary):
        if not executable.is_file() or not os.access(executable, os.X_OK):
            parser.error(f"Missing executable: {executable}. Build the app before registering it.")
    if "=" in str(launcher):
        parser.error("The desktop specification does not allow '=' in the executable path")
    for name in [*(f"icon-{size}.png" for size in SIZES), "icon.svg"]:
        if not (icons / name).is_file():
            parser.error(f"Missing icon: {icons / name}")
    data_home = Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local/share")
    if not data_home.is_absolute():
        data_home = Path.home() / ".local/share"
    applications = data_home / "applications"
    desktop = applications / f"{APP_ID}.desktop"
    if desktop.is_symlink() or (
        desktop.exists() and MARKER not in desktop.read_text(encoding="utf-8").splitlines()
    ):
        parser.error(f"Existing unmanaged application entry is preserved: {desktop}")
    # GLib checks the executable before expanding %% in desktop command lines.
    command = ["/usr/bin/env", str(launcher)]
    if args.data_dir is not None:
        command.append("--data-dir=" + str(args.data_dir.expanduser().resolve()))
    entry = "\n".join(
        [
            "[Desktop Entry]",
            "Type=Application",
            "Name=" + APP_NAME,
            "Exec=" + " ".join(exec_argument(value) for value in command),
            "TryExec=" + desktop_value(str(launcher)),
            "Icon=" + APP_ID,
            "Terminal=false",
            "Categories=Office;",
            "StartupNotify=false",
            "StartupWMClass=" + APP_ID,
            MARKER,
            "",
        ]
    )
    theme = data_home / "icons/hicolor"
    for size in SIZES:
        destination = theme / f"{size}x{size}/apps/{APP_ID}.png"
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(icons / f"icon-{size}.png", destination)
    vector = theme / f"scalable/apps/{APP_ID}.svg"
    vector.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(icons / "icon.svg", vector)
    applications.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", dir=applications, delete=False
        ) as handle:
            temporary = Path(handle.name)
            handle.write(entry)
        temporary.chmod(0o644)
        temporary.replace(desktop)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    for tool, arguments in (
        ("update-desktop-database", [str(applications)]),
        ("gtk-update-icon-cache", ["-f", "-t", str(theme)]),
    ):
        executable = shutil.which(tool)
        if executable:
            result = subprocess.run([executable, *arguments], capture_output=True, text=True)
            if result.returncode:
                print(
                    f"Optional cache update failed: {tool}: {result.stderr.strip()}",
                    file=sys.stderr,
                )
    print(f"Registered {APP_NAME}: {desktop}")


if __name__ == "__main__":
    main()
