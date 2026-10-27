# Paketierung und spätere Store-Auslieferung

Ziel ist zuerst der Microsoft Store, später der Mac App Store. Die lokalen
Pakete sind Testlieferungen. Ein Paketformat allein belegt weder Lizenzkonformität
noch bestandene Store-Zertifizierung.

## Gemeinsame Regeln

Qt 6.8.3 wird dynamisch eingebunden. Qt Core, Gui, Widgets und Sql stammen aus
derselben SDK-Installation wie die Plattform-, Bildformat- und SQLite-Plugins. Ein Paket
enthält auch `qtbase_de.qm`. Qt Test wird nicht ausgeliefert. Die App benötigt
keine separate Qt-Installation, keinen Compiler und kein Konto.

Vor Paketierung müssen Warnungen, Formatter und Tests bestehen. Release-Builds
verwenden dieselben Quellen. Paketnamen enthalten Version, Betriebssystem und
Architektur. Vorhandene Lieferverzeichnisse und Archive werden nicht ersetzt.
Noch nicht geteilte Zwischenlieferungen können zur weiteren Prüfung separat
archiviert werden. Bereits ausgelieferte Versionen bleiben unverändert erhalten.

Die LGPL-Wahl wurde ausdrücklich bestätigt. Lizenztexte und Quellenherkunft
liegen unter `licenses`, Hinweise zusätzlich im Paket und in „Über die App“.
Die genaue Qt-Basisquelle und SHA-256 sind in der Herkunftsdatei dokumentiert. Vor Veröffentlichung
müssen passende Qt-Quellen und Bauhinweise zuverlässig angeboten werden.
Bibliotheksaustausch und Untersuchung geänderter Bibliotheken dürfen nicht durch
App-Bedingungen ausgeschlossen werden. Ein späterer Wechsel zur kommerziellen
Qt-Lizenz erfordert eine Klärung mit Qt und ist nicht automatisch zugesichert.

## Linux als Prüfplattform

```bash
source scripts/env.sh
cmake --preset release
cmake --build --preset release --parallel 4
ctest --preset release
python3 scripts/package_linux.py --qt "$HOLZWERKPROJEKT_QT_ROOT"
```

Das fertige lokale Archiv liegt unter
`build/releases/holzwerkprojekt-0.1.0-linux-x64.tar.gz`.
Der [Prüfbericht](verification.md) dokumentiert den vollständigen sichtbaren
Bedienablauf und die Herkunft der tatsächlich geladenen Bibliotheken.

Falls `libxcb-cursor.so.0` nicht systemweit installiert ist, zusätzlich
`--cursor-library /pfad/libxcb-cursor.so.0` und
`--cursor-license /pfad/zum/copyright` angeben. Das Paket übernimmt beide in
seine eigene Laufzeit. Es enthält Qt, ICU 73, QSQLITE, das JPEG-Plugin und das GTK3-Plattformthema
für native Dateidialoge. PNG ist im verwendeten Qt Gui eingebaut. GTK3 selbst
bleibt eine Systemvoraussetzung.
Übliche Ubuntu-Desktopbibliotheken wie glibc, X11, Fontconfig und OpenGL bleiben
Systemvoraussetzungen. Das ist ein Ubuntu-24.04-x64-Testpaket, kein universelles
Linux-Paket und kein geplanter Linux-Store-Release.

```bash
./build/releases/holzwerkprojekt-0.1.0-linux-x64/HolzwerkProjekt.sh --data-dir /tmp/holzwerkprojekt-pakettest
```

Die Paketierung prüft Bibliotheksauflösung und erstellt SHA-256-Prüfsummen.
Anschließend muss das entpackte Paket außerhalb des Quellverzeichnisses mit
bereinigten Qt-Umgebungsvariablen sichtbar getestet werden. Die geladenen
Qt-, ICU- und SQLite-Bibliotheken müssen aus dem Paket stammen.

## Windows x64

Vorgesehen sind MSVC 2022, Windows-SDK, CMake, Ninja und Qt 6.8.3 MSVC-x64.
Die Qt-Basis unterstützt Windows 10 ab 1809 und Windows 11. Der native
Kompatibilitätsnachweis dieser App steht noch aus.

```powershell
$env:HOLZWERKPROJEKT_QT_ROOT = 'C:\Qt\6.8.3\msvc2022_64'
.\scripts\package_windows.ps1 -Format zip
```

Das Skript baut und testet, nutzt `windeployqt`, prüft das JPEG-Plugin, übernimmt QSQLITE explizit und
legt die VC143-CRT-DLLs aus `VCToolsRedistDir` app-lokal bei. Dadurch ist kein
nachzuladender Laufzeitinstaller vorgesehen. Die Nutzungsbedingungen dieser
Microsoft-Redistributables gehören zur nativen Distributionsprüfung.

Für die spätere MSIX-Erstellung werden echte Identitätswerte aus Partner Center
übergeben. Sie sind nicht mit der internen App-Kennung gleichzusetzen:

```powershell
.\scripts\package_windows.ps1 -Format msix -IdentityName '<Partner-Center-Paketname>' -Publisher '<Partner-Center-Publisher>' -PublisherDisplayName '<Anzeigename>'
```

Die Vorlage verwendet `Windows.FullTrustApplication` und `runFullTrust` für die
Win32-Desktop-App. `makeappx.exe` stammt aus dem Windows-SDK. Eine MSI-Upgrade-ID
ist für diesen gewählten Weg nicht erforderlich. Die interne Kennung bleibt
`de.appfabrik.holzwerkprojekt`. Das Skript signiert und veröffentlicht nichts.

