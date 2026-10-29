# HolzwerkProjekt

Eigenständige C++20-App mit Qt 6.8.3 Widgets, CMake und SQLite über QSQLITE.
Die Workspace-Regeln gelten ergänzend. Ausschließlich diese App bearbeiten.
Stufe 4 umfasst vier Textfelder, ein optionales Foto, drei Statuswerte und eine
Checkliste. Keine PDF- oder weiteren Hauptfunktionen ergänzen.

## Architektur und Daten

`domain` enthält Werte und Validierung, `application` Service, Repository-Vertrag
und Fotoimport, `persistence` ausschließlich SQL, `ui` Qt Widgets.
Ein einzelner Worker besitzt seine SQL-Verbindung. SQL, Fotoimport und Decoder
laufen außerhalb des GUI-Threads. Widgets gehören ausschließlich zum GUI-Thread.
Eigene Kennung `de.appfabrik.holzwerkprojekt` und Datenpfade beibehalten.

Texte, Fotobytes, Status und Schritte bilden einen Entwurf und eine Transaktion.
Projekt-IDs und Schritt-UUIDs sind stabil. Beim Entfernen bleiben Positionslücken.
Statusschlüssel ausdrücklich zuordnen. Unbekannte Werte niemals ersetzen.
Fremdschlüssel auf jeder Verbindung aktivieren. Fehler erhalten alle Entwürfe.
Sämtliche Abbruchwege schützen Änderungen mit Weiterbearbeiten als Vorauswahl.

Fotos nach Inhalt prüfen. Grenzen sind 10 MiB, 6000 Pixel je Seite und
16 Millionen Pixel. Ausschließlich lesender Import. Keine Originaldatei ändern.
Tests öffnen nur temporäre Datenbanken und verwenden synthetische Bilder.

## Prüfen

```bash
source scripts/env.sh
find src tests -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 clang-format -i
bash scripts/check.sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 bash scripts/check.sh asan
```

Höchstens vier parallele Build-Jobs. `.tools/env.sh` ist optional und ignoriert.
Sichtbare GUI-Abläufe auf DISPLAY=:0 über die gesamte Laufzeit mit
`flock /tmp/app-factory-cpp-visible-ui.lock` schützen. Keine fremden Fenster
bedienen oder Prozesse beenden. Roadmap-Änderungen unter der separaten Sperre
`/tmp/app-factory-cpp-roadmap.lock` frisch einlesen und nur gezielt ergänzen.

Vor Commit Formatter, Compilerwarnungen, statische Prüfung, Tests und erforderliche
sichtbare Abnahme ausführen. Paketprüfung einschließlich Foto, Status, Checkliste,
SQLite und Neustart ohne Bildquelle. Workspace-Changelog ergänzen. Nur eigene
Dateien app-lokal committen. Kein Root-Repository, Push oder Veröffentlichung.
Zuerst Microsoft Store, später Mac App Store. Native Abnahmen, Signierung und
konkrete Lizenzkonformität der späteren Store-Auslieferung bleiben gesondert.

## Internationalisierung

Sechs App-Kataloge und Qt-Basisübersetzungen für de-DE, en-US, es-ES, fr-FR,
it-IT und pt-BR. Neue sichtbare Texte vollständig übersetzen. Sprachwahl gilt
ab Neustart. Fachliche Daten, UUIDs, Positionen und Statuscodes bleiben sprachneutral.
`docs/internationalization.md` beschreibt Paketprüfung und Ordnerlieferung.
