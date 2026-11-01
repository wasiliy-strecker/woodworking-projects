# Mac-Menüs und Sprachdeklarationen in fünf C++-Apps

Am 01.11.2026 um 21:09 lokal abgeschlossen. Auftrag: den bestätigten
Fix-Plan für Board Game Shelf, Collection Showcase, Model Building Projects,
Woodworking Projects und Restoration Journal umsetzen.

App-lokale native Bearbeiten-Menüs ergänzen Ausschneiden, Kopieren, Einfügen und
Alles auswählen. Eigene Qt-Fenster erhalten Menüs auch für modale Bearbeitung.
Native Dateidialoge verwenden die Qt-/AppKit-Weiterleitung. Sichtbare, aktivierte
Textfelder und schreibgeschützte Inhalte werden berücksichtigt.

Alle sechs Mac-Sprachen sind deklariert, mit Englisch als Fallback.
Native Mac-Dateidialoge folgen wie bestätigt der macOS-Sprachwahl. Die gespeicherte
App-Sprachwahl bleibt unabhängig. Keine gemeinsamen Runtime-Pakete, Änderungen
an App-Kennungen, Datenpfaden, Nutzerdaten oder Exportformaten.

Menü- und Sprachkonfigurationstests ergänzt. In Board Game Shelf außerdem den
veralteten GUI-Testklick auf Verwerfen und Löschen an die bestehende Buttonfolge
angepasst. Die Produktfunktion wurde dafür nicht geändert.

| App | Development-Suiten | Sanitizer-Suiten | Checkpoint |
| --- | ---: | ---: | --- |
| Board Game Shelf | 11 | 11 | `7ea8bfe` |
| Collection Showcase | 14 | 14 | `191b1c9` |
| Model Building Projects | 16 | 16 | `990eb53` |
| Woodworking Projects | 17 | 17 | `de52292` |
| Restoration Journal | 19 | 19 | `077f5ed` |

Alle 77 Suiten bestehen jeweils in Development und mit ASan/LSan/UBSan.
Formatter, Compilerwarnungen, clang-tidy und Python-Prüfungen bestehen.
Zusätzlich bestehen native X11-Menütests und vollständige Bedienabläufe mit
aktuellen Development-Dateien in separaten Qt-Testlaufzeiten. Fotos, Status,
Checklisten und PDF werden entsprechend dem jeweiligen App-Umfang geprüft.
Repräsentative App-Aufnahmen und PDF-Seiten wurden visuell geprüft.

Prüflogs liegen jeweils unter `build/mac-fix-dev.log`, `build/mac-fix-asan.log`,
`build/mac-fix-native/` und `build/mac-fix-journey/`. Board Game Shelf verwendet
für den erfolgreichen Bedienablauf `build/mac-fix-journey-retry/`.
Die Dokumentation enthält die noch erforderlichen Cocoa- und Sandbox-Abnahmen.
Native macOS-Ausführung wurde in der Linux-Umgebung nicht behauptet.

Je App lokaler Checkpoint und Abschlusscommit. Keine Store-Assets, Mac-Release-
Pakete, Lieferarchive, Uploads, Veröffentlichung oder Push. Frühere Testpakete
und normale Nutzerdaten bleiben erhalten.
