#include "application/project_service.h"
#include "persistence/sqlite_project_repository.h"

#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

using namespace woodwork;

namespace {
QString databasePath(const QTemporaryDir& directory) {
  return QDir(directory.path()).filePath(QStringLiteral("holzwerkprojekt.sqlite3"));
}

class ExternalDatabase final {
public:
  explicit ExternalDatabase(const QString& path) {
    name_ = QUuid::createUuid().toString();
    database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name_);
    database.setDatabaseName(path);
    database.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=50"));
    if (!database.open()) {
      throw StorageError(database.lastError().text());
    }
  }
  ~ExternalDatabase() {
    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(name_);
  }
  void execute(const QString& sql) {
    QSqlQuery query(database);
    if (!query.exec(sql)) {
      throw StorageError(query.lastError().text());
    }
  }
  QSqlDatabase database;

private:
  QString name_;
};

QByteArray readFile(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.readAll();
}
} // namespace

class RepositoryTest final : public QObject {
  Q_OBJECT
private slots:
  void crudAndIndependentDuplicates() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteProjectRepository repository(directory.path());
    ProjectService service(repository);
    QVERIFY(service.load().isEmpty());
    const auto first = service.save(0, {QStringLiteral("Azul"), {}, {}, {}});
    const auto second = service.save(0, first.fields);
    QVERIFY(first.id > 0);
    QVERIFY(first.id != second.id);
    auto changed = first.fields;
    changed.location = QStringLiteral("Regal 4");
    const auto updated = service.save(first.id, changed);
    QCOMPARE(updated.id, first.id);
    const auto projects = service.load();
    QCOMPARE(projects.size(), 2);
    QVERIFY(projects.first() == updated);
    QVERIFY(projects.last() == second);
    service.remove(first.id);
    QVERIFY(service.load() == ProjectList{second});
    service.remove(second.id);
    QVERIFY(service.load().isEmpty());
    QVERIFY(service.save(0, second.fields).id > second.id);
  }
  void reopensWithAllFieldsAndSqlText() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    WoodProject saved;
    const ProjectDraft fields{
        QStringLiteral("  Ömer's Projekt'); DROP TABLE projects; --  "),
        QStringLiteral("Müller & \"Söhne\""), QStringLiteral("Küche\u00a0"),
        QStringLiteral("  Erweiterung liegt hier.\n\n\tSELECT * FROM projects;\n ")};
    {
      SqliteProjectRepository repository(directory.path());
      ProjectService service(repository);
      service.load();
      saved = service.save(0, fields);
    }
    {
      SqliteProjectRepository repository(directory.path());
      ProjectService service(repository);
      const auto projects = service.load();
      QCOMPARE(projects.size(), 1);
      QVERIFY(projects.first() == saved);
      QCOMPARE(projects.first().fields.note, fields.note);
      service.save(0, {QStringLiteral("Noch ein Projekt"), {}, {}, {}});
      QCOMPARE(service.load().size(), 2);
    }
  }
  void invalidAndMissingEntriesDoNotChangeData() {
    QTemporaryDir directory;
    SqliteProjectRepository repository(directory.path());
    ProjectService service(repository);
    service.load();
    const auto saved = service.save(0, {QStringLiteral("Unverändert"), {}, {}, {}});
    QVERIFY_EXCEPTION_THROWN(service.save(saved.id, {QStringLiteral("\u00a0"), {}, {}, {}}),
                             ValidationError);
    QVERIFY_EXCEPTION_THROWN(service.save(999, saved.fields), StorageError);
    QVERIFY_EXCEPTION_THROWN(service.remove(999), StorageError);
    QVERIFY(service.load() == ProjectList{saved});
  }
  void lockedWriteAndRetry() {
    QTemporaryDir directory;
    SqliteProjectRepository repository(directory.path());
    ProjectService service(repository);
    service.load();
    const auto saved = service.save(0, {QStringLiteral("Original"), {}, {}, {}});
    ExternalDatabase external(databasePath(directory));
    external.execute(QStringLiteral("BEGIN EXCLUSIVE"));
    QVERIFY_EXCEPTION_THROWN(service.save(saved.id, {QStringLiteral("Entwurf"), {}, {}, {}}),
                             StorageError);
    QVERIFY_EXCEPTION_THROWN(service.remove(saved.id), StorageError);
    external.execute(QStringLiteral("ROLLBACK"));
    QVERIFY(service.load() == ProjectList{saved});
    QCOMPARE(service.save(saved.id, {QStringLiteral("Entwurf"), {}, {}, {}}).fields.name,
             QStringLiteral("Entwurf"));
  }
  void failedCommitRollsBackAllFields() {
    QTemporaryDir directory;
    SqliteProjectRepository repository(directory.path());
    ProjectService service(repository);
    service.load();
    const auto saved = service.save(0, {QStringLiteral("Original"), QStringLiteral("Material"),
                                        QStringLiteral("Regal"), QStringLiteral("Notiz")});
    ExternalDatabase reader(databasePath(directory));
    reader.execute(QStringLiteral("BEGIN"));
    reader.execute(QStringLiteral("SELECT * FROM projects"));
    QVERIFY_EXCEPTION_THROWN(service.save(saved.id, {QStringLiteral("Neu"), QStringLiteral("A"),
                                                     QStringLiteral("B"), QStringLiteral("C")}),
                             StorageError);
    reader.execute(QStringLiteral("ROLLBACK"));
    QVERIFY(service.load() == ProjectList{saved});
    QVERIFY_EXCEPTION_THROWN(service.save(999, saved.fields), StorageError);
    QCOMPARE(service.save(saved.id, {QStringLiteral("Erneut"), {}, {}, {}}).id, saved.id);
  }
  void triggerWriteFailurePreservesData() {
    QTemporaryDir directory;
    SqliteProjectRepository repository(directory.path());
    ProjectService service(repository);
    service.load();
    const auto saved = service.save(0, {QStringLiteral("Original"), {}, {}, {}});
    ExternalDatabase external(databasePath(directory));
    external.execute(QStringLiteral("CREATE TRIGGER reject_update BEFORE UPDATE ON projects BEGIN "
                                    "SELECT RAISE(ABORT, 'synthetischer Schreibfehler'); END"));
    QVERIFY_EXCEPTION_THROWN(service.save(saved.id, {QStringLiteral("Neu"), {}, {}, {}}),
                             StorageError);
    QVERIFY(service.load() == ProjectList{saved});
    external.execute(QStringLiteral("DROP TRIGGER reject_update"));
    QCOMPARE(service.save(saved.id, {QStringLiteral("Neu"), {}, {}, {}}).id, saved.id);
  }
  void newerSchemaUnchanged() {
    QTemporaryDir directory;
    {
      ExternalDatabase database(databasePath(directory));
      database.execute(QStringLiteral("PRAGMA user_version=99"));
      database.execute(QStringLiteral("CREATE TABLE future(value TEXT)"));
      database.execute(QStringLiteral("INSERT INTO future VALUES('synthetisch')"));
    }
    const auto before = readFile(databasePath(directory));
    SqliteProjectRepository repository(directory.path());
    QVERIFY_EXCEPTION_THROWN(repository.initialize(), StorageError);
    QCOMPARE(readFile(databasePath(directory)), before);
  }
  void corruptDatabaseUnchanged() {
    QTemporaryDir directory;
    QFile file(databasePath(directory));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("Synthetisch beschaedigt. Keine SQLite-Datei.");
    file.close();
    const auto before = readFile(databasePath(directory));
    SqliteProjectRepository repository(directory.path());
    QVERIFY_EXCEPTION_THROWN(repository.initialize(), StorageError);
    QCOMPARE(readFile(databasePath(directory)), before);
  }
  void existingEmptyOrForeignDatabase_data() {
    QTest::addColumn<bool>("foreignSchema");
    QTest::newRow("empty-existing-file") << false;
    QTest::newRow("foreign-schema") << true;
  }
  void existingEmptyOrForeignDatabase() {
    QFETCH(bool, foreignSchema);
    QTemporaryDir directory;
    if (foreignSchema) {
      ExternalDatabase database(databasePath(directory));
      database.execute(QStringLiteral("CREATE TABLE unrelated(value TEXT)"));
    } else {
      QFile file(databasePath(directory));
      QVERIFY(file.open(QIODevice::WriteOnly));
    }
    const auto before = readFile(databasePath(directory));
    SqliteProjectRepository repository(directory.path());
    QVERIFY_EXCEPTION_THROWN(repository.initialize(), StorageError);
    QCOMPARE(readFile(databasePath(directory)), before);
  }
  void wrongSchemaVersionOne() {
    QTemporaryDir directory;
    {
      ExternalDatabase database(databasePath(directory));
      database.execute(QStringLiteral("PRAGMA user_version=1"));
      database.execute(QStringLiteral("CREATE TABLE projects(wrong TEXT)"));
    }
    SqliteProjectRepository repository(directory.path());
    QVERIFY_EXCEPTION_THROWN(repository.initialize(), StorageError);
  }
  void wrongTypesInKnownSchema() {
    QTemporaryDir directory;
    {
      ExternalDatabase database(databasePath(directory));
      database.execute(QStringLiteral("PRAGMA user_version=1"));
      database.execute(QStringLiteral(
          "CREATE TABLE projects(id TEXT,name TEXT,material TEXT,location TEXT,note TEXT)"));
    }
    const auto before = readFile(databasePath(directory));
    SqliteProjectRepository repository(directory.path());
    QVERIFY_EXCEPTION_THROWN(repository.initialize(), StorageError);
    QCOMPARE(readFile(databasePath(directory)), before);
  }
  void corruptStoredNameIsReportedWithoutRepair() {
    QTemporaryDir directory;
    SqliteProjectRepository repository(directory.path());
    repository.initialize();
    repository.create({QStringLiteral("Original"), {}, {}, {}});
    ExternalDatabase external(databasePath(directory));
    external.execute(QStringLiteral("UPDATE projects SET name=char(160)"));
    const auto before = readFile(databasePath(directory));
    QVERIFY_EXCEPTION_THROWN(repository.findAll(), StorageError);
    QVERIFY_EXCEPTION_THROWN(repository.find(1), StorageError);
    QCOMPARE(readFile(databasePath(directory)), before);
  }
  void lockedStartupCanRetry() {
    QTemporaryDir directory;
    {
      SqliteProjectRepository repository(directory.path());
      repository.initialize();
      repository.create({QStringLiteral("Vorhanden"), {}, {}, {}});
    }
    ExternalDatabase blocker(databasePath(directory));
    blocker.execute(QStringLiteral("BEGIN EXCLUSIVE"));
    SqliteProjectRepository repository(directory.path());
    QVERIFY_EXCEPTION_THROWN(repository.initialize(), StorageError);
    blocker.execute(QStringLiteral("ROLLBACK"));
    repository.initialize();
    QCOMPARE(repository.findAll().first().fields.name, QStringLiteral("Vorhanden"));
  }
  void secondInstanceRefusedThenRetries() {
    QTemporaryDir directory;
    auto first = std::make_unique<SqliteProjectRepository>(directory.path());
    first->initialize();
    SqliteProjectRepository second(directory.path());
    QVERIFY_EXCEPTION_THROWN(second.initialize(), StorageError);
    first.reset();
    second.initialize();
    QVERIFY(second.findAll().isEmpty());
  }
  void invalidDirectory() {
    QTemporaryDir directory;
    const auto path = QDir(directory.path()).filePath(QStringLiteral("file"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    SqliteProjectRepository repository(path);
    QVERIFY_EXCEPTION_THROWN(repository.initialize(), StorageError);
  }
  void readOnlyFile() {
#ifdef Q_OS_WIN
    QSKIP("Windows-Schreibschutz erfordert einen nativen ACL-Test.");
#else
    QTemporaryDir directory;
    WoodProject saved;
    {
      SqliteProjectRepository repository(directory.path());
      ProjectService service(repository);
      service.load();
      saved = service.save(0, {QStringLiteral("Original"), {}, {}, {}});
    }
    QVERIFY(QFile::setPermissions(databasePath(directory), QFileDevice::ReadOwner));
    {
      SqliteProjectRepository repository(directory.path());
      ProjectService service(repository);
      QVERIFY(service.load() == ProjectList{saved});
      QVERIFY_EXCEPTION_THROWN(service.save(saved.id, {QStringLiteral("Neu"), {}, {}, {}}),
                               StorageError);
      QVERIFY(service.load() == ProjectList{saved});
    }
    QVERIFY(QFile::setPermissions(databasePath(directory),
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner));
#endif
  }
};

QTEST_GUILESS_MAIN(RepositoryTest)
#include "repository_test.moc"
