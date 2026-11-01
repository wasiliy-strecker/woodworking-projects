# Add collection transfer and common interface improvements

Woodworking Projects übernimmt Layout und Sortierung, JSON-Export/-Import einschließlich
vorhandener Zusatzdaten, getrennte Info-/Datenschutz-/Lizenzfenster und rote
Schließen-Buttons. Englischer öffentlicher Name in allen sechs Sprachen.
Systemsprache beim Erststart mit Englisch als Fallback auf Nutzerauftrag.
Technische Kennungen, Datenpfade und vorhandene Daten bleiben erhalten.

Mac-Dateizugriff für ausgewählte Dateien und Ordner. Portable neue Exporte ohne
Hardlinks, Schutz bestehender Dateien und atomarer ergänzender Import.

Geprüft mit Formatter, strikten Compilerwarnungen, clang-tidy, 14 CTest-Suiten
in Development, Release und unter Sanitizern. Native Prüfungen in sechs Sprachen,
Sammlungstransfer und vollständiger Bedienablauf der eigenständigen Linux-Pakete.
Windows-/macOS-Sandbox und physisches exFAT bleiben nativ ungeprüft.

Checkpoint vor Umsetzung. Abschlusscommit nach erfolgreicher Prüfung.
Kein Push, keine Veröffentlichung und keine Nutzerdaten in Tests oder Commits.
