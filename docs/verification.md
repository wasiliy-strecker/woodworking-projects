# Prüfbericht

## Internationalisierung, 29. Oktober 2026

161 fachliche Qt-Testausführungen plus 16 Initialisierungs- und Abschlussaufrufe
bestehen. Alle neun CTest-Suiten einschließlich Katalogprüfung bestehen in Release
und ASan/UBSan. Compilerwarnungen, clang-tidy, Formatter, Ruff und Shell-Syntax
sind geprüft. Sechs eingebettete Kataloge enthalten je 143 Meldungen.

Zwölf echte Prozesse der gebündelten Laufzeit prüfen sechs Sprachen. Der Ablauf
umfasst Unicode, native Fotoauswahl, PNG/JPEG, drei Statuswerte, leere Schritte,
gleiche Texte mit unabhängigen UUIDs, Häkchen und Positionslücken. SQLite-Sperren
erhalten den Entwurf. Verwerfen und Neustart erhalten Checklisten und Fotobytes.
Der bisherige deutsche Komplettablauf besteht zusätzlich mit 14 Nachweisbildern.
24 Vorschauaufnahmen zeigen die reale Linux-App und wurden visuell geprüft.

Nachweise: `build/international-release-check.log`, `build/international-asan-check.log`,
`build/international-captures-final/` und `build/international-legacy-gui-final/`.
Die Laufzeit liegt in `build/staging/international-20261029/holzwerkprojekt-0.1.0-linux-x64/`.
Quell-, Laufzeit- und Nachweishashes sichern die Zuordnung zum lokalen Commit.
Windows-Abnahme, endgültige Windows-Aufnahmen und Store-Einreichung bleiben offen.

## Historischer Erststand

Stand: 27. Oktober 2026, HolzwerkProjekt 0.1.0.
Alle Testdaten sind synthetisch. Normale Benutzerdaten wurden nicht geöffnet.

## Umgebung und automatische Prüfungen

Ubuntu 24.04.4 LTS x64, GCC 13.3.0, Qt 6.8.3, CMake 4.4.4, Ninja 1.11.1,
clang-format 23.1.3 und clang-tidy 22.1.8 wurden lokal ermittelt.
Der Standarddatenpfad wurde ohne Datenbanköffnung per QStandardPaths ermittelt:
`/home/unknown/.local/share/de.appfabrik.holzwerkprojekt`.
Das dynamische Qt-SDK wurde mit GCC 10.3.1 gebaut. Die kompatible Linux-GCC-ABI
wird mit dem App-Compiler GCC 13.3.0 verwendet. Qt Test bestätigt Qt 6.8.3.
JPEG-Unterstützung stammt aus dem Qt-Plugin mit libjpeg-turbo 3.1.0, PNG aus
Qt Gui mit libpng 1.6.47. Die SDK-SPDX-Metadaten bestätigen diese Komponenten.

```bash
source scripts/env.sh
bash scripts/check.sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 bash scripts/check.sh asan
cmake --preset release
cmake --build --preset release --parallel 4
ctest --preset release
```

109 fachliche Testausführungen bestehen in sieben CTest-Suiten.
Qt Test meldet außerdem je Suite Initialisierung und Abschluss.
Die vollständigen Suiten bestehen in Debug, Release und unter Sanitizern.
Keine Fehler und keine übersprungenen Fälle auf dieser Linux-Umgebung.

| Suite | Fachliche Ausführungen | Schwerpunkte |
| --- | --- | --- |
| domain | 16 | Unicode-Pflichtfeld, optionale Felder, Notiz, Suche, Sortierung, isolierter Pfad |
| repository | 17 | CRUD, IDs, Duplikate, Persistenz, SQL-Texte, Sperren, Rechte, fremde und beschädigte Dateien |
| dialogs | 14 | Echte Qt-Dialoge, Abbruchwege, sichere Vorauswahl, Löschbestätigung, Fehler, Threadgrenzen, kleine Fenster |
| photo | 19 | JPEG/PNG, tatsächlicher Inhalt, Beschädigung, Byte-/Seiten-/Pixelgrenzen, Wachstum, EXIF, BLOB-Persistenz |
| status | 10 | Enum-Zuordnung, Standards, jeder Statuswechsel, ungültige Daten, Verwerfen und realer Commitfehler |
| photo_dialogs | 8 | Reine Fotoänderungen, ungültiger Import, Dateiauswahlabbruch, späte Ergebnisse, zerstörte Fenster |
| checklist | 25 | UUIDs, Reihenfolge, Suche, Constraints, Beschädigung, Transaktionen, echter Dialog und Verwerfschutz |

