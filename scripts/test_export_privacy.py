#!/usr/bin/env python3
"""Check policy delivery and agreement between the app and all Store URLs."""

from pathlib import Path
import tempfile
import unittest

from export_privacy import ROOT, export_privacy, privacy_url


class PrivacyDeliveryTest(unittest.TestCase):
    def test_exact_canonical_document_and_url_without_translated_copies(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "Privacy"
            export_privacy(output)
            self.assertEqual({"PRIVACY.md", "privacy-url.txt"}, {p.name for p in output.iterdir()})
            self.assertEqual((ROOT / "PRIVACY.md").read_bytes(), (output / "PRIVACY.md").read_bytes())
            self.assertEqual(privacy_url(), (output / "privacy-url.txt").read_text().strip())

    def test_existing_delivery_is_preserved(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "Privacy"
            output.mkdir()
            marker = output / "PRIVACY.md"
            marker.write_text("Existing delivery", encoding="utf-8")
            with self.assertRaises(FileExistsError):
                export_privacy(output)
            self.assertEqual("Existing delivery", marker.read_text())
            self.assertEqual([marker], list(output.iterdir()))

    def test_runtime_readme_and_localized_store_fields_share_this_apps_url(self):
        url = privacy_url()
        self.assertRegex(url, r"^https://github\.com/wasiliy-strecker/[a-z0-9-]+/blob/main/PRIVACY\.md$")
        self.assertIn(url, (ROOT / "README.md").read_text())
        java = list((ROOT / "src/main/java").glob("**/PrivacyPolicy.java"))
        runtime = java[0] if java else ROOT / "src/ui/privacy_link.cpp"
        self.assertIn('"' + url + '"', runtime.read_text())
        for field in (ROOT / "store-listings").rglob("privacy-url.txt"):
            self.assertEqual(url, field.read_text().strip(), str(field))
        policy = (ROOT / "PRIVACY.md").read_text()
        self.assertTrue(policy.startswith("# Privacy policy for "))
        self.assertIn("contact@appfabrik-ai.de", policy)
        self.assertIn("support@appfabrik-ai.de", policy)
        self.assertIn("Reading this policy on GitHub", policy)
        self.assertIn(url, policy)
        self.assertNotIn(";", policy)
        self.assertNotIn("This policy is included in the app and works offline", policy)
        self.assertNotIn("This privacy policy is included in the app", policy)


if __name__ == "__main__":
    unittest.main()
