"""Bundle the Linux test application with its own Qt and SQLite plugin."""

import argparse
import hashlib
import os
import re
import shutil
import subprocess
import tarfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qt", required=True, type=Path)
    parser.add_argument("--cursor-library", type=Path)
    parser.add_argument("--cursor-license", type=Path)
    parser.add_argument(
        "--stage-only", action="store_true", help="Prüfverzeichnis ohne Lieferarchiv"
    )
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    qt = args.qt.resolve()
    version = re.search(
        r"project\(HolzwerkProjekt VERSION ([\d.]+)",
        (root / "CMakeLists.txt").read_text(),
    )[1]
    name = f"holzwerkprojekt-{version}-linux-x64"
    output = root / ("build/staging" if args.stage_only else "build/releases")
    stage = output / name
    archive = output / f"{name}.tar.gz"
    if stage.exists() or archive.exists():
        raise SystemExit(
            "Lieferung existiert bereits. Für einen neuen Stand zuerst die Version erhöhen oder die ungeteilte Testlieferung separat archivieren."
        )
    if not args.cursor_library:
        for path in (Path("/usr/lib/x86_64-linux-gnu/libxcb-cursor.so.0"),):
            if path.exists():
                args.cursor_library = path
                args.cursor_license = Path("/usr/share/doc/libxcb-cursor0/copyright")
                break
    if not args.cursor_library or not args.cursor_license:
        raise SystemExit(
            "libxcb-cursor und der zugehörige Lizenztext müssen angegeben werden."
        )
    for folder in (
        "bin",
        "lib",
        "plugins/platforms",
        "plugins/sqldrivers",
        "plugins/imageformats",
        "plugins/platformthemes",
        "plugins/platforminputcontexts",
        "translations",
    ):
        (stage / folder).mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / "build/release/holzwerkprojekt", stage / "bin")
    for module in ("Core", "Gui", "Widgets", "Sql", "DBus", "XcbQpa", "OpenGL"):
        source = qt / f"lib/libQt6{module}.so.6"
        shutil.copy2(source.resolve(), stage / "lib" / source.name)
    for source in qt.glob("lib/libicu*.so.73"):
        shutil.copy2(source.resolve(), stage / "lib" / source.name)
    for relative in (
        "platforms/libqxcb.so",
        "platforms/libqoffscreen.so",
        "sqldrivers/libqsqlite.so",
        "imageformats/libqjpeg.so",
        "platformthemes/libqgtk3.so",
        "platforminputcontexts/libcomposeplatforminputcontextplugin.so",
        "platforminputcontexts/libibusplatforminputcontextplugin.so",
    ):
        shutil.copy2(qt / "plugins" / relative, stage / "plugins" / relative)
    shutil.copy2(args.cursor_library.resolve(), stage / "lib/libxcb-cursor.so.0")
    shutil.copy2(qt / "translations/qtbase_de.qm", stage / "translations")
    shutil.copytree(root / "licenses", stage / "licenses")
    shutil.copytree(root / "docs", stage / "docs")
    shutil.copy2(args.cursor_license, stage / "licenses/xcb-cursor.txt")
    shutil.copytree(qt / "sbom", stage / "licenses/qt-sdk-sbom")
    for filename in ("README.md", "THIRD_PARTY_NOTICES.md"):
        shutil.copy2(root / filename, stage / filename)
    (stage / "bin/qt.conf").write_text(
        "[Paths]\nPrefix=..\nPlugins=plugins\nLibraries=lib\nTranslations=translations\n"
    )
    launcher = stage / "HolzwerkProjekt.sh"
    launcher.write_text(
        '#!/bin/sh\nset -eu\nholzwerk_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexport LD_LIBRARY_PATH="$holzwerk_dir/lib"\nexport QT_PLUGIN_PATH="$holzwerk_dir/plugins"\nexport QT_QPA_PLATFORM_PLUGIN_PATH="$holzwerk_dir/plugins/platforms"\nexport QT_QPA_PLATFORMTHEME="gtk3"\nexec "$holzwerk_dir/bin/holzwerkprojekt" "$@"\n'
    )
    launcher.chmod(0o755)
    environment = dict(os.environ, LD_LIBRARY_PATH=str(stage / "lib"))
    inspected = [
        stage / "bin/holzwerkprojekt",
        *stage.glob("lib/*.so*"),
        *stage.glob("plugins/*/*.so"),
    ]
    for binary in inspected:
        result = subprocess.run(
            ["ldd", str(binary)],
            env=environment,
            capture_output=True,
            text=True,
            check=True,
        )
        if "not found" in result.stdout or str(qt) in result.stdout:
            raise SystemExit(
                f"Unvollständige Laufzeit für {binary.name}:\n{result.stdout}"
            )
    manifest = []
    for path in sorted(stage.rglob("*")):
        if path.is_file():
            manifest.append(
                f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(stage)}"
            )
    (stage / "SHA256SUMS").write_text("\n".join(manifest) + "\n")
    if args.stage_only:
        print(stage)
        return
    with tarfile.open(archive, "w:gz") as output_file:
        output_file.add(stage, arcname=name)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix(archive.suffix + ".sha256").write_text(
        f"{digest}  {archive.name}\n"
    )
    print(archive)


if __name__ == "__main__":
    main()
