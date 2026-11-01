# Internationalisierung und Vorschauordner

Aktueller öffentlicher Name: **Woodworking Projects**. Alle sechs Oberflächensprachen bleiben
verfügbar. Neue Installationen verwenden die unterstützte Systemsprache mit Englisch
als Fallback. Die technischen Kennungen und Datenverzeichnisse bleiben unverändert.
macOS erlaubt Zugriff auf ausgewählte Dateien und Ordner mit Lese-/Schreibrechten.
Die Export-Ordnerwahl gewährt Zugriff auch für temporäre Nachbardateien.
Native macOS-Sandbox und ein physischer exFAT-Datenträger benötigen eigene Abnahme.


HolzwerkProjekt 0.1.0 unterstützt de-DE, en-US, es-ES, fr-FR, it-IT und pt-BR.
Die sechs TS-Kataloge enthalten je 143 Meldungen. Eigene Oberflächen, Fotoimport,
Validierung, Datenbankfehler und Sprachwahl verwenden diese Kataloge. Passende
Qt-Basisübersetzungen für Standarddialoge und Text-Kontextmenüs sind eingebettet.
Bedienelemente nativer Dateiauswahldialoge können der Betriebssystemsprache folgen.

Beim ersten Start wird die passende unterstützte Systemsprache verwendet.
Nicht unterstützte Sprachen fallen auf Englisch zurück. Die Auswahl unter
„Sprache“ oder Strg+, wird atomar in `language.txt` neben der Datenbank gespeichert
und gilt ab Neustart. Fehler erhalten die bisherige Einstellung. App-ID,
Datenpfad, Schema 1, vier Textfelder, Statuscodes, Schritt-UUIDs, Positionslücken und Foto-BLOBs bleiben erhalten.
Regionale Zahlen und Sortierung folgen der aktiven Sprache. Die vierte
Portfolio-Stufe erhält keine weitere fachliche Hauptfunktion.

## Quellen und Prüfung

`src/i18n` installiert die Übersetzungen vor Fenstern und Worker.
`src/persistence/language_preferences.*` liest und speichert die Auswahl.
`src/ui/language_dialog.*` verwendet den vorhandenen Worker und behandelt
Schreibfehler ohne Verlust der Auswahl. Übersetzungen bleiben pro Prozess stabil.

```bash
source scripts/env.sh
bash scripts/check.sh release
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 bash scripts/check.sh asan
python3 scripts/check_translations.py
ruff check scripts
ruff format --check scripts
```

161 fachliche Qt-Testausführungen decken die bisherigen Daten- und Fotoregeln
sowie Sprachauflösung, regionale Zahlen, Sortierung, übersetzte Widgets,
Fehler, kleine Fenster, Einstellungen und unveränderte Bestandsdaten ab.
Die zusätzliche Katalogprüfung prüft Vollständigkeit, Platzhalter, Zeilenumbrüche,
eindeutige Tastenkürzel pro Formular, Store-Felder und synthetische Beispieldaten.

## Laufzeit vorbereiten

Die Aufnahme verwendet das gebündelte Release-Programm. Die Qt-Installation
muss LinguistTools und die passenden Qt-Basisübersetzungen enthalten.

```bash
source scripts/env.sh
python3 scripts/package_linux.py --qt "$HOLZWERKPROJEKT_QT_ROOT" \
  --stage-only --output-dir build/staging/international
```

Bei lokal bereitgestelltem libxcb-cursor zusätzlich `--cursor-library` und
`--cursor-license` übergeben. Bereits vorhandene Lieferungen werden nicht
überschrieben. Für weitere Durchläufe ein neues Staging-Verzeichnis wählen.

## Echte Fenster aufnehmen

Benötigt werden Xvfb, Openbox, xdotool, xsel, ImageMagick und Python 3.
Der Helfer startet ein eigenes Display mit eigenem Fensterverwalter. Der Desktop
des Nutzers bleibt verfügbar. Alle Daten liegen in temporären Verzeichnissen.

