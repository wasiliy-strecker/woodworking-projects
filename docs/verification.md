# Prüfbericht

## Mac-Menüs und Systemdialogsprachen, 1. November 2026

Die App registriert auf macOS ein natives Bearbeiten-Menü mit Ausschneiden,
Kopieren, Einfügen und Alles auswählen. Die Standardaktionen verwenden die von
Qt erkannten, bereits eingebetteten Qt-Übersetzungen und Standardkürzel.
App-eigene Qt-Fenster erhalten jeweils eine Menüleiste, damit die Aktionen auch
in modalen Bearbeitungs- und Dateinamen-Dialogen verfügbar sind. Eine globale
Menüleiste dient als Rückfall für native Dialoge. Datei-, Farb-, Schrift- und
Meldungsdialoge werden durch den Menühelfer nicht mit Qt-Kindwidgets erweitert.

Qt-Textaktionen betreffen ausschließlich das aktuell fokussierte, sichtbare und
aktivierte Textfeld. Schreibgeschützte Inhalte bleiben unverändert. Native
Mac-Dialoge nutzen die Qt-/AppKit-Weiterleitung und deren eigene Aktionsprüfung.
Unter Linux und Windows wird der Helfer im normalen App-Start nicht aktiviert.
Die neuen Tests instanziieren ihn dort gezielt zur Prüfung der gemeinsamen Logik.

Die Mac-Konfiguration deklariert Deutsch, Englisch, Spanisch, Französisch,
Italienisch und brasilianisches Portugiesisch mit Englisch als Fallback.
Native Mac-Dateidialoge folgen ausdrücklich der macOS-Sprachwahl. Die eigene
Sprachwahl der App bleibt unabhängig und gilt weiterhin nach einem Neustart.
Es werden keine Apple-Spracheinstellungen geschrieben und keine App-Kennungen,
Datenpfade, Datenbankinhalte oder Exportformate geändert.

Zusätzliche Regressionstests: `native_edit_menus` und `macos_localizations`.
Sie prüfen Textaktionen, Unicode, Undo nach Ausschneiden, Fokuswechsel, modale
Dateinamen-Eingabe, schreibgeschützte Felder, normale Tastenkürzel, wiederholtes
Öffnen, unveränderte Dateidialoge sowie die Katalog- und Plist-Sprachzuordnung.
Die gemeinsamen Tests prüfen die sechs Sprachen einschließlich der vorhandenen
Systemsprachwahl, expliziten Präferenzen und des Englisch-Fallbacks.

### Ausgeführte Linux-Prüfung

Alle 17 CTest-Suiten bestehen in Development und unter AddressSanitizer,
LeakSanitizer und UndefinedBehaviorSanitizer für den eigenen Code.
Formatter, strikte Compilerwarnungen, clang-tidy und die Python-Prüfungen bestehen.
Die Menütests bestehen zusätzlich mit xcb und GTK3 auf einem eigenen X11-Display.
Der vollständige Bedienablauf der aktuellen Development-Datei in einer separaten
Testlaufzeit mit eigener Qt-Laufzeit besteht einschließlich Speichern und Neustart.
Repräsentative Aufnahmen wurden visuell geprüft. Alle Daten sind synthetisch.
Frühere Pakete und die normale Nutzersammlung bleiben erhalten.

Nachweise: `build/mac-fix-dev.log`, `build/mac-fix-asan.log`,
`build/mac-fix-native/native-edit-menus.log`,
`build/mac-fix-journey/` und `build/mac-fix-native/result.json`.
Letztere Datei hält die Prüfsumme der tatsächlich geprüften ausführbaren Datei fest.
Die Testlaufzeit liegt unter `build/mac-fix-runtime/`. Kein Lieferarchiv.

### Noch offene native Mac-Abnahme

Die Linux-Prüfung ersetzt keine Ausführung in Cocoa oder der Apple-Sandbox.
Auf einem Mac mit isolierten synthetischen Daten sind zusätzlich zu prüfen:

