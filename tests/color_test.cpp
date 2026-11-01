#include "application/collection_codec.h"
#include "application/colors.h"
#include "i18n/languages.h"
#include "persistence/sqlite_project_repository.h"
#include "ui/main_window.h"
#include "ui/project_editor.h"
#include <QBuffer>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
using namespace woodwork;
namespace {
using Draft = ProjectDraft;
using Repository = SqliteProjectRepository;
using Editor = ProjectEditor;
Draft example() {
  Draft draft;
  draft.name = QStringLiteral("Color sample");
  draft.material = QStringLiteral("Material");
  draft.location = QStringLiteral("Shelf 2");
  draft.note = QStringLiteral("Saved note\nSecond line");
  QImage photo(20, 10, QImage::Format_RGB32);
  photo.fill(Qt::blue);
  QBuffer buffer(&draft.photo);
  if (!buffer.open(QIODevice::WriteOnly) || !photo.save(&buffer, "PNG"))
    throw std::runtime_error("Cannot create test photo");
  draft.status = ProjectStatus::Started;
  draft.steps = {{QUuid(QStringLiteral("12345678-1234-4234-8234-123456789abc")),
                  QStringLiteral("Cut"), true, 7}};
  return draft;
}
Draft read(Repository& repository, qint64 id) { return repository.find(id); }
void capture(QWidget& widget, const QString& name) {
  const auto folder = qEnvironmentVariable("CPP_COLOR_SCREENSHOTS");
  if (folder.isEmpty())
    return;
  QDir().mkpath(folder);
  QTest::qWait(80);
  const auto pixels = qobject_cast<QColorDialog*>(&widget) &&
                              QGuiApplication::platformName() == QStringLiteral("xcb")
                          ? widget.screen()->grabWindow(0)
                          : widget.grab();
  if (!pixels.save(QDir(folder).filePath(name + QStringLiteral(".png"))))
    throw std::runtime_error("Screenshot failed");
}
void choose(ColorField& field, const QColor& color, bool accept, const QString& screenshot = {}) {
  bool dialogSeen = false;
  QTimer::singleShot(0, &field, [&] {
    auto* dialog = qobject_cast<QColorDialog*>(QApplication::activeModalWidget());
    if (!dialog)
      return;
    dialogSeen = true;
    dialog->setCurrentColor(color);
    if (!screenshot.isEmpty())
      capture(*dialog, screenshot);
    // Click the actual translated dialog button, retaining the dialog's accept/reject logic.
    auto* buttons = dialog->findChild<QDialogButtonBox*>();
    if (buttons) {
      QTest::mouseClick(buttons->button(accept ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel),
                        Qt::LeftButton);
    } else if (accept) {
      dialog->accept();
    } else {
      dialog->reject();
    }
  });
  field.findChild<QPushButton*>(QStringLiteral("chooseColor"))->click();
  if (!dialogSeen)
    throw std::runtime_error("No color dialog");
}
class ExternalDatabase {
public:
  explicit ExternalDatabase(const QString& path) {
    name_ = QUuid::createUuid().toString();
    database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name_);
    database.setDatabaseName(path);
    if (!database.open())
      throw std::runtime_error("Cannot open fixture database");
  }
  ~ExternalDatabase() {
    database.close();
    database = {};
    QSqlDatabase::removeDatabase(name_);
  }
  void execute(const QString& sql) {
    QSqlQuery query(database);
    if (!query.exec(sql))
      throw std::runtime_error(query.lastError().text().toStdString());
  }
  QSqlDatabase database;

private:
  QString name_;
};
} // namespace
class ColorTest final : public QObject {
  Q_OBJECT
private slots:
  void rgbaEncodingAndSwatches() {
    QCOMPARE(encodeColor(QColor(18, 52, 86)), QStringLiteral("#123456"));
    QCOMPARE(encodeColor(QColor(18, 52, 86, 128)), QStringLiteral("#12345680"));
    QCOMPARE(parseColor(QStringLiteral("#12345680")), QColor(18, 52, 86, 128));
    QCOMPARE(parseColor(QStringLiteral("#12345600")), QColor(18, 52, 86, 0));
    QVERIFY(!parseColor(QStringLiteral("red")).isValid());
    QVERIFY(!parseColor(QStringLiteral("#123456\n")).isValid());
    auto draft = example();
    for (const auto& invalid : {QStringLiteral("red"), QStringLiteral("#12345"),
                                QStringLiteral("#1234567"), QStringLiteral("#123456\n")}) {
      draft.color = invalid;
      QVERIFY_EXCEPTION_THROWN(validateDraft(draft), ValidationError);
    }
    const auto transparent = colorSwatch(QStringLiteral("#FF000000"));
    QVERIFY(transparent.pixelColor(2, 2) != transparent.pixelColor(6, 2));
    const auto opaque = colorSwatch(QStringLiteral("#123456"));
    QCOMPARE(opaque.pixelColor(2, 2), QColor(18, 52, 86));
    QCOMPARE(opaque.pixelColor(6, 2), QColor(18, 52, 86));
  }
  void migrationPreservesExistingContentsAndIds() {
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("holzwerkprojekt.sqlite3"));
    const auto original = example();
    {
      ExternalDatabase legacy(path);
      QFile schema(QFINDTESTDATA("fixtures/legacy_color_schema.sql"));
      QVERIFY(schema.open(QIODevice::ReadOnly));
      for (const auto& sql :
           QString::fromUtf8(schema.readAll()).split(QChar(';'), Qt::SkipEmptyParts))
        if (!sql.trimmed().isEmpty())
          legacy.execute(sql);
      QSqlQuery insert(legacy.database);
      QVERIFY(insert.prepare(
          QStringLiteral("INSERT INTO projects(id,name,material,location,note,photo,status) "
                         "VALUES(42,:name,:details,:location,:note,:photo,'started')")));
      insert.bindValue(QStringLiteral(":name"), original.name);
      insert.bindValue(QStringLiteral(":details"), original.material);
      insert.bindValue(QStringLiteral(":location"), original.location);
      insert.bindValue(QStringLiteral(":note"), original.note);
      insert.bindValue(QStringLiteral(":photo"), original.photo);
      QVERIFY(insert.exec());
      legacy.execute(QStringLiteral("INSERT INTO checklist_steps "
                                    "VALUES('12345678-1234-4234-8234-123456789abc',42,'Cut',1,7)"));
    }
    {
      Repository repository(directory.path());
      repository.initialize();
      QCOMPARE(read(repository, 42), original);
      QCOMPARE(repository.findAll().first().id, 42);
      auto colored = original;
      colored.color = QStringLiteral("#12345680");
      repository.update(42, colored);
      QCOMPARE(read(repository, 42), colored);
      QCOMPARE(repository.findAll().first().fields.color, colored.color);
      auto another = colored;
      another.steps.first().id = QUuid::createUuid();
      const auto created = repository.create(another);
      QVERIFY(created.id > 42);
    }
    {
      Repository reopened(directory.path());
      reopened.initialize();
      QCOMPARE(read(reopened, 42).color, QStringLiteral("#12345680"));
      QCOMPARE(read(reopened, 42).photo, original.photo);
      auto removed = read(reopened, 42);
      removed.color.clear();
      reopened.update(42, removed);
    }
    Repository reopened(directory.path());
    reopened.initialize();
    QCOMPARE(read(reopened, 42), original);
  }
  void failedMigrationCanRetryWithoutPartialSchema() {
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("holzwerkprojekt.sqlite3"));
    {
      ExternalDatabase legacy(path);
      QFile schema(QFINDTESTDATA("fixtures/legacy_color_schema.sql"));
      QVERIFY(schema.open(QIODevice::ReadOnly));
      for (const auto& sql :
           QString::fromUtf8(schema.readAll()).split(QChar(';'), Qt::SkipEmptyParts))
        if (!sql.trimmed().isEmpty())
          legacy.execute(sql);
    }
    Repository repository(directory.path());
    {
      ExternalDatabase reader(path);
      reader.execute(QStringLiteral("BEGIN"));
      reader.execute(QStringLiteral("SELECT * FROM projects"));
      QVERIFY_EXCEPTION_THROWN(repository.initialize(), StorageError);
      QSqlQuery query(reader.database);
      QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")));
      QVERIFY(query.next());
      QCOMPARE(query.value(0).toInt(), 1);
      query.finish();
      QVERIFY(query.exec(QStringLiteral("PRAGMA table_info(projects)")));
      while (query.next())
        QVERIFY(query.value(1).toString() != QStringLiteral("color"));
      query.finish();
      reader.execute(QStringLiteral("ROLLBACK"));
    }
    repository.initialize();
    auto draft = example();
    draft.color = QStringLiteral("#ABCDEF");
    const auto id = repository.create(draft).id;
    QCOMPARE(read(repository, id), draft);
  }
  void legacyImportAndColorDifferences() {
    auto draft = example();
    auto legacy = encodeEntry(draft);
    legacy.remove(QStringLiteral("color"));
    QCOMPARE(decodeEntry(legacy), draft);
    auto invalid = legacy;
    invalid.insert(QStringLiteral("color"), true);
    QVERIFY_EXCEPTION_THROWN(decodeEntry(invalid), std::exception);
    invalid.insert(QStringLiteral("color"), QStringLiteral("bad"));
    QVERIFY_EXCEPTION_THROWN(decodeEntry(invalid), std::exception);
    QTemporaryDir data;
    Repository repository(data.path());
    repository.initialize();
    repository.create(draft);
    draft.color = QStringLiteral("#12345680");
    QCOMPARE(decodeEntry(encodeEntry(draft)), draft);
    QCOMPARE(repository.importEntries({draft}).added, 1);
    QCOMPARE(repository.importEntries({draft}).added, 0);
    QCOMPARE(repository.findAll().size(), 2);
    QCOMPARE(repository.transferSnapshot().last().color, draft.color);
  }
  void colorWriteFailureKeepsSavedDraft() {
    QTemporaryDir data;
    Repository repository(data.path());
    repository.initialize();
    auto draft = example();
    draft.color = QStringLiteral("#123456");
    const auto id = repository.create(draft).id;
    ExternalDatabase external(data.filePath(QStringLiteral("holzwerkprojekt.sqlite3")));
    external.execute(QStringLiteral("CREATE TRIGGER reject_color BEFORE UPDATE ON projects BEGIN "
                                    "SELECT RAISE(ABORT,'simulated failure'); END"));
    auto changed = draft;
    changed.color = QStringLiteral("#ABCDEF");
    changed.note = QStringLiteral("Changed");
    QVERIFY_EXCEPTION_THROWN(repository.update(id, changed), StorageError);
    QCOMPARE(read(repository, id), draft);
    external.execute(QStringLiteral("DROP TRIGGER reject_color"));
    repository.update(id, changed);
    QCOMPARE(read(repository, id), changed);
  }
  void editorWorkflow_data() {
    QTest::addColumn<QString>("tag");
    for (const auto& tag : supportedLanguages())
      QTest::newRow(qPrintable(tag)) << tag;
  }
  void editorWorkflow() {
    QFETCH(QString, tag);
    Translations translations;
    translations.install(tag);
    QTemporaryDir data;
    auto original = example();
    original.color = QStringLiteral("#315A7D80");
    qint64 id = 0;
    {
      Repository repository(data.path());
      repository.initialize();
      id = repository.create(original).id;
    }
    StorageController controller([&] { return std::make_unique<Repository>(data.path()); });
    QSignalSpy loaded(&controller, &StorageController::loaded);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }));
    WoodProject entry{id, original, true};
    Editor editor(controller, entry, nullptr, QImage::fromData(original.photo));
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    auto* field = editor.findChild<ColorField*>();
    QVERIFY(field);
    QCOMPARE(editor.draft(), original);
    auto* details = editor.findChild<QLineEdit*>(QStringLiteral("projectMaterial"));
    QVERIFY(details);
    details->setFocus();
    QTest::keyClick(details, Qt::Key_Tab);
    QCOMPARE(QApplication::focusWidget(),
             field->findChild<QPushButton*>(QStringLiteral("chooseColor")));
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
    QCOMPARE(QApplication::focusWidget(),
             field->findChild<QPushButton*>(QStringLiteral("clearColor")));
    capture(editor, tag + QStringLiteral("-editor"));
    choose(*field, QColor(10, 20, 30, 128), false);
    QCOMPARE(editor.draft(), original);
    choose(*field, QColor(10, 20, 30, 128), true, tag + QStringLiteral("-picker"));
    QCOMPARE(editor.draft().color, QStringLiteral("#0A141E80"));
    // Closing a draft changed only by color must ask and keep editing by default.
    bool prompted = false;
    QTimer::singleShot(0, &editor, [&] {
      auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
      if (message) {
        prompted = true;
        message->defaultButton()->click();
      }
    });
    QVERIFY(!editor.requestClose());
    QVERIFY(prompted);
    QVERIFY(editor.isVisible());
    QSignalSpy saved(&controller, &StorageController::saved);
    editor.save();
    QVERIFY(QTest::qWaitFor([&] { return saved.size() == 1; }));
    QVERIFY(!editor.isVisible());
    auto stored = qvariant_cast<WoodProject>(saved.first().first());
    // Project results are list summaries. Use the known original photo in the edit snapshot.
    stored.fields.photo = original.photo;
    Editor second(controller, stored, nullptr, QImage::fromData(original.photo));
    second.show();
    auto* clear = second.findChild<QPushButton*>(QStringLiteral("clearColor"));
    QVERIFY(clear && clear->isEnabled());
    clear->click();
    QVERIFY(second.draft().color.isEmpty());
    QVERIFY(!clear->isEnabled());
    second.save();
    QVERIFY(QTest::qWaitFor([&] { return saved.size() == 2; }));
    QVERIFY(qvariant_cast<WoodProject>(saved.last().first()).fields.color.isEmpty());
  }
  void tableSearchSortAndSwatch() {
    QTemporaryDir data;
    qint64 id = 0;
    {
      Repository repository(data.path());
      repository.initialize();
      auto first = example();
      first.color = QStringLiteral("#0000FF");
      id = repository.create(first).id;
      first.color = QStringLiteral("#FF0000");
      first.steps.first().id = QUuid::createUuid();
      repository.create(first);
    }
    StorageController controller([&] { return std::make_unique<Repository>(data.path()); });
    QSignalSpy loaded(&controller, &StorageController::loaded);
    MainWindow window(controller, data.path());
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }));
    auto* table = window.findChild<QTableWidget*>();
    QVERIFY(table);
    const int column = static_cast<int>(SortField::Color);
    table->horizontalHeader()->setSortIndicator(column, Qt::AscendingOrder);
    QCOMPARE(table->item(0, column)->text(), QStringLiteral("#0000FF"));
    QVERIFY(!table->item(0, column)->icon().isNull());
    table->selectRow(0);
    table->horizontalHeader()->setSortIndicator(column, Qt::DescendingOrder);
    QCOMPARE(table->item(0, column)->text(), QStringLiteral("#FF0000"));
    QCOMPARE(table->item(table->currentRow(), 0)->data(Qt::UserRole).toLongLong(), id);
    auto* search = window.findChild<QLineEdit*>();
    QVERIFY(search);
    search->setText(QStringLiteral("#0000ff"));
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 0)->data(Qt::UserRole).toLongLong(), id);
    capture(window, QStringLiteral("color-search"));
    QVERIFY(QTest::qWaitFor([&] { return !controller.isBusy(); }));
  }
};
QTEST_MAIN(ColorTest)
#include "color_test.moc"
