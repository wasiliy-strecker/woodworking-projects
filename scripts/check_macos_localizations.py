"""Check runtime Mac language declarations against the embedded app catalogs."""

import plistlib
from pathlib import Path
from xml.etree import ElementTree

ROOT = Path(__file__).resolve().parents[1]


def main():
    catalogs = {
        ElementTree.parse(path).getroot().attrib["language"].replace("_", "-")
        for path in (ROOT / "translations").glob("app_*.ts")
    }
    expected = {
        language if language == "pt-BR" else language.split("-")[0]
        for language in catalogs
    }
    with (ROOT / "packaging/macos/Info.plist.in").open("rb") as source:
        info = plistlib.load(source)
    declared = info["CFBundleLocalizations"]
    assert len(declared) == len(set(declared)), "Duplicate Mac localizations"
    assert set(declared) == expected, (declared, expected)
    assert info["CFBundleDevelopmentRegion"] == "en", "English fallback required"
    assert not info.get("CFBundleAllowMixedLocalizations", False)
    print("Mac localizations match all six app catalogs, with English fallback")


if __name__ == "__main__":
    main()