Vor Store-Einreichung stehen Produktregistrierung, finale Paketidentität,
Lizenzprüfung für die konkrete MSIX-Auslieferung, Metadaten, Datenschutzauskunft,
native Abnahme und Microsoft-Zertifizierung aus. Der Store signiert eingereichte
MSIX-Pakete im Veröffentlichungsprozess. Lokales Sideloading benötigt einen
eigenen Testsignierungsweg. Keine Zertifikate oder privaten Schlüssel committen.

## macOS Intel und Apple Silicon

Vorgesehen sind Xcode 15 oder neuer und das Qt-6.8.3-macOS-SDK. Deployment-Target
ist macOS 12. Für Intel und Apple Silicon wird nativ getrennt gebaut:

```bash
export HOLZWERKPROJEKT_QT_ROOT=/pfad/Qt/6.8.3/macos
bash scripts/package_macos.sh
```

Das Skript erzeugt `HolzwerkProjekt.app`, bündelt Qt über `macdeployqt`, prüft
QSQLITE, JPEG- und Cocoa-Plugin und erstellt eine DMG. Das Bundle hat die Kennung
`de.appfabrik.holzwerkprojekt` und ein eigenes Icon. Die Ad-hoc-Signatur dient
lokalen Tests und ist keine vertrauenswürdige Distributionssignatur.

`packaging/macos/app-store.entitlements` bereitet App Sandbox und
`com.apple.security.files.user-selected.read-only` vor. Die native Fotoauswahl
erlaubt einen einmaligen lesenden Import. Dauerhafter Zugriff auf die externe
Quelle und Security-Scoped Bookmarks sind für den gespeicherten BLOB nicht nötig.
Die App benötigt keinen Netzwerkzugriff. Das Entitlement muss nativ geprüft werden.
Store-Identität, Team, Provisioning und Signierung aller Bundle-Bestandteile
müssen später auf einem Mac festgelegt und geprüft werden. Keine pauschalen
Entitlements zum Abschalten von Bibliotheksprüfungen hinzufügen.

In der Sandbox liegt der von QStandardPaths gelieferte App-Pfad innerhalb des
App-Containers, typischerweise unter
`~/Library/Containers/de.appfabrik.holzwerkprojekt/Data/Library/Application Support/`.
Den vollständigen tatsächlich verwendeten Pfad mit „Über die App“ kontrollieren.
Normale Nutzerdaten nicht automatisch kopieren oder migrieren. `--data-dir`
ist bei Sandbox-Tests auf zugängliche Containerpfade beschränkt.

Für den Mac App Store bleiben insbesondere LGPL-Vertriebskonformität,
App-Sandbox, eingebettete Bibliotheken, Apple-Entwickleridentität,
Einreichungspaket und Review offen. Notarisierte Direktdistribution über eine
DMG ist ein anderer Auslieferungsweg und ersetzt diese Prüfung nicht.

## Native Abnahme

Auf jedem Zielsystem mit einem frischen Benutzerprofil und ohne Qt-SDK prüfen:
Start, Anlegen, Bearbeiten, Unicode-Suche einschließlich Schritttexten, Löschen,
Abbruchschutz, Fehlerversuch, Neustart, Skalierung und Tastenkürzel. Zusätzlich
nativen JPEG-/PNG-Import, Fotoersatz und Entfernen, Statuswechsel in beide
Richtungen, sämtliche Checklistenaktionen, reine Häkchenänderung und Verwerfen
prüfen. Nach Entfernen der Bildquelle erneut starten und Fotobytes, Status,
Schritttexte, Häkchen, UUIDs und Reihenfolge kontrollieren. Installation und Updates müssen die
Sammlung erhalten. Auch Datenpfade und Bibliotheksherkunft kontrollieren.

Die native Windows- und macOS-Abnahme steht aus. Die lokalen Skripte sind
vorbereitet, wurden hier jedoch ausschließlich auf Linux ausgeführt.
Es gibt keinen ausgeführten Store- oder Signierungsworkflow.

## Offizielle Quellen

Stand der Prüfung: 27. Oktober 2026. Vor Veröffentlichung erneut prüfen.

- [Qt 6.8 unterstützte Plattformen](https://doc.qt.io/qt-6.8/supported-platforms.html)
- [Windows-Deployment mit Qt](https://doc.qt.io/qt-6.8/windows-deployment.html)
- [macOS-Deployment mit Qt](https://doc.qt.io/qt-6.8/macos-deployment.html)
- [Qt-LGPL-Pflichten](https://www.qt.io/development/open-source-lgpl-obligations)
- [Qt-Lizenzwechsel](https://www.qt.io/faq/qt-commercial-licensing)
- [Microsoft: MSIX-Komponenten](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-manual-conversion)
- [Microsoft: Store und Signierung](https://learn.microsoft.com/en-us/windows/apps/publish/get-started)
- [Apple: Vorbereitung der Auslieferung](https://developer.apple.com/documentation/xcode/preparing-your-app-for-distribution)

Für die Sandbox-Fotoauswahl zusätzlich die offizielle
[Apple-Dokumentation zum lesenden Zugriff auf ausgewählte Dateien](https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.files.user-selected.read-only) prüfen.
Die Bilddecoder-Herkunft ist in [den Komponentenhinweisen](../THIRD_PARTY_NOTICES.md) dokumentiert.
