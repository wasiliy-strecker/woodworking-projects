# Komponenten und Lizenzhinweise

HolzwerkProjekt verwendet Qt 6.8.3 unter der GNU Lesser General Public License,
Version 3 (LGPL-3.0). Qt Core, Gui, Widgets und Sql werden dynamisch eingebunden.
Der QSQLITE-Treiber wird als dynamisches Plugin ausgeliefert. Qt Test wird nur
für Entwicklungsprüfungen verwendet.

Qt ist Copyright The Qt Company Ltd. und weiterer Beitragender.
Lizenzinformationen: https://www.qt.io/development/open-source-lgpl-obligations
Quellcode der eingesetzten Qt-Basis:
https://download.qt.io/archive/qt/6.8/6.8.3/submodules/qtbase-everywhere-src-6.8.3.tar.xz

Die Dateien im Ordner licenses enthalten die Lizenztexte und Hinweise zu
mitgelieferten Fremdkomponenten. Zur Veröffentlichung müssen der zu den
ausgelieferten Bibliotheken gehörende Quellcode und die nötigen Bauhinweise
verfügbar gemacht werden. Das lokale Testpaket ist noch keine Store-Freigabe.

Die Rechte zum Ändern, Ersetzen und erneuten Einbinden der LGPL-Bibliotheken
sowie zur Untersuchung solcher Änderungen werden nicht eingeschränkt.
Eine spätere zusätzliche Nutzungsvereinbarung darf diese Rechte nicht aufheben.

SQLite ist gemeinfrei. Weitere Komponenten innerhalb von Qt, beispielsweise
Unicode-Daten, PCRE2, HarfBuzz, FreeType, libpng und zlib, haben eigene Hinweise.
Die Paketierung übernimmt die mit dem konkreten SDK ausgelieferten Metadaten
und dokumentiert zusätzlich gebündelte Laufzeitbibliotheken.

Eigener App-Code: Copyright 2026 Wasiliy Strecker. Eine gesonderte öffentliche
Quellcodelizenz ist bisher nicht festgelegt. Diese Aussage ändert keine Rechte
an Qt oder anderen Fremdkomponenten.

Foto-Unterstützung: Das JPEG-Plugin des verwendeten Qt-6.8.3-SDKs enthält
libjpeg-turbo 3.1.0 unter IJG und BSD-3-Clause. Die PNG-Unterstützung in Qt Gui
verwendet libpng 1.6.47 unter Libpng und libpng-2.0. Die unveränderten
Lizenztexte und Zuordnungen liegen unter licenses/qtbase-6.8.3/src/3rdparty.
Die Anwendung bindet keine zusätzliche Bildbibliothek direkt ein.

Das Linux-Prüfpaket bündelt außerdem das Qt-GTK3-Plattformthema für native
Dateidialoge. GTK3 selbst und die üblichen Desktopbibliotheken bleiben
Systemvoraussetzungen. Der vollständige Komponentenbestand des konkreten SDKs
liegt im Paket unter licenses/qt-sdk-sbom.

Offizielle Quellen für den tatsächlich verwendeten Patchstand:
https://github.com/qt/qtbase/blob/v6.8.3/src/3rdparty/libjpeg/qt_attribution.json
https://github.com/qt/qtbase/blob/v6.8.3/src/3rdparty/libpng/qt_attribution.json
