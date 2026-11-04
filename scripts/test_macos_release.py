"""Guards for source/signature matching and required native acceptance."""

import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import macos_release as release
import replace_macos_qt


class ReleaseTests(unittest.TestCase):
    def test_source_digest_ignores_private_config_but_tracks_public_sources(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for name in ("CMakeLists.txt", "CMakePresets.json", "README.md", "THIRD_PARTY_NOTICES.md",
                         ".clang-format", ".clang-tidy"):
                (root / name).write_text("fixture")
            (root / "packaging").mkdir()
            with patch.object(release, "ROOT", root):
                before = release.source_digest()
                (root / "packaging/signing.local.json").write_text("private")
                self.assertEqual(before, release.source_digest())
                (root / "README.md").write_text("changed")
                self.assertNotEqual(before, release.source_digest())

    def test_inventory_tracks_symlink_target_without_following_it(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "actual").write_bytes(b"library")
            (root / "current").symlink_to("actual")
            result = release.inventory(root)
            self.assertEqual(result["actual"], hashlib.sha256(b"library").hexdigest())
            self.assertEqual(result["current"], {"symlink": "actual"})

    def test_package_refuses_incomplete_user_acceptance_before_productbuild(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "signing-report.json").write_text('{}')
            acceptance = root / "acceptance.json"
            acceptance.write_text(json.dumps({"arm64Execution": True}))
            with patch.object(release, "inspect"), patch.object(release, "run") as run:
                with self.assertRaisesRegex(ValueError, "complete native"):
                    release.package(root / "Sample.app", root / "config.json", acceptance, root / "app.pkg")
                run.assert_not_called()

    def test_qt_replacement_refuses_existing_output_without_modifying_it(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            app = root / "Source.app"
            (app / "Contents").mkdir(parents=True)
            (app / "Contents/Info.plist").write_text("fixture")
            output = root / "Existing.app"
            output.mkdir()
            marker = output / "keep.txt"
            marker.write_text("keep")
            with self.assertRaisesRegex(ValueError, "never replaced"):
                replace_macos_qt.replace(app, root / "frameworks", output)
            self.assertEqual(marker.read_text(), "keep")


if __name__ == "__main__":
    unittest.main()