Compilerwarnungen für eigenen C++-Code werden als Fehler behandelt.
Formatter und clang-tidy sind ohne verbleibende Befunde.
AddressSanitizer, LeakSanitizer und UndefinedBehaviorSanitizer bestehen ohne
Unterdrückungsdateien. Das vorgebaute Qt-SDK und dessen SQLite-/Bilddecoder sind
selbst nicht mit diesen Sanitizern instrumentiert. Deren innere Ausführung ist
daher nicht im gleichen Umfang geprüft wie eigener App-Code.
Qt-Offscreen-Hinweise und die absichtlich ausgelöste JPEG-Decoderwarnung sind
bekannte Ausgaben der Fehlerfälle, keine Compiler- oder Sanitizerbefunde.

Ruff und Python-Formatprüfung, Shell-Syntaxprüfung sowie JSON-/XML-Prüfungen
bestehen. PowerShell, Windows-Ressourcencompiler, Xcode und Apple-Signierwerkzeuge
wurden hier nicht ausgeführt.

## Nachweise der Atomarität und Entwürfe

Ein echter SQLite-Trigger lässt erst einen späteren Schritt-INSERT scheitern,
nachdem die Projektzeile geändert und der erste Schritt bereits eingefügt wurde.
Der Test prüft danach alle ursprünglichen Projektfelder, Foto, Status und Schritte.
Ein fehlgeschlagenes Anlegen hinterlässt keine zusätzlichen Projekt- oder Schrittzeilen.
Nach Entfernen des Triggers wird derselbe Entwurf erfolgreich gespeichert.
Ein zusätzlicher Leser verhindert den Commit einer Änderung und belegt auch dort
den vollständigen Rollback. SQLite-Schreibsperren laufen außerhalb des GUI-Threads.

Der echte Projektdialog bleibt bei ungültigem Schritttext und spätem SQL-Fehler
mit Fotobytes, Texten, Status, vollständigen Zeilen und Häkchen offen.
Ein weiterer Speicherversuch gelingt. Doppelaktionen sind gesperrt.
Abbrechen, Escape, Dialog- und Hauptfensterschließen schützen Änderungen.
Reines Abhaken, Wiederöffnen, Umbenennen, Hinzufügen und Entfernen werden geprüft.
Ein vollständig wiederhergestellter Entwurf schließt ohne Rückfrage.

Schritte mit gleichem Text und Projekte mit gleichem Namen bleiben unabhängig.
Entfernen der mittleren Zeile verändert keine IDs oder verbleibenden Positionen.
Statuswechsel erhalten alle Schrittwerte. Alle Schritte zu erledigen verändert
keinen Status. Ein fertiges Projekt mit offenen Schritten ist gültig.

Reale Datenbankconstraints werden mit unerlaubten SQL-Mutationen getestet.
Beschädigte Schritt-IDs, Boolean-Werte, Positionen, Datentypen und Unicode-Leertexte
werden beim Laden gemeldet. Projektlöschung entfernt nur zugehörige Schritte.
Suche über mehrere passende Schritte liefert genau ein Projekt.

Fotos werden nach Entfernung der Quelle und nach Wiederöffnen aus dem BLOB gelesen.
Unabhängige Fotos gleichnamiger Projekte bleiben erhalten.
Ein absichtlich verzögertes Repository prüft schnelle Auswahlwechsel und zerstörte
Fenster. Ungültige gespeicherte Fotobytes öffnen keinen leeren Bearbeitungsentwurf.
Die automatisierte Dateiauswahlprüfung verwendet unter Offscreen den Qt-Fallback.
Eine native Dateiauswahl benötigt den ergänzenden sichtbaren Ablauf.

