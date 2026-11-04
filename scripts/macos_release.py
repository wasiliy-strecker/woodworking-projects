#!/usr/bin/env python3
"""Build and verify an independent universal Qt Mac App Store delivery."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile

import macos_bundle

ROOT = Path(__file__).resolve().parents[1]
PACKAGING = ROOT / "packaging/macos"
ARCHITECTURES = {"arm64", "x86_64"}
MACH_MAGICS = (b"\xcf\xfa\xed\xfe", b"\xfe\xed\xfa\xcf", b"\xca\xfe\xba\xbe",
               b"\xbe\xba\xfe\xca", b"\xca\xfe\xba\xbf", b"\xbf\xba\xfe\xca")


def read_json(path):
    return json.loads(Path(path).read_text())


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + "\n")


def run(*command, capture=False):
    return subprocess.run([str(x) for x in command], cwd=ROOT, check=True,
                          stdout=subprocess.PIPE if capture else None,
                          stderr=subprocess.PIPE if capture else None)


def settings():
    value = read_json(PACKAGING / "release.json")
    version = re.search(r"project\(HolzwerkProjekt VERSION ([\d.]+)",
                        (ROOT / "CMakeLists.txt").read_text())[1]
    if value["version"] != version or set(value["architectures"]) != ARCHITECTURES:
        raise ValueError("Release version or architecture mismatch")
    if value["bundleId"] != "de.appfabrik.woodworkingprojects":
        raise ValueError("Keep the existing registered bundle identifier")
    if not re.fullmatch(r"[1-9][0-9]*", value["build"]):
        raise ValueError("Build number must be a positive integer")
    return value


def source_digest():
    files = [ROOT / x for x in ("CMakeLists.txt", "CMakePresets.json", "README.md",
                               "THIRD_PARTY_NOTICES.md", ".clang-format", ".clang-tidy")]
    for directory in ("src", "scripts", "packaging", "assets", "translations", "licenses", "docs"):
        files.extend(p for p in (ROOT / directory).rglob("*") if p.is_file()
                     and not p.name.endswith(".local.json") and "__pycache__" not in p.parts)
    digest = hashlib.sha256()
    for path in sorted(set(files)):
        digest.update(path.relative_to(ROOT).as_posix().encode())
        digest.update(bytes.fromhex(sha256(path)))
    return digest.hexdigest()


def inventory(folder):
    result = {}
    for path in sorted(Path(folder).rglob("*")):
        if path.is_symlink():
            result[str(path.relative_to(folder))] = {"symlink": os.readlink(path)}
        elif path.is_file():
            result[str(path.relative_to(folder))] = sha256(path)
    return result


def is_macho(path):
    if path.is_symlink() or not path.is_file():
        return False
    with path.open("rb") as stream:
        return stream.read(4) in MACH_MAGICS


def native_files(app):
    return sorted(p for p in app.rglob("*") if is_macho(p))


def inspect(app, signed=False):
    macos_bundle.inspect_bundle(app)
    release = settings()
    info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
    expected = {"CFBundleIdentifier": release["bundleId"],
                "CFBundleShortVersionString": release["version"],
                "CFBundleVersion": release["build"],
                "CFBundleDisplayName": release["displayName"],
                "LSMinimumSystemVersion": release["minimumMacOS"]}
    for key, value in expected.items():
        if info.get(key) != value:
            raise ValueError("Unexpected bundle field: " + key)
    for required in ("MacOS/holzwerkprojekt", "PlugIns/sqldrivers/libqsqlite.dylib",
                     "PlugIns/imageformats/libqjpeg.dylib",
                     "PlugIns/platforms/libqcocoa.dylib", "Resources/qt.conf"):
        if not (app / "Contents" / required).is_file():
            raise ValueError("Missing runtime file: " + required)
    files = native_files(app)
    for path in files:
        arches = set(run("lipo", "-archs", path, capture=True).stdout.decode().split())
        if arches != ARCHITECTURES:
            raise ValueError("Native file is not universal: " + str(path))
        linked = run("otool", "-L", path, capture=True).stdout.decode().splitlines()
        for line in linked:
            if not line.startswith("\t"):
                continue
            dependency = line.strip().split(" (", 1)[0]
            if not dependency.startswith(("@rpath/", "@loader_path/", "@executable_path/",
                                          "/System/Library/", "/usr/lib/")):
                raise ValueError("External SDK dependency: " + dependency)
    for entry in read_json(PACKAGING / "qt-sources.json")["files"]:
        path = app / "Contents/Resources/QtSources" / entry["file"]
        if not path.is_file() or sha256(path) != entry["sha256"]:
            raise ValueError("Missing or changed corresponding Qt source: " + entry["file"])
    if signed:
        run("codesign", "--verify", "--deep", "--strict", "--verbose=2", app)
        ent = plistlib.loads(run("codesign", "-d", "--entitlements", ":-", app, capture=True).stdout)
        if not ent.get("com.apple.security.app-sandbox") or not ent.get("com.apple.security.files.user-selected.read-write"):
            raise ValueError("Missing sandbox or user-selected file entitlement")
        if any(ent.get(key) for key in ("com.apple.security.network.client", "com.apple.security.network.server",
                                       "com.apple.security.cs.disable-library-validation", "com.apple.security.inherit")):
            raise ValueError("Unexpected expanded permissions")
    print(f"Verified identity, source archives and {len(files)} universal native files.", flush=True)


def build(qt_root, build_dir, output):
    release = settings()
    if build_dir.exists() or output.exists():
        raise ValueError("Choose fresh build and delivery directories")
    version = run(qt_root / "bin/qmake", "-query", "QT_VERSION", capture=True).stdout.decode().strip()
    if version != "6.8.3":
        raise ValueError("Qt 6.8.3 SDK required")
    sdk_before = {name: sha256(qt_root / name) for name in (
        "lib/QtCore.framework/Versions/A/QtCore", "lib/QtGui.framework/Versions/A/QtGui",
        "lib/QtWidgets.framework/Versions/A/QtWidgets", "lib/QtSql.framework/Versions/A/QtSql",
        "plugins/sqldrivers/libqsqlite.dylib", "plugins/platforms/libqcocoa.dylib")}
    digest = source_digest()
    run("cmake", "-S", ROOT, "-B", build_dir, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
        "-DBUILD_TESTING=OFF", f"-DCMAKE_PREFIX_PATH={qt_root}",
        "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0",
        f"-DWOODWORKING_PROJECTS_BUILD_NUMBER={release['build']}")
    run("cmake", "--build", build_dir, "--parallel", "4")
    output.mkdir(parents=True)
    app = output / (release["displayName"] + ".app")
    shutil.copytree(build_dir / "holzwerkprojekt.app", app, symlinks=True)
    run(qt_root / "bin/macdeployqt", app, "-always-overwrite", "-appstore-compliant")
    # Qt deploys every SQL driver in its SDK, including Mimer, whose client is
    # an external /usr/local dependency. This app uses SQLite exclusively.
    for driver in (app / "Contents/PlugIns/sqldrivers").iterdir():
        if driver.name != "libqsqlite.dylib":
            if driver.is_dir():
                raise ValueError("Unexpected SQL driver directory")
            driver.unlink()
    resources = app / "Contents/Resources"
    resources.mkdir(exist_ok=True)
    (resources / "qt.conf").write_text("[Paths]\nPlugins=PlugIns\nTranslations=Resources/translations\n")
    shutil.copytree(ROOT / "licenses", resources / "licenses")
    shutil.copytree(qt_root / "sbom", resources / "licenses/qt-sdk-sbom")
    shutil.copy2(ROOT / "THIRD_PARTY_NOTICES.md", resources)
    shutil.copy2(ROOT / "docs/qt-library-replacement.md", resources)
    shutil.copy2(ROOT / "scripts/replace_macos_qt.py", resources)
    source_dir = resources / "QtSources"
    source_dir.mkdir()
    for entry in read_json(PACKAGING / "qt-sources.json")["files"]:
        source = ROOT / "build/qt-source-cache" / entry["file"]
        if sha256(source) != entry["sha256"]:
            raise ValueError("Qt source archive checksum mismatch")
        shutil.copy2(source, source_dir)
    shutil.copy2(PACKAGING / "qt-sources.json", source_dir)
    iconset = output / "WoodworkingProjects.iconset"
    iconset.mkdir()
    for size in (16, 32, 128, 256, 512):
        for scale in (1, 2):
            name = f"icon_{size}x{size}" + ("@2x" if scale == 2 else "") + ".png"
            run("sips", "-z", size * scale, size * scale, ROOT / "assets/branding/icon-1024.png",
                "--out", iconset / name, capture=True)
    run("iconutil", "-c", "icns", iconset, "-o", resources / "WoodworkingProjects.icns")
    info_path = app / "Contents/Info.plist"
    info = plistlib.loads(info_path.read_bytes()); info["CFBundleIconFile"] = "WoodworkingProjects.icns"
    info_path.write_bytes(plistlib.dumps(info))
    macos_bundle.prepare_bundle(app)
    inspect(app)
    if digest != source_digest() or any(sha256(qt_root / p) != value for p, value in sdk_before.items()):
        raise ValueError("Source or SDK changed during the build")
    write_json(output / "build-report.json", {"sourceDigest": digest, "release": release,
               "qtVersion": version, "sdkInputs": sdk_before, "files": inventory(app)})


def sign(app, config, mode):
    inspect(app)
    build_report = read_json(app.parent / "build-report.json")
    if build_report["sourceDigest"] != source_digest() or build_report["files"] != inventory(app):
        raise ValueError("Sign only a fresh copy of the verified build and its report")
    value = read_json(config)
    release = settings()
    if value["appStoreConnectBundleId"] != release["bundleId"] or not re.fullmatch(r"[A-Z0-9]{10}", value["teamId"]):
        raise ValueError("Signing identity belongs to a different app/team")
    profile = plistlib.loads(run("security", "cms", "-D", "-i", value["provisioningProfile"], capture=True).stdout)
    entitlements = profile["Entitlements"]
    identifier = entitlements.get("com.apple.application-identifier", entitlements.get("application-identifier"))
    if (identifier != value["teamId"] + "." + release["bundleId"] or value["teamId"] not in profile["TeamIdentifier"]
            or profile["ExpirationDate"].replace(tzinfo=timezone.utc) <= datetime.now(timezone.utc)):
        raise ValueError("Wrong or expired profile")
    identity = value["applicationIdentity"]
    prefix = ("Apple Development:", "Mac Developer:") if mode == "development" else ("Apple Distribution:", "3rd Party Mac Developer Application:")
    if not identity.startswith(prefix) or (mode == "store" and profile.get("ProvisionedDevices")):
        raise ValueError("Signing identity/profile does not match the requested mode")
    embedded = app / "Contents/embedded.provisionprofile"
    shutil.copyfile(value["provisioningProfile"], embedded); embedded.chmod(0o644)
    ent = plistlib.loads((PACKAGING / "app-store.entitlements").read_bytes())
    for key in ("com.apple.application-identifier", "application-identifier", "com.apple.developer.team-identifier"):
        if key in entitlements:
            ent[key] = entitlements[key]
    with tempfile.TemporaryDirectory(prefix="woodworking-projects-sign-") as temp:
        path = Path(temp) / "app.entitlements"; path.write_bytes(plistlib.dumps(ent))
        main = app / "Contents/MacOS/holzwerkprojekt"
        for native in native_files(app):
            if native == main or any(p.suffix == ".framework" for p in native.parents):
                continue
            run("codesign", "--force", "--options", "runtime", "--timestamp", "--sign", identity, native)
        for framework in sorted(app.rglob("*.framework"), key=lambda p: len(p.parts), reverse=True):
            if not framework.is_symlink():
                run("codesign", "--force", "--options", "runtime", "--timestamp", "--sign", identity, framework)
        run("codesign", "--force", "--options", "runtime", "--timestamp", "--sign", identity,
            "--entitlements", path, app)
    inspect(app, signed=True)
    details = run("codesign", "-dvv", app, capture=True).stderr.decode()
    if "TeamIdentifier=" + value["teamId"] not in details:
        raise ValueError("Signature team mismatch")
    write_json(app.parent / "signing-report.json", {"mode": mode, "sourceDigest": source_digest(),
               "files": inventory(app), "nativeAcceptance": "pending"})


def package(app, config, acceptance, output):
    inspect(app, signed=True)
    signed = read_json(app.parent / "signing-report.json")
    accepted = read_json(acceptance)
    required = ("arm64Execution", "x86_64Execution", "sandboxStorage", "crudAndSearch", "sorting",
                "jsonTransfer", "restart", "dialogCancellation", "nativeMenus", "photoAndColor", "projectStatus", "checklist", "offlineLaunch", "qtLibraryReplacement")
    if any(accepted.get(key) is not True for key in required) or accepted.get("x86_64Method") not in ("native", "rosetta"):
        raise ValueError("Record complete native functional acceptance before packaging")
    if (signed["mode"] != "store" or signed["files"] != inventory(app)
            or signed["sourceDigest"] != source_digest() or accepted["sourceDigest"] != source_digest()):
        raise ValueError("Signature, sources and acceptance must match")
    value = read_json(config)
    if value["appStoreConnectBundleId"] != settings()["bundleId"] or not value["installerIdentity"].startswith(("3rd Party Mac Developer Installer:", "Mac Installer Distribution:")):
        raise ValueError("Wrong installer identity")
    if output.exists():
        raise ValueError("Never overwrite an existing installer")
    run("productbuild", "--component", app, "/Applications", "--sign", value["installerIdentity"], output)
    run("pkgutil", "--check-signature", output)
    write_json(output.with_suffix(".json"), {"sourceDigest": source_digest(), "sha256": sha256(output),
               "bytes": output.stat().st_size})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    cmd = sub.add_parser("build")
    for name in ("qt-root", "build-dir", "output"):
        cmd.add_argument("--" + name, type=Path, required=True)
    cmd = sub.add_parser("inspect"); cmd.add_argument("--app", type=Path, required=True)
    cmd.add_argument("--signed", action="store_true")
    cmd = sub.add_parser("sign")
    cmd.add_argument("--mode", choices=("development", "store"), required=True)
    for name in ("app", "config"):
        cmd.add_argument("--" + name, type=Path, required=True)
    cmd = sub.add_parser("package")
    for name in ("app", "config", "acceptance", "output"):
        cmd.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("Native release operations require macOS")
    for name in ("qt_root", "build_dir", "output", "app", "config", "acceptance"):
        if hasattr(args, name):
            setattr(args, name, getattr(args, name).expanduser().resolve())
    if args.command == "build":
        build(args.qt_root, args.build_dir, args.output)
    elif args.command == "inspect":
        inspect(args.app, args.signed)
    elif args.command == "sign":
        sign(args.app, args.config, args.mode)
    else:
        package(args.app, args.config, args.acceptance, args.output)


if __name__ == "__main__":
    main()
