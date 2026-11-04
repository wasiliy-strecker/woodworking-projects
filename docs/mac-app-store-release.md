# Woodworking Projects: Mac App Store

Store-Name **Woodworking by Appfabrik**, installierter Name **Woodworking Projects**.
Bundle-ID `de.appfabrik.woodworkingprojects`, App-ID `6830850301`, Version 1.0, Build 1.
Primärsprache en-US, sechs Store-/App-Sprachen, kostenlose weltweite Verfügbarkeit.
Bestehende Datenpfade, SQLite-Schema und JSON-Format bleiben unverändert.

## Vorbereitung

Python 3.12, CMake, Ninja, Xcode und Qt 6.8.3 macOS (Universal-SDK) bereitstellen.
Maschinenpfade gehören in die ignorierte `.tools/env.sh`. Mit passender Umgebung:

```sh
bash scripts/check.sh
python3 -B scripts/test_macos_bundle.py
python3 -B scripts/test_macos_release.py
```

Das Prüfskript unterstützt macOS-Bash 3.2. Wenn clang-tidy fehlt, prüft
`scripts/analyze_macos.py` alle eigenen Übersetzungseinheiten mit Apple Clang
Static Analyzer. Das ist im Prüfbericht ausdrücklich von clang-tidy zu trennen.
Die üblichen Compilerwarnungen bleiben Fehler. Separate GUI-Tests ersetzen keine
Abnahme im signierten Sandbox-Paket.

Die unter `packaging/macos/qt-sources.json` festgelegten vollständigen Qt-Archive
in `build/qt-source-cache/` herunterladen und ihre Prüfsummen bestätigen.
Sie werden zusammen mit Lizenzen, SPDX-Inventaren und Austauschhinweisen in
jedes Store-Bundle eingebettet. Keine zusätzliche Qt-Installation beim Nutzer.

## Bauen und signieren

```sh
python3 scripts/macos_release.py build --qt-root "$WOODWORKING_PROJECTS_QT_ROOT" \
  --build-dir build/mac-compile-<kennung> --output build/mac-build-<kennung>
```

Für Entwicklung und Store jeweils eine neue Kopie der erzeugten `.app` sowie
der danebenliegenden `build-report.json` anlegen. Das Build-Inventar und der
Quellendigest müssen vor der Signierung exakt passen. Konfiguration aus
`signing.example.json` in `development.local.json` bzw. `store.local.json` ablegen.
Profile bleiben privat außerhalb des Repositorys; nur die eingebettete Kopie
erhält lesbare Dateirechte.

```sh
python3 scripts/macos_release.py sign --mode development \
  --app 'build/mac-development-<kennung>/Woodworking Projects.app' \
  --config packaging/macos/development.local.json
python3 scripts/macos_release.py sign --mode store \
  --app 'build/mac-store-<kennung>/Woodworking Projects.app' \
  --config packaging/macos/store.local.json
```

Inspektion prüft Kennungen, Version, Dateirechte, interne Symlinks, alle nativen
Architekturen, SDK-unabhängige Bibliotheksverweise und die Quellenarchive.
Signierte Inspektion prüft die gesamte Signatur und die engen Sandbox-Rechte.
Kein Netzwerkentitlement oder pauschal deaktivierter Bibliotheksschutz.

## Native Abnahme

ARM64 und Intel/Rosetta sichtbar starten; Methode und Grenzen festhalten.
Tatsächlichen Containerpfad und die geladenen gebündelten Qt-/SQLite-Bibliotheken
prüfen. Die eigenen Projekte nicht mit externen Skripten bearbeiten.
Nutzer prüft Fotoimport (JPEG/PNG), Foto-/Farberhalt ohne ursprüngliche Bilddatei,
Checklisten (hinzufügen, bearbeiten, abhaken, entfernen), Statuswechsel (Geplant/Angefangen/Fertig), Anlegen/Bearbeiten/Löschen, Suchreset nach Neuanlage, Sortierung,
Abbruchschutz, Fotos/Farben, Neustart, Sprache, native Bearbeiten-Menüs sowie JSON-Import und
-Export einschließlich Überschreibbestätigung und Abbruch. Die Probe zum lokalen
Qt-Austausch muss eine separate Kopie mit eigenem Container verwenden.

`acceptance.example.json` nach `acceptance.local.json` kopieren. Nur tatsächlich
beobachtete oder vom Nutzer ausdrücklich bestätigte Felder setzen und die Art
der Evidenz dokumentieren. Ohne vollständige passende Abnahme erzeugt der Helfer
kein Installer-Paket. Vor einem Upload erneut auf vorhandene Buildnummer prüfen.

```sh
python3 scripts/macos_release.py package \
  --app 'build/mac-store-<kennung>/Woodworking Projects.app' \
  --config packaging/macos/store.local.json \
  --acceptance packaging/macos/acceptance.local.json \
  --output build/mac-store-<kennung>/Woodworking-Projects-1.0-build-1-universal.pkg
```

Danach `altool --validate-app`, Upload, Apples Verarbeitung und Review-Einreichung
getrennt protokollieren. Erst nach Nutzerbestätigung einreichen. Upload bedeutet
noch keine Einreichung, WAITING_FOR_REVIEW noch keine Veröffentlichung.

## Screenshots

`-DWOODWORKING_PROJECTS_STORE_CAPTURE=ON` baut einen separaten Entwicklerhelfer
`mac_store_capture`. Er verwendet die echten Produktionswidgets und SQLite mit
synthetischen Sprachfixtures. Auf macOS mit Cocoa ausführen:

```sh
build/dev/mac_store_capture en-US assets/store_demo/en-US.json build/mac-shots/en-US
```

Die Bilder sind unveränderte Qt-Fensteraufnahmen in 1280×800 bzw. 2560×1600,
keine nachgebauten Oberflächen. Der Helfer wird nicht mit der App ausgeliefert.
Alle Aufnahmen vor Upload visuell prüfen. Für die übrigen Sprachen wiederholen.

Qt-LGPL-Quellen-/Austauschunterlagen: `docs/qt-library-replacement.md`.
Die technischen Prüfungen sind keine pauschale rechtliche Zusicherung aller
Store-Vertragsbedingungen. Apple behält die Entscheidung über die Zulassung.
