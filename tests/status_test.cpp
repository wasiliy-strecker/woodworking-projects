#include "application/project_service.h"
#include "persistence/sqlite_project_repository.h"
#include "ui/main_window.h"

#include <QApplication>
#include <QBuffer>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QtTest>

using namespace woodwork;
namespace {
QByteArray picture(QColor color) {
  QImage image(64, 48, QImage::Format_RGB32);
  image.fill(color);
  QByteArray result;
  QBuffer buffer(&result);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");
  return result;
}
class External final {
public:
  explicit External(const QString& directory) : name_(QUuid::createUuid().toString()) {
    database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name_);
    database.setDatabaseName(directory + QStringLiteral("/holzwerkprojekt.sqlite3"));
    if (!database.open())
      throw std::runtime_error("External database failed");
  }
  ~External() {
    database.close();
    database = {};
    QSqlDatabase::removeDatabase(name_);
  }
  void sql(const QString& text) {
    QSqlQuery query(database);
    if (!query.exec(text))
      throw StorageError(query.lastError().text());
  }
  QSqlDatabase database;

private:
  QString name_;
};
void discard(bool* confirmed, bool keep = false) {
  QTimer::singleShot(0, qApp, [confirmed, keep] {
    auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (!box)
      return;
    *confirmed = box->defaultButton()->text() == QStringLiteral("Weiterbearbeiten");
    box->findChild<QPushButton*>(keep ? QStringLiteral("keepEditing")
                                      : QStringLiteral("discardChanges"))
        ->click();
  });
}
RepositoryFactory factory(const QString& path) {
  return [path] { return std::make_unique<SqliteProjectRepository>(path); };
}
QComboBox* status(ProjectEditor& editor) {
  return editor.findChild<QComboBox*>(QStringLiteral("projectStatus"));
}
} // namespace
class StatusTest final : public QObject {
  Q_OBJECT
private slots:
  void explicitMapping_data() {
    QTest::addColumn<ProjectStatus>("value");
    QTest::addColumn<QString>("key");
    QTest::addColumn<QString>("label");
    QTest::newRow("planned") << ProjectStatus::Planned << QStringLiteral("planned")
                             << QStringLiteral("Geplant");
    QTest::newRow("started") << ProjectStatus::Started << QStringLiteral("started")
                             << QStringLiteral("Angefangen");
    QTest::newRow("finished") << ProjectStatus::Finished << QStringLiteral("finished")
                              << QStringLiteral("Fertig");
  }
  void explicitMapping() {
    QFETCH(ProjectStatus, value);
    QFETCH(QString, key);
    QFETCH(QString, label);
    QCOMPARE(storedStatus(value), key);
    QCOMPARE(displayStatus(value), label);
    QCOMPARE(statusFromStorage(key), value);
    QTemporaryDir directory;
    qint64 id = 0;
    {
      SqliteProjectRepository repository(directory.path());
      repository.initialize();
      id = repository.create({QStringLiteral("Modell"), {}, {}, {}, value}).id;
      External external(directory.path());
      QSqlQuery query(external.database);
      QVERIFY(query.exec(QStringLiteral("SELECT status FROM projects")));
      QVERIFY(query.next());
      QCOMPARE(query.value(0).toString(), key);
    }
    SqliteProjectRepository reopened(directory.path());
    reopened.initialize();
    QCOMPARE(reopened.find(id).status, value);
    QCOMPARE(reopened.findAll().first().fields.status, value);
  }
  void invalidValues() {
    for (const auto& key : {QString(), QStringLiteral("Geplant"), QStringLiteral("0"),
                            QStringLiteral("PLANNED"), QStringLiteral("future")}) {
      QVERIFY_EXCEPTION_THROWN(statusFromStorage(key), ValidationError);
    }
    const auto invalid = static_cast<ProjectStatus>(200);
    QVERIFY_EXCEPTION_THROWN(storedStatus(invalid), ValidationError);
    QVERIFY_EXCEPTION_THROWN(displayStatus(invalid), ValidationError);
    QVERIFY_EXCEPTION_THROWN(validateDraft({QStringLiteral("Name"), {}, {}, {}, invalid}),
                             ValidationError);
    QTemporaryDir directory;
    SqliteProjectRepository repository(directory.path());
    repository.initialize();
    repository.create({QStringLiteral("Original"), {}, {}, {}});
    External external(directory.path());
    for (const auto& sql : {QStringLiteral("UPDATE projects SET status=NULL"),
                            QStringLiteral("UPDATE projects SET status='invalid'")}) {
      QVERIFY_EXCEPTION_THROWN(external.sql(sql), StorageError);
    }
    QCOMPARE(repository.findAll().first().fields.status, ProjectStatus::Planned);
    external.sql(QStringLiteral("PRAGMA ignore_check_constraints=ON"));
    external.sql(QStringLiteral("UPDATE projects SET status='unknown-future-value'"));
    QVERIFY_EXCEPTION_THROWN(repository.findAll(), StorageError);
    QVERIFY_EXCEPTION_THROWN(repository.find(1), StorageError);
  }
  void everyTransitionPreservesOtherFieldsAndDuplicate() {
    QTemporaryDir directory;
    SqliteProjectRepository repository(directory.path());
    repository.initialize();
    const auto bytes = picture(Qt::red);
    ProjectDraft draft{QStringLiteral("Gleich"), QStringLiteral("Material"),
                       QStringLiteral("Regal"),  QStringLiteral("  Notiz\n\nEnde\n"),
                       ProjectStatus::Planned,   bytes};
    const auto first = repository.create(draft);
    draft.status = ProjectStatus::Finished;
    const auto duplicate = repository.create(draft);
    for (const auto from :
         {ProjectStatus::Planned, ProjectStatus::Started, ProjectStatus::Finished}) {
      draft.status = from;
      repository.update(first.id, draft);
      for (const auto to :
           {ProjectStatus::Planned, ProjectStatus::Started, ProjectStatus::Finished}) {
        draft.status = to;
        repository.update(first.id, draft);
        QVERIFY(repository.find(first.id) == draft);
        QCOMPARE(repository.find(duplicate.id).status, ProjectStatus::Finished);
        QCOMPARE(repository.find(duplicate.id).photo, bytes);
        draft.status = from;
        repository.update(first.id, draft);
      }
    }
  }
  void defaultsAndFullyRevertedDialog() {
    QCOMPARE(ProjectDraft{}.status, ProjectStatus::Planned);
    QTemporaryDir directory;
    StorageController controller(factory(directory.path()));
    ProjectEditor editor(controller, std::nullopt);
    editor.show();
    QVERIFY(editor.draft().steps.isEmpty());
    QVERIFY(editor.draft().photo.isEmpty());
    QCOMPARE(status(editor)->count(), 3);
    QVERIFY(!status(editor)->isEditable());
    QCOMPARE(status(editor)->currentText(), QStringLiteral("Geplant"));
    status(editor)->setCurrentIndex(2);
    status(editor)->setCurrentIndex(0);
    QCOMPARE(editor.draft().status, ProjectStatus::Planned);
    QVERIFY(editor.requestClose());
  }
  void statusOnlyDiscard_data() {
    QTest::addColumn<int>("action");
    QTest::newRow("cancel") << 0;
    QTest::newRow("escape") << 1;
    QTest::newRow("close") << 2;
  }
  void statusOnlyDiscard() {
    QFETCH(int, action);
    QTemporaryDir directory;
    WoodProject original;
    {
      SqliteProjectRepository repo(directory.path());
      repo.initialize();
      original = repo.create({QStringLiteral("Original"), {}, {}, {}});
    }
    StorageController controller(factory(directory.path()));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    QSignalSpy saved(&controller, &StorageController::saved);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
    ProjectEditor editor(controller, original);
    editor.show();
    status(editor)->setCurrentIndex(2);
    QTest::qWait(20);
    QCOMPARE(saved.size(), 0);
    bool confirmed = false;
    discard(&confirmed, true);
    if (action == 0)
      editor.findChild<QPushButton*>(QStringLiteral("cancelEdit"))->click();
    else if (action == 1)
      QTest::keyClick(&editor, Qt::Key_Escape);
    else
      editor.close();
    QVERIFY(confirmed);
    QVERIFY(editor.isVisible());
    QCOMPARE(editor.draft().status, ProjectStatus::Finished);
    confirmed = false;
    discard(&confirmed);
    QVERIFY(editor.requestClose());
    QVERIFY(confirmed);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (2); }, 5000));
    QCOMPARE(qvariant_cast<ProjectList>(loaded.last().first()).first().fields.status,
             ProjectStatus::Planned);
  }
  void realCommitFailureRetainsEntireDraftAndRetry() {
    QTemporaryDir directory;
    const auto bytes = picture(Qt::red);
    ProjectDraft old{QStringLiteral("Original"), QStringLiteral("Material"), QStringLiteral("Ort"),
                     QStringLiteral("Notiz\n"),  ProjectStatus::Started,     bytes};
    WoodProject original;
    {
      SqliteProjectRepository repo(directory.path());
      repo.initialize();
      original = repo.create(old);
    }
    original.fields = old;
    StorageController controller(factory(directory.path()));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    QSignalSpy failed(&controller, &StorageController::failed);
    QSignalSpy saved(&controller, &StorageController::saved);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
    ProjectEditor editor(controller, original, nullptr, QImage::fromData(bytes));
    editor.show();
    editor.findChild<QLineEdit*>(QStringLiteral("projectName"))
        ->setText(QStringLiteral("Geändert"));
    status(editor)->setCurrentIndex(2);
    const auto replacement = picture(Qt::blue);
    QFile file(directory.filePath(QStringLiteral("replace.png")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(replacement);
    file.close();
    editor.importFromPath(file.fileName());
    QVERIFY(QTest::qWaitFor([&] { return !controller.isBusy(); }, 5000));
    const auto draft = editor.draft();
    QCOMPARE(draft.photo, replacement);
    External reader(directory.path());
    reader.sql(QStringLiteral("BEGIN"));
    reader.sql(QStringLiteral("SELECT * FROM projects"));
    editor.save();
    QVERIFY(QTest::qWaitFor([&] { return (failed.size()) == (1); }, 5000));
    QCOMPARE(saved.size(), 0);
    QVERIFY(editor.isVisible());
    QVERIFY(editor.draft() == draft);
    QSqlQuery query(reader.database);
    QVERIFY(query.exec(QStringLiteral("SELECT name,status,photo FROM projects")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), old.name);
    QCOMPARE(query.value(1).toString(), QStringLiteral("started"));
    QCOMPARE(query.value(2).toByteArray(), old.photo);
    query.finish();
    reader.sql(QStringLiteral("ROLLBACK"));
    editor.save();
    editor.save();
    QVERIFY(QTest::qWaitFor([&] { return (saved.size()) == (1); }, 5000));
    QVERIFY(!editor.isVisible());
    QVERIFY(query.exec(QStringLiteral("SELECT name,status,photo FROM projects")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), draft.name);
    QCOMPARE(query.value(1).toString(), QStringLiteral("finished"));
    QCOMPARE(query.value(2).toByteArray(), replacement);
  }
};
QTEST_MAIN(StatusTest)
#include "status_test.moc"
