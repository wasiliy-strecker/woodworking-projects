"""Check translation coverage, placeholders and copyable listing fields."""

import ast
import json
import re
from pathlib import Path
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
LANGUAGES = ("de-DE", "en-US", "es-ES", "fr-FR", "it-IT", "pt-BR")


def main():
    sources = set()
    pattern = r'QCoreApplication::translate\(\s*"App",\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\)'
    for path in (ROOT / "src").rglob("*.cpp"):
        for match in re.finditer(pattern, path.read_text(encoding="utf-8")):
            sources.add(
                "".join(
                    ast.literal_eval(part)
                    for part in re.findall(r'"(?:[^"\\]|\\.)*"', match[1])
                )
            )
    assert sources
    for language in LANGUAGES:
        tree = ET.parse(ROOT / "translations" / f"app_{language}.ts")
        assert tree.getroot().get("language") == language.replace("-", "_")
        messages = tree.findall("./context/message")
        assert len(messages) == len(sources), language
        assert {m.findtext("source") for m in messages} == sources, language
        for message in messages:
            source = message.findtext("source")
            translation = message.findtext("translation")
            assert translation and message.find("translation").get("type") is None, (
                language,
                source,
            )
            assert sorted(re.findall(r"%[1-9]", source)) == sorted(
                re.findall(r"%[1-9]", translation)
            ), (language, source)
            assert source.count("\n") == translation.count("\n"), (language, source)
            assert source.count("&") == translation.count("&"), (language, source)
        translations = {
            m.findtext("source"): m.findtext("translation") for m in messages
        }
        for group in (
            ("&Bearbeiten", "&Löschen"),
            (
                "&Projektname *",
                "Holz oder &Material",
                "&Checkliste",
                "Arbeitsschritt hin&zufügen",
                "&Aufbewahrungsort",
                "&Status",
                "&Notiz",
                "&Foto hinzufügen …",
                "Foto &entfernen",
            ),
        ):
            mnemonics = [
                re.search(r"&(.)", translations[key])[1].casefold() for key in group
            ]
            assert len(mnemonics) == len(set(mnemonics)), (language, group)
        listing = ROOT / "store-listings" / language
        for field, limit in (
            ("name", 50),
            ("short-description", 1000),
            ("description", 10000),
            ("features", 10000),
            ("captions", 1000),
        ):
            value = (listing / f"{field}.txt").read_text(encoding="utf-8")
            assert value.strip() and len(value) <= limit, (language, field)
            assert ";" not in value
            assert not re.search(r"(?m)^\s*(?:[-*#]|\d+[.)])\s", value), (
                language,
                field,
            )
        assert (
            len((listing / "captions.txt").read_text(encoding="utf-8").splitlines())
            == 4
        )
        fixture = json.loads(
            (ROOT / "assets/store_demo" / f"{language}.json").read_text(
                encoding="utf-8"
            )
        )
        assert len(fixture["items"]) == 5
        assert len(fixture["photos"]) == 5
        assert len(fixture["statuses"]) == 5
        assert set(fixture["statuses"]) == {"planned", "started", "finished"}
        for photo in fixture["photos"]:
            if photo:
                assert (ROOT / "assets/store_demo" / photo).is_file()
        assert all(len(item) == 4 and all(item) for item in fixture["items"])
        assert (
            sum(
                fixture["search"].casefold() in " ".join(item).casefold()
                for item in fixture["items"]
            )
            == 1
        )
    print(f"{len(sources)} messages complete in {len(LANGUAGES)} languages")


if __name__ == "__main__":
    main()