```bash
python3 scripts/store_assets/capture.py \
  --binary build/staging/international/holzwerkprojekt-0.1.0-linux-x64/HolzwerkProjekt.sh \
  --output build/international-captures-final --clean-environment
```

Bei lokalen Werkzeugen `--xvfb /pfad/Xvfb` und
`--window-manager /pfad/openbox` ergänzen. Das Ausgabeverzeichnis muss neu sein.
`--language en-US` erlaubt einen einzelnen Prüflauf, eine vollständige Lieferung
verlangt alle sechs Sprachen.

Je Sprache entstehen vier echte Aufnahmen mit 1920 × 1080 Pixeln: Sammlung mit
Foto, Bearbeitungsdialog, Suche und Sprachwahl. Sie zeigen die laufende Linux-App,
keine nachgebauten Oberflächen. Die fünf Holzwerkprojekte pro Sprache sind erfunden.
Wandregal, Werkzeugkiste und Gartenbank sind eigene SVG-Zeichnungen mit PNG-/JPEG-Ableitungen.
Vorhandenes App-Branding und das ergänzte 300-Pixel-Store-Symbol bleiben app-lokal.

Zwölf echte App-Prozesse prüfen zusätzlich Unicode-CRUD, Pflichtfeld,
native Fotoauswahl samt Abbruch, PNG/JPEG, ungültige Bilder, Fotoersatz und
Entfernung, sichere Abbruchvorauswahl, echte SQLite-Sperren und Wiederholung,
alle Statuswechsel, reinen Statusabbruch, leere Schritte, doppelte Schritttexte mit
unabhängigen UUIDs, Häkchen, Positionslücken, reinen Häkchenabbruch, kleine Fenster, Einstellungsfehler und Neustart in der nächsten Sprache.
Nach Löschung der importierten Quelldateien müssen gespeicherte Fotos und
Unicode-Texte und Checklisten unverändert bleiben. Prozessabbildungen prüfen Qt, QSQLITE,
JPEG und GTK3-Plattformhelfer aus dem eigenen Paket. Die Umgebung enthält keine
Entwicklungs-SDK-Pfade. Quellen und Laufzeitdateien werden vor und nach dem
Durchlauf gehasht. `report.json` bindet Aufnahmen und Nachweise an diesen Stand.

## Als Ordner liefern

Store-Texte liegen in `store-listings/<Sprache>/`. Jede Datei enthält ausschließlich
den direkt kopierbaren Feldinhalt. Englisch ist die Vorgabesprache der Listings.
Nach Prüfung der Bilder und lokalem App-Commit:

```bash
python3 scripts/store_assets/assemble.py \
  --capture build/international-captures-final \
  --check-log build/international-release-check.log \
  --check-log build/international-asan-check.log
```

Der Helfer verlangt einen sauberen Git-Stand und übereinstimmende Quellen,
Laufzeitdateien und Aufnahmen. Er erzeugt unter `build/releases/international/`
einen neuen Ordner `holzwerkprojekt-0.1.0-linux-preview-<Commit>` mit `Store`,
`Branding`, `Verification`, Manifest und SHA-256-Prüfsummen. Kein ZIP oder anderes
Lieferarchiv. Der Vorschauordner enthält Assets und Nachweise. Die geprüfte
Linux-Laufzeit bleibt separat im Staging-Verzeichnis.

Die Bilder sind Linux-Vorschauen. Windows-Build, endgültige Windows-Aufnahmen,
Installation und Update ohne Datenverlust, Skalierung, native Dateiauswahl,
echte Store-Identität, Signierung und Einreichung bleiben eigene Folgeschritte.
Das bestehende Windows-Manifest und die Store-Sprachressourcen müssen bei diesem
nativen Schritt abgestimmt werden. Diese Lieferung ist keine Store-Freigabe.
