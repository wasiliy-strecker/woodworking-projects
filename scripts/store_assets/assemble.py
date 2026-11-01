"""Assemble a verified folder of localized listing assets. No archive is created."""

import argparse
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

from common import LANGUAGES, ROOT, png_dimensions, source_hashes

SCREENS = ("01-collection.png", "02-editor.png", "03-search.png", "04-language.png")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--check-log", type=Path, action="append", default=[])
    args = parser.parse_args()
    if subprocess.check_output(
        ["git", "status", "--porcelain"], cwd=ROOT, text=True
    ).strip():
        raise SystemExit("Commit the verified source before assembling the delivery.")
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
    ).strip()
    report = json.loads((args.capture / "report.json").read_text(encoding="utf-8"))
    assert report["source_hashes"] == source_hashes(), (
        "Captures do not match the current source"
    )
    assert {journey["language"] for journey in report["journeys"]} == set(LANGUAGES)
    assert len(report["journeys"]) == len(LANGUAGES)
    assert len(report["screenshots"]) == len(LANGUAGES) * len(SCREENS)
    for language in LANGUAGES:
        for name in SCREENS:
            relative = f"{language}/screenshots/{name}"
            path = args.capture / relative
            assert (
                hashlib.sha256(path.read_bytes()).hexdigest()
                == report["screenshots"][relative]
            )
            assert png_dimensions(path) == (1920, 1080)
    for relative, digest in report["evidence_hashes"].items():
        assert (
            hashlib.sha256((args.capture / relative).read_bytes()).hexdigest() == digest
        )
    runtime = Path(report["binary"]).parent
    for relative, digest in report["runtime_hashes"].items():
        assert hashlib.sha256((runtime / relative).read_bytes()).hexdigest() == digest
    version = re.search(
        r"project\(HolzwerkProjekt VERSION ([\d.]+)",
        (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"),
    )[1]
    output = (
        ROOT
        / "build/releases/international"
        / f"holzwerkprojekt-{version}-linux-preview-{commit[:7]}"
    )
    output.mkdir(parents=True, exist_ok=False)
    for language in LANGUAGES:
        store = output / "Store" / language
        shutil.copytree(args.capture / language / "screenshots", store / "screenshots")
        shutil.copytree(ROOT / "store-listings" / language, store / "text")
    shutil.copytree(ROOT / "assets/branding", output / "Branding")
    verification = output / "Verification"
    shutil.copytree(args.capture, verification)
    shutil.copy2(ROOT / "docs/verification.md", verification)
    shutil.copy2(ROOT / "docs/internationalization.md", verification)
    for log in args.check_log:
        shutil.copy2(log, verification / log.name)
    manifest = {
        "app": "Woodworking Projects",
        "version": version,
        "source_commit": commit,
        "languages": LANGUAGES,
        "default_listing_language": "en-US",
        "platform": "Linux X11 preview",
        "ready_for_store_submission": False,
        "screenshots_per_language": len(SCREENS),
        "dimensions": [1920, 1080],
        "gui_processes": sum(journey["processes"] for journey in report["journeys"]),
        "source_hashes": report["source_hashes"],
        "runtime_hashes": report["runtime_hashes"],
    }
    (output / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    (output / "LIESMICH.txt").write_text(
        "HolzwerkProjekt – internationale Linux-Vorschau\n\nStore enthält pro Sprache vier echte Screenshots und direkt kopierbare Textfelder.\nBranding enthält das vorhandene SVG- und PNG-Branding, Windows-Kacheln und das Store-Symbol mit 300 × 300 Pixeln.\nVerification enthält Quellnachweise und Prüfberichte.\n\nDie Screenshots zeigen die echte Linux-App mit synthetischen Beispieldaten auf einem isolierten Bildschirm. Sie sind Vorschauen. Endgültige Windows-Aufnahmen, Windows-Paketierung, Signierung und native Store-Abnahme folgen separat.\nKeine Veröffentlichung und kein GitHub-Push. Die Lieferung ist ein Ordner ohne ZIP-Datei.\n",
        encoding="utf-8",
    )
    sums = [
        f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(output)}"
        for path in sorted(output.rglob("*"))
        if path.is_file()
    ]
    (output / "SHA256SUMS").write_text("\n".join(sums) + "\n", encoding="utf-8")
    print(output)


if __name__ == "__main__":
    main()
