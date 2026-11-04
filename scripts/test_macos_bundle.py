#!/usr/bin/env python3
"""Packaging regressions using temporary fixtures only; no app build or signing."""

from pathlib import Path
import plistlib
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest

import macos_bundle as bundle


class MacBundleTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.app = self.root / "Example App.app"
        self.contents = self.app / "Contents"
        self.contents.mkdir(parents=True)
        self.info = self.contents / "Info.plist"
        self.info.write_bytes(plistlib.dumps({"CFBundleIdentifier": "de.example.app"}))

    def test_private_profile_copy_is_readable_without_changing_source(self):
        source = self.root / "private.provisionprofile"
        source.write_bytes(b"synthetic signed profile")
        source.chmod(0o600)
        embedded = self.contents / "embedded.provisionprofile"
        shutil.copy2(source, embedded)
        with self.assertRaisesRegex(ValueError, "not readable"):
            bundle.inspect_bundle(self.app)
        bundle.prepare_bundle(self.app)
        self.assertEqual(source.read_bytes(), embedded.read_bytes())
        self.assertEqual(0o600, stat.S_IMODE(source.stat().st_mode))
        self.assertEqual(0o644, stat.S_IMODE(embedded.stat().st_mode))

    def test_directory_and_launcher_modes_preserve_executable_and_data_roles(self):
        executable = self.contents / "MacOS/Launcher"
        executable.parent.mkdir()
        executable.write_bytes(b"synthetic executable")
        executable.chmod(0o700)
        executable.parent.chmod(0o700)
        self.info.chmod(0o600)
        bundle.prepare_bundle(self.app)
        self.assertEqual(0o755, stat.S_IMODE(executable.stat().st_mode))
        self.assertEqual(0o755, stat.S_IMODE(executable.parent.stat().st_mode))
        self.assertEqual(0o644, stat.S_IMODE(self.info.stat().st_mode))
        self.assertEqual(b"synthetic executable", executable.read_bytes())

    def test_inspection_checks_group_read_and_directory_traversal(self):
        for path, mode in ((self.info, 0o604), (self.contents, 0o744)):
            with self.subTest(mode=oct(mode)):
                path.chmod(mode)
                with self.assertRaisesRegex(ValueError, "not readable"):
                    bundle.inspect_bundle(self.app)
                bundle.prepare_bundle(self.app)

    def test_rejects_legacy_runtime_id_without_rewriting_it(self):
        runtime = self.contents / "PlugIns/Runtime-x86_64.bundle/Contents/Info.plist"
        runtime.parent.mkdir(parents=True)
        runtime.write_bytes(plistlib.dumps({"CFBundleIdentifier": "de.example.app.runtime.x86_64"}))
        original = runtime.read_bytes()
        with self.assertRaisesRegex(ValueError, "Invalid bundle identifier"):
            bundle.prepare_bundle(self.app)
        self.assertEqual(original, runtime.read_bytes())
        runtime.write_bytes(plistlib.dumps({"CFBundleIdentifier": "de.example.app.runtime.x86-64"}))
        bundle.prepare_bundle(self.app)

    def test_requires_a_valid_root_identifier(self):
        for identifier in (None, "", "de.example_bad", "de.example app", 123):
            with self.subTest(identifier=identifier):
                value = {} if identifier is None else {"CFBundleIdentifier": identifier}
                self.info.write_bytes(plistlib.dumps(value))
                with self.assertRaisesRegex(ValueError, "Invalid bundle identifier"):
                    bundle.inspect_bundle(self.app)

    def test_preserves_binary_plists_and_signature_bytes(self):
        self.info.write_bytes(plistlib.dumps({"CFBundleIdentifier": "de.example.app"}, fmt=plistlib.FMT_BINARY))
        signature = self.contents / "_CodeSignature/CodeResources"
        signature.parent.mkdir()
        signature.write_bytes(b"existing signature fixture")
        original = {path: path.read_bytes() for path in (self.info, signature)}
        bundle.prepare_bundle(self.app)
        bundle.prepare_bundle(self.app)
        for path, content in original.items():
            self.assertEqual(content, path.read_bytes())

    def test_accepts_internal_framework_links(self):
        framework = self.contents / "Frameworks/QtCore.framework"
        version = framework / "Versions/A"
        version.mkdir(parents=True)
        (version / "QtCore").write_bytes(b"library")
        (framework / "Versions/Current").symlink_to("A", target_is_directory=True)
        (framework / "QtCore").symlink_to("Versions/Current/QtCore")
        bundle.prepare_bundle(self.app)
        self.assertTrue((framework / "QtCore").is_symlink())

    def test_rejects_external_and_broken_links_without_touching_their_targets(self):
        source = self.root / "private-file"
        source.write_bytes(b"private")
        source.chmod(0o600)
        link = self.contents / "linked-file"
        for target in (source, self.root / "missing"):
            with self.subTest(target=target.name):
                link.symlink_to(target)
                with self.assertRaisesRegex(ValueError, "Broken or external"):
                    bundle.prepare_bundle(self.app)
                self.assertEqual(0o600, stat.S_IMODE(source.stat().st_mode))
                link.unlink()

    def test_rejects_non_bundle_paths(self):
        with self.assertRaisesRegex(ValueError, "staged .app"):
            bundle.prepare_bundle(self.root)
        linked = self.root / "Linked.app"
        linked.symlink_to(self.app, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "staged .app"):
            bundle.prepare_bundle(linked)

    def test_cli_inspection_is_read_only_and_fails_before_preparation(self):
        self.info.chmod(0o600)
        command = [sys.executable, "-B", str(Path(bundle.__file__)), "inspect", "--app", str(self.app)]
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertNotEqual(0, result.returncode)
        self.assertIn("not readable", result.stderr)
        self.assertEqual(0o600, stat.S_IMODE(self.info.stat().st_mode))
        command[3] = "prepare"
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(0, result.returncode, result.stderr)
        command[3] = "inspect"
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(0, result.returncode, result.stderr)


if __name__ == "__main__":
    unittest.main()
