#include "i18n/languages.h"
#include "persistence/language_preferences.h"
#include "persistence/sqlite_project_repository.h"
#include "ui/language_dialog.h"
#include "ui/main_window.h"

#include <QAction>
#include <QBuffer>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

using namespace woodwork;

namespace {
QString text(const char* source) { return QCoreApplication::translate("App", source); }

template <typename Widget> Widget* child(QWidget& parent, const char* name) {
  auto* widget = parent.findChild<Widget*>(QString::fromLatin1(name));
  Q_ASSERT(widget);
  return widget;
}

void languages() {
  QTest::addColumn<QString>("tag");
  for (const auto& tag : supportedLanguages()) {
    QTest::newRow(qPrintable(tag)) << tag;
  }
}
} // namespace

class I18nTest final : public QObject {
  Q_OBJECT
private slots:
  void languageResolution_data() {
    QTest::addColumn<QString>("preference");
    QTest::addColumn<QString>("system");
    QTest::addColumn<QString>("expected");
    const QStringList systems{QStringLiteral("de-AT"), QStringLiteral("en-GB"),
                              QStringLiteral("es-MX"), QStringLiteral("fr-CA"),
                              QStringLiteral("it-CH"), QStringLiteral("pt-PT")};
    const auto tags = supportedLanguages();
    for (qsizetype i = 0; i < tags.size(); ++i) {
      QTest::newRow(qPrintable(tags[i])) << QStringLiteral("auto") << systems[i] << tags[i];
    }
    QTest::newRow("unsupported-system")
        << QStringLiteral("auto") << QStringLiteral("ja-JP") << QStringLiteral("en-US");
    QTest::newRow("unknown-preference")
        << QStringLiteral("xx-XX") << QStringLiteral("de-DE") << QStringLiteral("en-US");
    QTest::newRow("explicit-preference")
        << QStringLiteral("fr-FR") << QStringLiteral("de-DE") << QStringLiteral("fr-FR");
  }
  void languageResolution() {
    QFETCH(QString, preference);
    QFETCH(QString, system);
    QFETCH(QString, expected);
    QCOMPARE(languageFor(preference, QLocale(system)), expected);
  }

