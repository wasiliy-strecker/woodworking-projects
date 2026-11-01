# Woodworking Projects: Linux-Icons und Starter

Das vorhandene Motiv ist jetzt als Benutzer-Desktop-Eintrag mit passender
Qt-Desktop-Kennung, Fensterklasse und Hicolor-Icons eingebunden. Sechs zusätzliche
PNG-Größen ergänzen die vorhandenen 256- und 1024-Pixel-Dateien und das SVG.
Der schlanke Dev-Starter startet den gebauten Stand ohne erneuten Build.

Der app-lokale Installer unterstützt Entwicklungsverzeichnis und Linux-Paket.
Wiederholte Installation aktualisiert eigene Einträge. Fremde Einträge und
Symlinks bleiben geschützt. Paketpfade mit Leerzeichen, Dollar- und Prozentzeichen
wurden geprüft. Das Linux-Paket enthält Installer und Icons. Bestehende
Lieferartefakte, technische Kennungen, Sammlungen und Motive bleiben erhalten.

Auf ausdrücklichen Nutzerwunsch nur gezielte Prüfungen: Dev- und Release-App
gebaut, C++-Formatter und clang-tidy auf main.cpp, Python-/Shell-Syntax,
Desktop-Dateivalidierung und Installerprüfung in temporären Verzeichnissen.
Dev-App und frisches eigenständiges Linux-Paket über Desktop-Einträge mit
isolierten Daten gestartet. Fenstericon mit 256 × 256 Pixeln, WM_CLASS und
GTK-Anwendungskennung bestätigt. Der Desktop findet das installierte Icon.
Keine vollständigen Fachtests, Sanitizer oder CRUD-Durchläufe ausgeführt.
Die Dock-Sichtprüfung war wegen des gesperrten Desktops nicht möglich.

Der normale Dev-Menüeintrag ist im Benutzerkonto installiert. Anleitung in
README ergänzt. Lokaler Checkpoint und Abschlusscommit. Kein Push.