## Sichtbare Abnahme und Paket

Der reproduzierbare Bedienhelfer liegt unter `scripts/gui_smoke.py`.
Er hält auf DISPLAY=:0 die gemeinsame Sperre über den gesamten Bedienlauf.
Er sucht Fenster anhand des selbst gestarteten Prozesses, kontrolliert deren ID
und verwendet tatsächliche Clientkoordinaten. Tastaturereignisse gehen nur an
den kontrollierten eigenen Fokus. Er beendet ausschließlich seinen eigenen Prozess.

```bash
DISPLAY=:0 python3 scripts/gui_smoke.py --binary build/dev/holzwerkprojekt --output build/manual/dev-visible
DISPLAY=:0 python3 scripts/gui_smoke.py --binary /absoluter/paketpfad/HolzwerkProjekt.sh --output build/manual/package-visible --clean-environment
```

Die Entwicklungsapp besteht den vollständigen Ablauf in einem eigenen Xephyr-
Fenster, sichtbar auf DISPLAY=:0, mit eigenem Openbox und nativer GTK3-Dateiauswahl.
Das innere Display wurde frei vergeben, hier :2. Die gemeinsame Sperre blieb über
Serverstart, Bedienung und Abbau gehalten. Kein fremdes Display wurde verwendet.
Die ersten direkten GNOME-Automationsversuche waren wegen abweichender Client-
und Rahmenkoordinaten nicht zuverlässig und gelten nicht als bestandene Abnahme.
Ein sichtbarer Screenshot des gesamten eigenen Prüfbildschirms belegt den Aufbau.

```bash
DISPLAY=:0 python3 scripts/visible_desktop.py --binary build/dev/holzwerkprojekt --output build/manual/dev-visible
```

Dieser zusätzliche Helfer benötigt Xephyr und Openbox. Hier wurde Openbox 3.6.1
mit seinen Abhängigkeiten aus den Ubuntu-Paketen lediglich in die ignorierte
`.tools/desktop/root` entpackt. Es wurde nichts systemweit installiert. Der lokale
Aufruf ergänzt `--wm-root .tools/desktop/root`. Python 3, xdotool, xsel und
ImageMagick dienen nur Tests, sie gehören nicht zur ausgelieferten Anwendung.

| Sichtbarer Ablauf | Ergebnis |
| --- | --- |
| Leere Sammlung und ungültiger Projektname | Verständliche Hinweise ohne Datensatz |
| Texte, echte neue Notizzeile, Foto, Status und drei Schritte | Vollständig gespeichert |
| Native Dateiauswahl abbrechen und PNG importieren | Entwurf erhalten und Foto übernommen |
| Unicode-Suche über Schritttexte, keine Treffer, Suche leeren | Jeweils verständlicher Zustand |
| Alleinige Häkchenänderung, Escape und sichere Vorauswahl | Weiterbearbeiten erhält Entwurf, Verwerfen erhält Datenbank |
| Mittlere Zeile entfernen, verbleibende Zeile umbenennen | Richtige UUID bearbeitet, Positionen 0 und 2 erhalten |
| Abhaken, Wiederöffnen, neue Zeile und Leertextfehler | Gesamter Entwurf erhalten, neue Position 3 |
| JPEG-Ersatz und Statuswechsel Fertig → Geplant → Angefangen | Foto und Schritte unabhängig vom Status |
| Echter später Schritt-INSERT-Fehler | Keine Teiländerung, Dialog bleibt offen, Wiederholung gelingt |
| Foto entfernen und verwerfen, beim zweiten Projekt dauerhaft entfernen | Unabhängige Fotowerte |
| Gleichnamiges zweites Projekt mit eigenen Schritten | Unabhängige Projekt- und Schritt-IDs |
| Prozessneustart nach Entfernen beider Bildquellen | Foto, Status, Texte, Häkchen und Reihenfolge erhalten |
| Löschabbruch und beide Projekte nacheinander löschen | Nur zugehörige Schrittzeilen verschwinden |

