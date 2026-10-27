#include "persistence/sqlite_project_repository.h"
#include "ui/checklist_editor.h"
#include "ui/main_window.h"

#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

using namespace woodwork;
namespace {
ChecklistStep step(const QString& text, qint64 position, bool completed = false) {
  return {QUuid::createUuid(), text, completed, position};
}
QByteArray photo(QColor color) {
  QImage image(48, 32, QImage::Format_RGB32);
  image.fill(color);
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");
  return bytes;
}
ProjectDraft example() {
  return {QStringLiteral("Regal"),
          QStringLiteral("Eiche"),
          QStringLiteral("Werkstatt"),
          QStringLiteral("  Notiz\n\n  Einrückung\n"),
          ProjectStatus::Started,
          photo(Qt::red),
          {step(QStringLiteral("Holz zuschneiden"), 0),
           step(QStringLiteral("Kanten schleifen"), 1, true),
           step(QStringLiteral("Oberfläche ölen"), 2)}};
}
class External final {
public:
  explicit External(const QString& directory) : name_(QUuid::createUuid().toString()) {
    database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name_);
    database.setDatabaseName(directory + QStringLiteral("/holzwerkprojekt.sqlite3"));
    if (!database.open())
      throw StorageError(database.lastError().text());
    sql(QStringLiteral("PRAGMA foreign_keys=ON"));
  }
  ~External() {
    database.close();
    database = {};
    QSqlDatabase::removeDatabase(name_);
  }
  void sql(const QString& statement) {
    QSqlQuery query(database);
    if (!query.exec(statement))
      throw StorageError(query.lastError().text());
  }
  qint64 count(const QString& table) {
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT count(*) FROM ") + table) || !query.next())
      throw StorageError(query.lastError().text());
    return query.value(0).toLongLong();
  }
  QSqlDatabase database;

private:
  QString name_;
};
RepositoryFactory factory(const QString& directory) {
  return [directory] { return std::make_unique<SqliteProjectRepository>(directory); };
}
QWidget* row(ProjectEditor& editor, const QUuid& id) {
  for (auto* candidate : editor.findChildren<QWidget*>()) {
    if (candidate->property("stepId").toString() == id.toString(QUuid::WithoutBraces))
      return candidate;
  }
  return nullptr;
}
void answer(bool& checked, bool keep = false) {
  QTimer::singleShot(0, qApp, [&checked, keep] {
    auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (!box)
      return;
    checked = box->defaultButton()->objectName() == QStringLiteral("keepEditing");
    box->findChild<QPushButton*>(keep ? QStringLiteral("keepEditing")
                                      : QStringLiteral("discardChanges"))
        ->click();
  });
}
} // namespace