  void uiAndErrors_data() { languages(); }
  void uiAndErrors() {
    QFETCH(QString, tag);
    Translations translations;
    translations.install(tag);
    QCOMPARE(QLocale().name(), QLocale(tag).name());
    const QStringList saves{QStringLiteral("Speichern"), QStringLiteral("Save"),
                            QStringLiteral("Guardar"),   QStringLiteral("Enregistrer"),
                            QStringLiteral("Salva"),     QStringLiteral("Salvar")};
    QCOMPARE(text("Speichern"), saves[supportedLanguages().indexOf(tag)]);
    QTemporaryDir directory;
    StorageController controller(
        [&directory] { return std::make_unique<SqliteProjectRepository>(directory.path()); });
    QSignalSpy loaded(&controller, &StorageController::loaded);
    MainWindow window(controller, directory.path(), tag);
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }));
    QCOMPARE(child<QPushButton>(window, "addProject")->text(), text("Projekt hinzufügen"));
    QCOMPARE(child<QLabel>(window, "collectionState")->text(),
             text("Deine Projektliste ist noch leer. Füge dein erstes Holzwerkprojekt hinzu."));
    ProjectEditor editor(controller, std::nullopt, &window);
    editor.show();
    QCOMPARE(child<QLabel>(editor, "editorPhoto")->accessibleName(), text("Projektfoto"));
    auto* name = child<QLineEdit>(editor, "projectName");
    auto* note = child<QPlainTextEdit>(editor, "projectNote");
    note->setPlainText(QStringLiteral("Étoile, ação, Grüße\n第二行"));
    editor.save();
    QCOMPARE(child<QLabel>(editor, "editorError")->text(),
             text("Bitte gib einen Projektnamen ein."));
    QCOMPARE(note->toPlainText(), QStringLiteral("Étoile, ação, Grüße\n第二行"));
    name->setText(QStringLiteral("Étoile 東京"));
    name->selectAll();
    const std::unique_ptr<QMenu> menu(name->createStandardContextMenu());
    const QStringList copyLabels{QStringLiteral("Kopieren"), QStringLiteral("Copy"),
                                 QStringLiteral("Copiar"),   QStringLiteral("Copier"),
                                 QStringLiteral("Copia"),    QStringLiteral("Copiar")};
    const auto copyLabel = copyLabels[supportedLanguages().indexOf(tag)];
    bool translatedCopy = false;
    for (const auto* action : menu->actions()) {
      translatedCopy |= action->text().remove('&').section('\t', 0, 0) == copyLabel;
    }
    QVERIFY2(translatedCopy, qPrintable(tag));
    QSignalSpy saved(&controller, &StorageController::saved);
    editor.save();
    QVERIFY(QTest::qWaitFor([&] { return saved.size() == 1; }));
    QCOMPARE(child<QTableWidget>(window, "projectsTable")->rowCount(), 1);
    QCOMPARE(child<QTableWidget>(window, "projectsTable")->item(0, 0)->text(),
             QStringLiteral("Étoile 東京"));
    QTemporaryDir corrupt;
    QFile invalid(QDir(corrupt.path()).filePath(QStringLiteral("holzwerkprojekt.sqlite3")));
    QVERIFY(invalid.open(QIODevice::WriteOnly));
    invalid.write("not a database");
    invalid.close();
    SqliteProjectRepository broken(corrupt.path());
    try {
      broken.initialize();
      QFAIL("A corrupt database must fail");
    } catch (const StorageError& error) {
      QCOMPARE(QString::fromUtf8(error.what()),
               text("Die Datenbank ist beschädigt oder keine gültige SQLite-Datei. Sie wird nicht "
                    "ersetzt. Sichere die Datei vor einer Reparatur."));
    }
  }

  void localizedStatusPreservesCodes_data() { languages(); }
  void localizedStatusPreservesCodes() {
    QFETCH(QString, tag);
    Translations translations;
    translations.install(tag);
    QTemporaryDir directory;
    SqliteProjectRepository repository(directory.path());
    repository.initialize();
    ProjectDraft draft{QStringLiteral("Étoile"), {}, {}, {}, ProjectStatus::Planned, {}};
    const auto created = repository.create(draft);
    for (const auto status :
         {ProjectStatus::Planned, ProjectStatus::Started, ProjectStatus::Finished}) {
      draft.status = status;
      const auto saved = repository.update(created.id, draft);
      QCOMPARE(saved.fields.status, status);
      QCOMPARE(statusFromStorage(storedStatus(status)), status);
      QCOMPARE(repository.find(created.id), draft);
    }
    const QStringList planned{QStringLiteral("Geplant"),     QStringLiteral("Planned"),
                              QStringLiteral("Planificado"), QStringLiteral("Prévu"),
                              QStringLiteral("Pianificato"), QStringLiteral("Planejado")};
    QCOMPARE(displayStatus(ProjectStatus::Planned), planned[supportedLanguages().indexOf(tag)]);
    StorageController controller(
        [&directory] { return std::make_unique<SqliteProjectRepository>(directory.path()); });
    ProjectEditor editor(controller, std::nullopt);
    const auto* combo = child<QComboBox>(editor, "projectStatus");
    for (int i = 0; i < combo->count(); ++i) {
      const auto status = combo->itemData(i).value<ProjectStatus>();
      QCOMPARE(combo->itemText(i), displayStatus(status));
    }
  }

  void preferencePersistence_data() { languages(); }
  void preferencePersistence() {
    QFETCH(QString, tag);
    QTemporaryDir directory;
    QCOMPARE(readLanguagePreference(directory.path()).value, QStringLiteral("auto"));
    saveLanguagePreference(directory.path(), tag);
    const auto result = readLanguagePreference(directory.path());
    QCOMPARE(result.value, tag);
    QVERIFY(!result.readFailed);
    QVERIFY_EXCEPTION_THROWN(saveLanguagePreference(directory.path(), QStringLiteral("invalid")),
                             StorageError);
    QCOMPARE(readLanguagePreference(directory.path()).value, tag);
    Translations translations;
    translations.install(result.value, QLocale(QStringLiteral("ja-JP")));
    QCOMPARE(QLocale().name(), QLocale(tag).name());
    saveLanguagePreference(directory.path(), QStringLiteral("auto"));
    QCOMPARE(readLanguagePreference(directory.path()).value, QStringLiteral("auto"));
  }

  void languageChangePreservesPhotos_data() { languages(); }
  void languageChangePreservesPhotos() {
    QFETCH(QString, tag);
    Translations translations;
    translations.install(tag);
    QTemporaryDir directory;
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(QColor(73, 121, 133));
    QByteArray bytes;
    QBuffer buffer(&bytes);
    QVERIFY(buffer.open(QIODevice::WriteOnly));
    QVERIFY(image.save(&buffer, "PNG"));
    ProjectDraft draft{QStringLiteral("Étoile – ação"), QStringLiteral("Atelier Mira"),
                       QStringLiteral("Vitrine 2"),     QStringLiteral("Grüße\nSão Paulo"),
                       ProjectStatus::Started,          bytes};
    draft.steps = {{QUuid::createUuid(), QStringLiteral("Maße prüfen – ação"), true, 0},
                   {QUuid::createUuid(), QStringLiteral("Maße prüfen – ação"), false, 2}};
    qint64 id = 0;
    {
      SqliteProjectRepository repository(directory.path());
      repository.initialize();
      id = repository.create(draft).id;
    }
    QFile database(QDir(directory.path()).filePath(QStringLiteral("holzwerkprojekt.sqlite3")));
    QVERIFY(database.open(QIODevice::ReadOnly));
    const auto before = database.readAll();
    database.close();
    saveLanguagePreference(directory.path(), tag);
    QVERIFY(database.open(QIODevice::ReadOnly));
    QCOMPARE(database.readAll(), before);
    database.close();
    {
      SqliteProjectRepository reopened(directory.path());
      reopened.initialize();
      QCOMPARE(reopened.find(id), draft);
    }
    StorageController controller(
        [&directory] { return std::make_unique<SqliteProjectRepository>(directory.path()); });
    QSignalSpy loaded(&controller, &StorageController::loaded);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }));
    ProjectEditor editor(controller, WoodProject{id, draft, true}, nullptr, image);
    editor.show();
    QFile invalid(QDir(directory.path()).filePath(QStringLiteral("invalid.png")));
    QVERIFY(invalid.open(QIODevice::WriteOnly));
    invalid.write("synthetic non-image");
    invalid.close();
    QSignalSpy failed(&controller, &StorageController::failed);
    editor.importFromPath(invalid.fileName());
    QVERIFY(QTest::qWaitFor([&] { return failed.size() == 1; }));
    QCOMPARE(child<QLabel>(editor, "editorError")->text(),
             text("Bitte wähle ein gültiges JPEG- oder PNG-Foto. Der Dateiinhalt wird geprüft."));
    QCOMPARE(editor.draft(), draft);
    QCOMPARE(child<QPushButton>(editor, "choosePhoto")->text(), text("&Foto ersetzen …"));
    QCOMPARE(child<QPushButton>(editor, "removePhoto")->text(), text("Foto &entfernen"));
  }

  void invalidPreferencesAreReported() {
    QTemporaryDir directory;
    const auto path = QDir(directory.path()).filePath(QStringLiteral("language.txt"));
    for (const auto& value : {QByteArray("xx-XX"), QByteArray(1024, 'x'), QByteArray()}) {
      QFile file(path);
      QVERIFY(file.open(QIODevice::WriteOnly));
      file.write(value);
      file.close();
      const auto result = readLanguagePreference(directory.path());
      QVERIFY(result.readFailed);
      QCOMPARE(result.value, QStringLiteral("en-US"));
    }
    QVERIFY(QFile::remove(path));
    QVERIFY(QDir().mkdir(path));
    QVERIFY(readLanguagePreference(directory.path()).readFailed);
    QVERIFY_EXCEPTION_THROWN(saveLanguagePreference(directory.path(), QStringLiteral("fr-FR")),
                             StorageError);
    QVERIFY(QFileInfo(path).isDir());
  }

  void failedLanguageSaveKeepsDialogAndRetries_data() { languages(); }
  void failedLanguageSaveKeepsDialogAndRetries() {
    QFETCH(QString, tag);
    Translations translations;
    translations.install(tag);
    QTemporaryDir directory;
    const auto path = QDir(directory.path()).filePath(QStringLiteral("language.txt"));
    QVERIFY(QDir().mkdir(path));
    StorageController controller(
        [&directory] { return std::make_unique<SqliteProjectRepository>(directory.path()); });
    LanguageDialog dialog(controller, directory.path(), QStringLiteral("auto"));
    dialog.show();
    auto* combo = child<QComboBox>(dialog, "languageChoice");
    combo->setCurrentIndex(combo->findData(tag));
    QSignalSpy failed(&controller, &StorageController::failed);
    QSignalSpy saved(&controller, &StorageController::languageSaved);
    child<QPushButton>(dialog, "saveLanguage")->click();
    dialog.reject();
    QVERIFY(dialog.isVisible());
    QVERIFY(QTest::qWaitFor([&] { return failed.size() == 1; }));
    QVERIFY(dialog.isVisible());
    QCOMPARE(combo->currentData().toString(), tag);
    QCOMPARE(child<QLabel>(dialog, "languageError")->text(),
             text("Die Sprache konnte nicht gespeichert werden. Prüfe das Datenverzeichnis und "
                  "deine Zugriffsrechte. Die bisherige Einstellung bleibt erhalten."));
    QCOMPARE(saved.size(), 0);
    QVERIFY(QDir().rmdir(path));
    child<QPushButton>(dialog, "saveLanguage")->click();
    QVERIFY(QTest::qWaitFor([&] { return saved.size() == 1; }));
    QVERIFY(!dialog.isVisible());
    QCOMPARE(readLanguagePreference(directory.path()).value, tag);
  }

  void regionalNumbersAndSorting_data() { languages(); }
  void regionalNumbersAndSorting() {
    QFETCH(QString, tag);
    Translations translations;
    translations.install(tag);
    const QStringList numbers{QStringLiteral("12.345"), QStringLiteral("12,345"),
                              QStringLiteral("12.345"), QStringLiteral("12\u202f345"),
                              QStringLiteral("12.345"), QStringLiteral("12.345")};
    QCOMPARE(QLocale().toString(12345), numbers[supportedLanguages().indexOf(tag)]);
    const ProjectList projects{{1, {QStringLiteral("Jeu 10"), {}, {}, {}, {}}},
                               {2, {QStringLiteral("Jeu 2"), {}, {}, {}, {}}},
                               {3, {QStringLiteral("Étoile"), {}, {}, {}, {}}},
                               {4, {QStringLiteral("Zebra"), {}, {}, {}, {}}}};
    const auto ordered = searchProjects(projects, {});
    QCOMPARE(ordered[0].id, 3);
    QCOMPARE(ordered[1].id, 2);
    QCOMPARE(ordered[2].id, 1);
    QCOMPARE(ordered[3].id, 4);
  }

  void smallDialogsKeepActionsReachable_data() { languages(); }
  void smallDialogsKeepActionsReachable() {
    QFETCH(QString, tag);
    Translations translations;
    translations.install(tag);
    const auto originalFont = QApplication::font();
    const auto restoreFont = qScopeGuard([originalFont] { QApplication::setFont(originalFont); });
    auto font = originalFont;
    font.setPointSize(18);
    QApplication::setFont(font);
    QTemporaryDir directory;
    StorageController controller(
        [&directory] { return std::make_unique<SqliteProjectRepository>(directory.path()); });
    ProjectEditor editor(controller, std::nullopt);
    editor.resize(400, 340);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    for (const auto* name : {"saveProject", "cancelEdit"}) {
      const auto* button = child<QPushButton>(editor, name);
      QVERIFY(editor.rect().contains(QRect(button->mapTo(&editor, QPoint()), button->size())));
      QVERIFY(button->width() >= button->fontMetrics().horizontalAdvance(button->text()));
    }
    LanguageDialog language(controller, directory.path(), tag);
    language.resize(400, 250);
    language.show();
    QVERIFY(QTest::qWaitForWindowExposed(&language));
    for (const auto* name : {"saveLanguage", "cancelLanguage"}) {
      const auto* button = child<QPushButton>(language, name);
      QVERIFY(language.rect().contains(QRect(button->mapTo(&language, QPoint()), button->size())));
      QVERIFY(button->width() >= button->fontMetrics().horizontalAdvance(button->text()));
    }
  }
};

QTEST_MAIN(I18nTest)
#include "i18n_test.moc"