Die Datensätze wurden zusätzlich über eine unabhängige lesende SQLite-Verbindung
geprüft. Der Helfer kontrolliert UUIDs, Positionen, Statusschlüssel und Bildbytes.
Die synthetischen Originalbilder bleiben bis zu ihrer ausdrücklichen Entfernung
durch den Test unverändert. Die App selbst entfernt niemals Quellen.

Zusätzlich wurden echte Dialogfenster direkt auf DISPLAY=:0 mit 18-Punkt-Schrift
und 400 × 340 Pixeln geprüft, ebenfalls unter der gemeinsamen Sperre.
Eine Variante enthält 30 Schritte. Textfelder bleiben scrollbar und Speichern
sowie Abbrechen sichtbar. Die Screenshots wurden visuell angesehen.
Die sichtbare Abnahme entdeckte den fehlenden Tastaturfokus für Alt+C. Dieser
wurde korrigiert und zusätzlich im echten automatisierten Dialog geprüft.

Ausgewählte Belege liegen unter `docs/screenshots`, vollständige lokale Abläufe
und Ergebnisdateien unter `build/manual`.

## Eigenständiges Linux-Paket

Ein Ubuntu-24.04-x64-Testpaket wurde erzeugt und außerhalb des Projektverzeichnisses
unter `/tmp/holzwerkprojekt-package-test-bd1y3ief/HolzwerkProjekt` geprüft.
`PATH` enthielt nur `/usr/bin:/bin`. Externe Qt-, QML-, SDK- und Bibliothekspfade
wurden entfernt. Der eigene Paketlauncher setzt ausschließlich seine Laufzeitpfade.

```bash
DISPLAY=:0 python3 scripts/visible_desktop.py --wm-root .tools/desktop/root --binary /tmp/holzwerkprojekt-package-test-bd1y3ief/HolzwerkProjekt/HolzwerkProjekt.sh --output build/manual/package-visible --clean-environment
```

Der vollständige oben beschriebene Bedienablauf besteht auch mit diesem Paket.
Native Dateiauswahl, PNG und JPEG, Fotoersatz und Entfernung, Statuswechsel in
beide Richtungen, sämtliche Schrittaktionen, reiner Häkchenabbruch, später
SQL-Fehler und erneutes Speichern wurden sichtbar ausgeführt. Nach Entfernen
der Bildquellen und Prozessneustart sind Foto, Status, Schritttexte, Häkchen
und Positionen 0, 2 und 3 unverändert. Abschließendes Löschen lässt keine Schritte
zurück. SQLite wurde zusätzlich unabhängig gelesen.

`/proc/<pid>/maps` bestätigt alle geladenen Qt- und ICU-Bibliotheken sowie
QSQLITE, JPEG und das GTK3-Plattformthema innerhalb des Paketordners.
Die Belege stehen in `build/manual/package-visible/result.json` und
`loaded-libraries.json`. Übliche Linux-Desktopbibliotheken bleiben Systemvoraussetzungen.
Das finale Lieferarchiv unter `build/releases` erhält nur aktualisierte Dokumentation.
Sein Launcher, Programm und sämtliche Bibliotheken, Plugins und Übersetzungen
werden byteweise mit dem sichtbar geprüften Paket verglichen.

## Noch offene Zielsysteme

Windows-x64-Paket mit MSIX-Vorlage und macOS-App-Bundle/DMG sind vorbereitet.
Sie wurden auf diesem Linux-Rechner nicht nativ gebaut oder ausgeführt.
Windows-ACLs, native Installation und Updates, beide macOS-Architekturen,
App-Sandbox, native Systemdialoge und plattformspezifische Tastenkürzel benötigen
eine eigene Abnahme ohne installiertes SDK. Das Linux-Paket ist ein ergänzender
Nachweis und ersetzt diese Prüfungen nicht.

Store-Identitäten, Signierung, Lizenzkonformität der konkreten Store-Auslieferung,
Metadaten und Einreichung stehen aus. Veröffentlichung zuerst im Microsoft Store,
danach im Mac App Store. Kein Push und keine Veröffentlichung.