class ChecklistTest final : public QObject {
  Q_OBJECT
private slots:
  void validation_data() {
    QTest::addColumn<QString>("text");
    QTest::newRow("empty") << QString();
    QTest::newRow("ascii") << QStringLiteral(" \t\r\n ");
    QTest::newRow("unicode") << QStringLiteral("\u00a0\u2007\u202f\u3000\u2028");
    QTest::newRow("multiline") << QStringLiteral("Holz\nsägen");
  }
  void validation() {
    QFETCH(QString, text);
    auto draft = example();
    draft.steps[1].text = text;
    QVERIFY_EXCEPTION_THROWN(validateDraft(draft), ValidationError);
  }
  void emptyListTrimmingAndInvalidIdentity() {
    QVERIFY(ProjectDraft{}.steps.isEmpty());
    auto draft = example();
    draft.steps[0].text = QStringLiteral("\u3000 Holz sägen \u00a0");
    const auto clean = validateDraft(draft);
    QCOMPARE(clean.steps[0].text, QStringLiteral("Holz sägen"));
    QCOMPARE(clean.steps[0].id, draft.steps[0].id);
    draft.steps[1].id = draft.steps[0].id;
    QVERIFY_EXCEPTION_THROWN(validateDraft(draft), ValidationError);
    draft = example();
    draft.steps[0].id = {};
    QVERIFY_EXCEPTION_THROWN(validateDraft(draft), ValidationError);
    draft = example();
    draft.steps[0].position = -1;
    QVERIFY_EXCEPTION_THROWN(validateDraft(draft), ValidationError);
    draft = example();
    draft.steps[1].position = 0;
    QVERIFY_EXCEPTION_THROWN(validateDraft(draft), ValidationError);
    draft.steps.clear();
    QVERIFY(validateDraft(draft).steps.isEmpty());
  }
  void searchStepsUnicodeOnlyOneProject() {
    auto draft = example();
    draft.steps = {step(QStringLiteral("O\u0308len"), 0), step(QStringLiteral("ÖLEN"), 7)};
    const ProjectList rows{{7, draft}, {8, {QStringLiteral("Anderes"), {}, {}, {}}}};
    for (const auto& query :
         {QStringLiteral("ölen"), QStringLiteral("ÖLEN"), QStringLiteral("O\u0308LEN")}) {
      const auto result = searchProjects(rows, query);
      QCOMPARE(result.size(), 1);
      QCOMPARE(result.first().id, 7);
    }
    QCOMPARE(searchProjects(rows, {}).size(), 2);
    QVERIFY(searchProjects(rows, QStringLiteral("fehlt")).isEmpty());
  }
  void persistenceIdentityGapsDuplicatesStatusAndCascade() {
    QTemporaryDir directory;
    auto draft = example();
    draft.steps[1].text = draft.steps[0].text;
    qint64 first = 0;
    qint64 other = 0;
    ProjectDraft duplicate = draft;
    for (auto& item : duplicate.steps)
      item.id = QUuid::createUuid();
    duplicate.status = ProjectStatus::Finished;
    {
      SqliteProjectRepository repository(directory.path());
      repository.initialize();
      first = repository.create(draft).id;
      other = repository.create(duplicate).id;
      draft.steps.removeAt(1);
      draft.steps[1].text = QStringLiteral("Ömer's Öl '); DROP TABLE projects; --");
      draft.steps[1].completed = true;
      repository.update(first, draft);
      QVERIFY(repository.find(first) == draft);
      QVERIFY(repository.find(other) == duplicate);
      for (const auto value :
           {ProjectStatus::Finished, ProjectStatus::Planned, ProjectStatus::Started}) {
        draft.status = value;
        repository.update(first, draft);
        QVERIFY(repository.find(first) == draft);
      }
      for (auto& item : draft.steps)
        item.completed = true;
      repository.update(first, draft);
      QCOMPARE(repository.find(first).status, ProjectStatus::Started);
    }
    SqliteProjectRepository reopened(directory.path());
    reopened.initialize();
    QVERIFY(reopened.find(first) == draft);
    QVERIFY(reopened.find(other) == duplicate);
    QCOMPARE(reopened.find(first).steps[1].position, 2);
    External external(directory.path());
    QCOMPARE(external.count(QStringLiteral("checklist_steps")), 5);
    reopened.remove(first);
    QCOMPARE(external.count(QStringLiteral("checklist_steps")), 3);
    QVERIFY(reopened.find(other) == duplicate);
    reopened.remove(other);
    QCOMPARE(external.count(QStringLiteral("checklist_steps")), 0);
  }
  void realLateInsertFailureRollsBackUpdateAndCreateThenRetry() {
    QTemporaryDir directory;
    SqliteProjectRepository repository(directory.path());
    repository.initialize();
    const auto old = example();
    const auto id = repository.create(old).id;
    auto changed = example();
    changed.name = QStringLiteral("Neu");
    changed.material = QStringLiteral("Birke");
    changed.location = QStringLiteral("Keller");
    changed.note = QStringLiteral("Neue\nNotiz");
    changed.photo = photo(Qt::blue);
    changed.status = ProjectStatus::Finished;
    changed.steps[1].text = QStringLiteral("SPÄTER FEHLER");
    External external(directory.path());
    external.sql(QStringLiteral("CREATE TRIGGER fail_late BEFORE INSERT ON checklist_steps "
                                "WHEN NEW.text='SPÄTER FEHLER' BEGIN SELECT "
                                "RAISE(ABORT,'synthetisch später Fehler'); END"));
    QVERIFY_EXCEPTION_THROWN(repository.update(id, changed), StorageError);
    QVERIFY(repository.find(id) == old);
    QCOMPARE(external.count(QStringLiteral("projects")), 1);
    QCOMPARE(external.count(QStringLiteral("checklist_steps")), 3);
    QVERIFY_EXCEPTION_THROWN(repository.create(changed), StorageError);
    QCOMPARE(external.count(QStringLiteral("projects")), 1);
    QCOMPARE(external.count(QStringLiteral("checklist_steps")), 3);
    QVERIFY(repository.find(id) == old);
    external.sql(QStringLiteral("DROP TRIGGER fail_late"));
    repository.update(id, changed);
    QVERIFY(repository.find(id) == changed);
    for (auto& item : changed.steps)
      item.id = QUuid::createUuid();
    const auto created = repository.create(changed);
    QVERIFY(repository.find(created.id) == changed);
  }
  void actualSchemaConstraints() {
    QTemporaryDir directory;
    SqliteProjectRepository repository(directory.path());
    repository.initialize();
    repository.create(example());
    External external(directory.path());
    for (const auto& statement :
         {QStringLiteral("UPDATE checklist_steps SET completed=2"),
          QStringLiteral("UPDATE checklist_steps SET completed=0.5"),
          QStringLiteral("UPDATE checklist_steps SET completed=NULL"),
          QStringLiteral("UPDATE checklist_steps SET position=-1"),
          QStringLiteral("UPDATE checklist_steps SET position=0.2"),
          QStringLiteral("UPDATE checklist_steps SET position=0"),
          QStringLiteral("UPDATE checklist_steps SET id=NULL"),
          QStringLiteral("UPDATE checklist_steps SET id='bad'"),
          QStringLiteral("UPDATE checklist_steps SET text=''"),
          QStringLiteral("UPDATE checklist_steps SET text=NULL"),
          QStringLiteral("UPDATE checklist_steps SET project_id=999"),
          QStringLiteral("UPDATE checklist_steps SET id='11111111-1111-4111-8111-111111111111'")}) {
      QVERIFY_EXCEPTION_THROWN(external.sql(statement), StorageError);
    }
    // The repository connection must also enforce FK cascades, not only this helper.
    repository.remove(1);
    QCOMPARE(external.count(QStringLiteral("checklist_steps")), 0);
  }
  void corruptStepValues_data() {
    QTest::addColumn<QString>("statement");
    QTest::newRow("bool") << QStringLiteral(
        "UPDATE checklist_steps SET completed=9 WHERE position=0");
    QTest::newRow("position") << QStringLiteral(
        "UPDATE checklist_steps SET position=-8 WHERE position=0");
    QTest::newRow("position-type")
        << QStringLiteral("UPDATE checklist_steps SET position='wrong' WHERE position=0");
    QTest::newRow("uuid") << QStringLiteral(
        "UPDATE checklist_steps SET id='not-a-uuid' WHERE position=0");
    QTest::newRow("whitespace") << QStringLiteral(
        "UPDATE checklist_steps SET text=char(160) WHERE position=0");
    QTest::newRow("text-type") << QStringLiteral(
        "UPDATE checklist_steps SET text=x'616263' WHERE position=0");
  }
  void corruptStepValues() {
    QFETCH(QString, statement);
    QTemporaryDir directory;
    SqliteProjectRepository repository(directory.path());
    repository.initialize();
    repository.create(example());
    External external(directory.path());
    external.sql(QStringLiteral("PRAGMA ignore_check_constraints=ON"));
    external.sql(statement);
    QVERIFY_EXCEPTION_THROWN(repository.find(1), StorageError);
    QVERIFY_EXCEPTION_THROWN(repository.findAll(), StorageError);
    QCOMPARE(external.count(QStringLiteral("checklist_steps")), 3);
  }
  void editorActionsUseIdsAndPreservePositions() {
    QTemporaryDir directory;
    StorageController controller(factory(directory.path()));
    auto draft = example();
    draft.steps[1].text = draft.steps[0].text;
    ProjectEditor editor(controller, WoodProject{1, draft, true}, nullptr,
                         QImage::fromData(draft.photo));
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    QTest::keyClick(&editor, Qt::Key_C, Qt::AltModifier);
    QCOMPARE(QApplication::focusWidget(), row(editor, draft.steps[0].id)->findChild<QCheckBox*>());
    row(editor, draft.steps[1].id)->findChild<QPushButton*>()->click();
    row(editor, draft.steps[2].id)->findChild<QLineEdit*>()->setText(QStringLiteral("Ölen"));
    auto* check = row(editor, draft.steps[2].id)->findChild<QCheckBox*>();
    check->setFocus();
    QTest::keyClick(check, Qt::Key_Space);
    QCOMPARE(editor.draft().steps[1].id, draft.steps[2].id);
    QCOMPARE(editor.draft().steps[1].position, 2);
    QVERIFY(editor.draft().steps[1].completed);
    QTest::keyClick(check, Qt::Key_Space);
    QVERIFY(!editor.draft().steps[1].completed);
    editor.findChild<QPushButton*>(QStringLiteral("addStep"))->click();
    const auto newStep = editor.draft().steps.last();
    QVERIFY(!newStep.id.isNull());
    QCOMPARE(newStep.position, 3);
    QVERIFY(!newStep.completed);
    QVERIFY(newStep.text.isEmpty());
    row(editor, newStep.id)->findChild<QLineEdit*>()->setText(QStringLiteral("  Polieren  "));
    QCOMPARE(validateDraft(editor.draft()).steps.last().text, QStringLiteral("Polieren"));
    QCOMPARE(editor.draft().status, draft.status);
  }
  void checklistOnlyDirtyProtection_data() {
    QTest::addColumn<int>("action");
    for (int i = 0; i < 5; ++i)
      QTest::newRow(qPrintable(QString::number(i))) << i;
  }
  void checklistOnlyDirtyProtection() {
    QFETCH(int, action);
    QTemporaryDir directory;
    StorageController controller(factory(directory.path()));
    auto draft = example();
    ProjectEditor editor(controller, WoodProject{1, draft, true}, nullptr,
                         QImage::fromData(draft.photo));
    editor.show();
    if (action == 0)
      row(editor, draft.steps[0].id)->findChild<QCheckBox*>()->setChecked(true);
    if (action == 1)
      row(editor, draft.steps[1].id)->findChild<QCheckBox*>()->setChecked(false);
    if (action == 2)
      row(editor, draft.steps[0].id)->findChild<QLineEdit*>()->setText(QStringLiteral("Umbenannt"));
    if (action == 3)
      editor.findChild<QPushButton*>(QStringLiteral("addStep"))->click();
    if (action == 4)
      row(editor, draft.steps[0].id)->findChild<QPushButton*>()->click();
    bool checked = false;
    answer(checked, true);
    QTest::keyClick(&editor, Qt::Key_Escape);
    QVERIFY(checked);
    QVERIFY(editor.isVisible());
    checked = false;
    answer(checked);
    QVERIFY(editor.close());
    QVERIFY(checked);
  }
  void fullyRevertedChecklistHasNoPrompt() {
    QTemporaryDir directory;
    StorageController controller(factory(directory.path()));
    auto draft = example();
    ProjectEditor editor(controller, WoodProject{1, draft, true}, nullptr,
                         QImage::fromData(draft.photo));
    auto* check = row(editor, draft.steps[0].id)->findChild<QCheckBox*>();
    check->setChecked(true);
    check->setChecked(false);
    auto* text = row(editor, draft.steps[0].id)->findChild<QLineEdit*>();
    text->setText(QStringLiteral("Anders"));
    text->setText(draft.steps[0].text);
    editor.findChild<QPushButton*>(QStringLiteral("addStep"))->click();
    const auto id = editor.draft().steps.last().id;
    row(editor, id)->findChild<QPushButton*>()->click();
    QVERIFY(editor.draft() == draft);
    QVERIFY(editor.requestClose());
  }
  void invalidStepAndLateSqlFailureKeepRealDialogThenRetry() {
    QTemporaryDir directory;
    auto old = example();
    WoodProject original;
    {
      SqliteProjectRepository repository(directory.path());
      repository.initialize();
      original = repository.create(old);
    }
    original.fields = old;
    StorageController controller(factory(directory.path()));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    QSignalSpy failed(&controller, &StorageController::failed);
    QSignalSpy saved(&controller, &StorageController::saved);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }, 5000));
    ProjectEditor editor(controller, original, nullptr, QImage::fromData(old.photo));
    editor.show();
    editor.findChild<QLineEdit*>(QStringLiteral("projectName"))->setText(QStringLiteral("Neu"));
    editor.findChild<QComboBox*>(QStringLiteral("projectStatus"))->setCurrentIndex(2);
    const auto path = directory.filePath(QStringLiteral("synthetic.png"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(photo(Qt::blue));
    file.close();
    editor.importFromPath(path);
    QVERIFY(QTest::qWaitFor([&] { return !controller.isBusy(); }, 5000));
    row(editor, old.steps[0].id)->findChild<QCheckBox*>()->setChecked(true);
    auto* text = row(editor, old.steps[1].id)->findChild<QLineEdit*>();
    text->setText(QStringLiteral("\u00a0\u3000"));
    const auto invalid = editor.draft();
    editor.save();
    QVERIFY(editor.isVisible());
    QVERIFY(editor.draft() == invalid);
    QVERIFY(editor.findChild<QLabel*>(QStringLiteral("editorError"))
                ->text()
                .contains(QStringLiteral("2")));
    text->setText(QStringLiteral("SPÄTER FEHLER"));
    const auto pending = editor.draft();
    External external(directory.path());
    external.sql(QStringLiteral(
        "CREATE TRIGGER fail_late BEFORE INSERT ON checklist_steps "
        "WHEN NEW.text='SPÄTER FEHLER' BEGIN SELECT RAISE(ABORT,'synthetisch'); END"));
    editor.save();
    editor.save();
    QVERIFY(QTest::qWaitFor([&] { return failed.size() == 1; }, 5000));
    QCOMPARE(saved.size(), 0);
    QVERIFY(editor.isVisible());
    QVERIFY(editor.draft() == pending);
    QSignalSpy read(&controller, &StorageController::readReady);
    controller.read(original.id, 77);
    QVERIFY(QTest::qWaitFor([&] { return read.size() == 1; }, 5000));
    QVERIFY(qvariant_cast<ProjectDraft>(read.last().at(2)) == old);
    external.sql(QStringLiteral("DROP TRIGGER fail_late"));
    editor.save();
    QVERIFY(QTest::qWaitFor([&] { return saved.size() == 1; }, 5000));
    QVERIFY(!editor.isVisible());
    controller.read(original.id, 78);
    QVERIFY(QTest::qWaitFor([&] { return read.size() == 2; }, 5000));
    QVERIFY(qvariant_cast<ProjectDraft>(read.last().at(2)) == pending);
  }
  void mainCloseProtectsCheckboxAndMainChecklistIsReadOnly() {
    QTemporaryDir directory;
    const auto draft = example();
    {
      SqliteProjectRepository repository(directory.path());
      repository.initialize();
      repository.create(draft);
    }
    StorageController controller(factory(directory.path()));
    MainWindow window(controller, directory.path());
    window.show();
    auto* edit = window.findChild<QPushButton*>(QStringLiteral("editProject"));
    QVERIFY(QTest::qWaitFor([&] { return edit->isEnabled(); }, 5000));
    auto* view = window.findChild<QPlainTextEdit*>(QStringLiteral("selectedChecklist"));
    QVERIFY(view->isReadOnly());
    QVERIFY(view->toPlainText().contains(QStringLiteral("☑  Kanten schleifen")));
    edit->click();
    auto* editor = window.findChild<ProjectEditor*>();
    QVERIFY(editor);
    row(*editor, draft.steps[0].id)->findChild<QCheckBox*>()->setChecked(true);
    bool checked = false;
    answer(checked, true);
    QVERIFY(!window.close());
    QVERIFY(checked);
    checked = false;
    answer(checked);
    QVERIFY(window.close());
    QVERIFY(checked);
    QSignalSpy read(&controller, &StorageController::readReady);
    controller.read(1, 55);
    QVERIFY(QTest::qWaitFor([&] { return read.size() == 1; }, 5000));
    QVERIFY(qvariant_cast<ProjectDraft>(read.first().at(2)) == draft);
  }
  void manyStepsSmallWindowLargeFont() {
    QTemporaryDir directory;
    StorageController controller(factory(directory.path()));
    auto draft = example();
    for (int i = 3; i < 30; ++i)
      draft.steps.append(step(QStringLiteral("Schritt %1").arg(i), i));
    ProjectEditor editor(controller, WoodProject{1, draft, true}, nullptr,
                         QImage::fromData(draft.photo));
    auto font = editor.font();
    font.setPointSize(18);
    editor.setFont(font);
    editor.resize(400, 340);
    editor.show();
    QTest::qWait(30);
    for (const auto* name : {"saveProject", "cancelEdit"}) {
      auto* button = editor.findChild<QPushButton*>(QLatin1String(name));
      QVERIFY(editor.rect().contains(QRect(button->mapTo(&editor, QPoint()), button->size())));
    }
    auto* field = row(editor, draft.steps.last().id)->findChild<QLineEdit*>();
    auto* scroll = editor.findChild<QScrollArea*>();
    scroll->ensureWidgetVisible(field);
    field->setFocus();
    QTest::keyClick(field, Qt::Key_End);
    QTest::keyClicks(field, " Ende");
    QVERIFY(editor.draft().steps.last().text.endsWith(QStringLiteral("Ende")));
    QVERIFY(scroll->viewport()->rect().intersects(
        QRect(field->mapTo(scroll->viewport(), QPoint()), field->size())));
    const auto screenshot = qEnvironmentVariable("HOLZWERKPROJEKT_CHECKLIST_SCREENSHOT");
    if (!screenshot.isEmpty())
      QVERIFY(editor.grab().save(screenshot));
  }
};
QTEST_MAIN(ChecklistTest)
#include "checklist_test.moc"