- In den vorhandenen nativen Dateidialogen funktionieren ⌘X, ⌘C, ⌘V und ⌘A im
  Suchfeld beziehungsweise in der Pfadeingabe. Der tatsächlich fokussierte
  Dialog erhält die Aktion. Hintergrundentwürfe bleiben unverändert.
- Im Bearbeitungsdialog und Exportdateiname funktionieren dieselben Aktionen
  per Menü und Tastatur. Schreibgeschützte Ansichten erlauben keine Änderungen.
- Erststart und Dateidialoge in allen sechs Systemsprachen, Englisch-Fallback
  ohne unterstützte macOS-Sprachpräferenz und Neustart nach expliziter App-Wahl.
  Eine abweichende App-Sprachwahl überschreibt die Mac-Dialogsprache nicht.
- Import, Export, bestätigter Ersatz, Abbruch, Dateinamen mit Unicode und Neustart
  mit der Sandbox-Testapp funktionieren. Fotoimport gehört ab Stufe 2 dazu.
  Restoration Journal prüft zusätzlich den PDF-Export.
- Beenden per ⌘Q, App-Menü und Fensterschließen schützt ungespeicherte Entwürfe.

Für diese Mac-Prüfung wurde kein Release-Paket erzeugt und keine Store-Abnahme
behauptet. Referenz: [Qt 6.8 zu macOS-Menüs und Internationalisierung](https://doc.qt.io/qt-6.8/macos-issues.html).

## Sammlungstransfer und gemeinsame Oberfläche, 1. November 2026

Woodworking Projects übernimmt die Verbesserungen von Board Game Shelf bis Commit `11245ea`.
Buttons sind neu angeordnet. Alle Tabellenspalten bieten auf- und absteigende
Sortierung, ohne die ausgewählte Eintrags-ID zu verlieren. Info enthält drei
getrennte Offline-Fenster mit roten Schließen-Buttons. Der öffentliche englische
Name bleibt in allen sechs Sprachen gleich. Die Systemsprache beim Erststart
mit Englisch als Fallback ist ausdrücklich bestätigt. Datenpfade, App-Kennungen,
Datenbankschema und gespeicherte Sprachwahlen bleiben erhalten.

JSON-Dateien sichern alle gespeicherten Inhalte einschließlich Originalfotos.
Ein konsistenter Lesesnapshot enthält auch die in der Tabelle absichtlich nicht
geladenen Foto-BLOBs. Import ergänzt fehlende Exemplare atomar, berücksichtigt
die Anzahl identischer Exemplare und verändert keine vorhandenen Einträge.
Neue Arbeitsschritte erhalten bei UUID-Kollisionen neue Kennungen. Reihenfolge
und Häkchen bleiben erhalten. Der Worker importiert den bestätigten Snapshot,
auch wenn die Quelldatei zwischenzeitlich geändert wird.

Die vollständigen 14 CTest-Suiten bestehen in Development, Release und mit
AddressSanitizer, LeakSanitizer und UndefinedBehaviorSanitizer. Formatter,
strikte Compilerwarnungen, clang-tidy, Ruff, Shell- und XML-Prüfung bestehen.
Die vorgebauten Qt-Bibliotheken sind selbst nicht mit Sanitizern instrumentiert.
Nach den letzten Layoutkorrekturen wurden die betroffenen UI-, Übersetzungs-
und Dialogsuiten in allen drei Konfigurationen zusätzlich erfolgreich geprüft.

Native X11-Prüfungen auf eigenen Xvfb-Bildschirmen verwenden ausschließlich
synthetische Daten. Geprüft sind alle sechs Sprachen, kleine Fenster,
Tastaturbedienung der Info-Menüs, rote Schließen-Buttons, Sortierung mit
beibehaltener Auswahl, vollständiger Sammlungstransfer und die vorhandenen
Foto-/Projektabläufe mit Neustart ohne Bildquelle.

Auch die eigenständigen Linux-Pakete mit eigener Qt-Laufzeit bestehen den
vollständigen Bedienablauf. Native GTK-Dateidialoge prüfen Export bei aktiver
Suche, Importabbruch, bestätigten Import mit Fotos, wiederholten Import ohne
zusätzliche Duplikate und Neustart. Bei Checklisten werden zusätzlich kollidierende
Schrittkennungen geprüft. Laufzeitnachweise kontrollieren die tatsächlich geladenen
Qt-, SQLite- und JPEG-Bibliotheken. Die Pakete werden ausschließlich als Ordner
mit `--stage-only` erzeugt. Vorherige Testpakete bleiben erhalten.

Die Tests simulieren fehlende Hardlinks, späte Dateikollisionen und Schreibfehler.
Neue Dateien werden dann exklusiv angelegt. Der Kopierweg ist nicht atomar und
kann bei hartem Prozessabbruch eine unvollständige neue Datei hinterlassen.
Bestehende Dateien werden ausschließlich nach Bestätigung und erneuter Prüfung
atomar ersetzt. App-Daten und Dateiverweise sind als Exportziel geschützt.

Native Windows- und macOS-Ausführung, Apple-Sandbox, Signierung und ein physischer
exFAT-Datenträger bleiben offen. Linux-Prüfungen sind keine Store-Freigabe.

Prüfbefehle und lokale Nachweise:

```bash
bash scripts/check.sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 bash scripts/check.sh asan
cmake --build --preset release --parallel 4
ctest --preset release
```

Vollständige Protokolle liegen unter `build/port-verified-dev.log`,
`build/port-verified-asan.log` und `build/port-release-check.log`.
Die abschließenden UI-Prüfungen stehen in `build/port-final-*.log`.
Native Belege und Aufnahmen liegen in `build/port-final-native/`,
`build/port-package-journey/` und `build/port-final-package-transfer/`.
Die abschließenden Testpakete liegen in `build/port-package-final/`.


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

## Farbauswahl pro Eintrag, 1. November 2026

Die optionale Farbauswahl aus Yarn Box ist in dieser App umgesetzt. Der Qt-
Farbwähler bietet Palette, eigene Farben und Transparenz. Die Auswahl wird erst
mit dem gesamten Entwurf gespeichert. Farbkachel, Hexcode, Tab-Reihenfolge,
Entfernen, Suche und Sortierung wurden geprüft.

`bash scripts/check.sh` besteht mit 15 CTest-Suiten, clang-format,
Compilerwarnungen als Fehlern und clang-tidy ohne neue Meldungen.
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 bash scripts/check.sh asan`
besteht ebenfalls mit allen 15 Suiten. Die neuen Farbtests prüfen RGBA-Bytes,
Migration echter Schema-1-Fixtures einschließlich Foto, Status und Schritten,
Rollback einer gesperrten Migration und erneuten Versuch, atomare Schreibfehler,
Altimporte ohne Farbe, Importunterschiede und Wiederholungen sowie Editor-
und Tabellenabläufe in sechs Sprachen. Alle Prüfdaten sind synthetisch.

Zusätzlich bestehen `color_tests` und `collection_features_tests` mit xcb und
GTK3 auf einem eigenen sichtbaren Xephyr-Bildschirm. Das eigenständige Linux-
Paket unter `build/manual/color-package/holzwerkprojekt-0.1.0-linux-x64/`
besteht `scripts/color_gui_smoke.py` und den vollständigen bisherigen
`scripts/gui_smoke.py`-Ablauf mit bereinigter Entwicklungsumgebung. Die neue
Prüfung öffnet den nativen Farbpicker per Tab und Leertaste, bestätigt Weiß mit
dem nativen Auswahlbutton, bricht eine weitere Auswahl ab, entfernt die Farbe
und prüft beide gespeicherten Zustände nach zwei Neustarts. Der bisherige
Ablauf prüft außerdem Fotoimport, Status, Checkliste, Fehler und Neustart ohne
Bildquelle. Die normalen Nutzerdaten werden von Tests nicht geöffnet.

Nachweise liegen unter `build/manual/colors/`, `build/manual/color-package-native/`
und `build/manual/color-package-workflow/`. Die normale Nutzersammlung wurde
anschließend mit Schema 2 erfolgreich geöffnet. Native Windows- und macOS-
Prüfungen wurden in dieser Linux-Umgebung nicht ausgeführt. Keine Store-
Veröffentlichung, kein Upload und kein Lieferarchiv.
