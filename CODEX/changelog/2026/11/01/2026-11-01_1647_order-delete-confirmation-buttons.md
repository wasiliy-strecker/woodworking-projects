# Woodworking Projects: Reihenfolge bei Löschen und Verwerfen

Die Löschbestätigung zeigt Abbrechen links und Löschen rechts. Die Rückfrage
zu ungespeicherten Änderungen zeigt Weiterbearbeiten links und Verwerfen rechts.
Die Anordnung wird nur für diese beiden Dialoge festgelegt. Die Buttons behalten
ihre Rollen. Abbrechen beziehungsweise Weiterbearbeiten bleiben Vorauswahl
und Escape-Aktion. Texte und Übersetzungen bleiben unverändert. Die Qt-Dialoge
verwenden auf allen Plattformen dieselbe Anordnung.

Prüfung: `bash scripts/check.sh` mit Formatter, strengen Compilerwarnungen,
14 erfolgreichen CTest-Suiten, Katalogprüfung, clang-tidy und Whitespace-Prüfung.
Zusätzlich die echten Bestätigungsfunktionen auf einem isolierten X11-Display
in allen sechs Sprachen geöffnet. Je Dialog bestanden 30 Prüfungen für
Button-Geometrie, Abbrechen beziehungsweise Weiterbearbeiten, Enter, Escape,
Fensterschließen und bestätigtes Löschen beziehungsweise Verwerfen.
Bilder und Protokolle liegen lokal unter `build/delete-button-order-review/`
und `build/discard-button-order-review/`. Die Dialogbilder wurden visuell
kontrolliert. Keine Nutzerdaten für die Dialogprüfungen verwendet.
Windows und macOS wurden nicht nativ ausgeführt.

Manuell wiederholbar: App mit isoliertem `--data-dir` starten, einen Testeintrag
anlegen und Löschen öffnen. Abbrechen muss links von Löschen stehen. Abbrechen,
Enter, Escape und Fensterschließen erhalten den Eintrag. Erst der Klick auf
Löschen entfernt ihn. Im Editor anschließend einen Entwurf ändern und abbrechen.
Weiterbearbeiten muss links von Verwerfen stehen. Nur Verwerfen verwirft den
Entwurf. Die übrigen Abbruchwege behalten ihn bei.
