#include "persistence/sqlite_project_repository.h"
#include "ui/main_window.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollArea>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <atomic>
#include <memory>

using namespace woodwork;

namespace {
struct Controls {
  std::atomic<bool> failWrites{false};
  std::atomic<int> delay{0};
  std::atomic<int> writes{0};
  std::atomic<bool> wrongThread{false};
  std::atomic<bool> destroyed{false};
};

class ControlledRepository final : public ProjectRepository {
public:
  ControlledRepository(const QString& directory, std::shared_ptr<Controls> controls)
      : repository_(directory), controls_(std::move(controls)), thread_(QThread::currentThread()) {
    if (thread_ == qApp->thread()) {
      controls_->wrongThread = true;
    }
  }
  ~ControlledRepository() override {
    checkThread();
    controls_->destroyed = true;
  }
  void initialize() override {
    checkThread();
    repository_.initialize();
  }
  ProjectList findAll() override {
    checkThread();
    return repository_.findAll();
  }
  ProjectDraft find(qint64 id) override {
    checkThread();
    return repository_.find(id);
  }
  WoodProject create(const ProjectDraft& draft) override {
    beforeWrite();
    auto result = repository_.create(draft);
    ++controls_->writes;
    return result;
  }
  WoodProject update(qint64 id, const ProjectDraft& draft) override {
    beforeWrite();
    auto result = repository_.update(id, draft);
    ++controls_->writes;
    return result;
  }
  void remove(qint64 id) override {
    beforeWrite();
    repository_.remove(id);
    ++controls_->writes;
  }

private:
  void checkThread() {
    if (QThread::currentThread() != thread_) {
      controls_->wrongThread = true;
    }
  }
  void beforeWrite() {
    checkThread();
    QThread::msleep(static_cast<unsigned long>(controls_->delay.load()));
    if (controls_->failWrites) {
      throw StorageError(QStringLiteral("Synthetischer Speicherfehler. Bitte erneut versuchen."));
    }
  }
  SqliteProjectRepository repository_;
  std::shared_ptr<Controls> controls_;
  QThread* thread_;
};

RepositoryFactory factory(const QString& directory, const std::shared_ptr<Controls>& controls) {
  return
      [directory, controls] { return std::make_unique<ControlledRepository>(directory, controls); };
}

template <typename Widget> Widget* child(QWidget& parent, const char* name) {
  auto* result = parent.findChild<Widget*>(QString::fromLatin1(name));
  Q_ASSERT(result);
  return result;
}

void fill(ProjectEditor& editor, const QString& name = QStringLiteral("Azul")) {
  child<QLineEdit>(editor, "projectName")->setText(name);
  child<QLineEdit>(editor, "projectMaterial")->setText(QStringLiteral("Müller"));
  child<QLineEdit>(editor, "projectLocation")->setText(QStringLiteral("Regal 2"));
  child<QPlainTextEdit>(editor, "projectNote")
      ->setPlainText(QStringLiteral("  Erste Zeile\n\nGeschenk von Ömer.\n"));
}

void answerConfirmation(const QString& objectName, const QString& buttonName,
                        const QString& expectedDefault, bool* checked) {
  QTimer::singleShot(0, qApp, [objectName, buttonName, expectedDefault, checked] {
    auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (!box || box->objectName() != objectName) {
      return;
    }
    auto* button = box->findChild<QPushButton*>(buttonName);
    *checked = box->defaultButton() && box->defaultButton()->objectName() == expectedDefault &&
               box->escapeButton() == box->defaultButton();
    if (button) {
      button->click();
    }
  });
}
} // namespace

class DialogsTest final : public QObject {
  Q_OBJECT
private slots:
  void invalidInputKeepsFullDraft() {
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    QVERIFY(controller.load());
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
    ProjectEditor editor(controller, std::nullopt);
    editor.show();
    fill(editor, QStringLiteral("\u00a0\u202f"));
    const auto before = editor.draft();
    QTest::mouseClick(child<QPushButton>(editor, "saveProject"), Qt::LeftButton);
    QVERIFY(editor.isVisible());
    QVERIFY(editor.draft() == before);
    QVERIFY(child<QLabel>(editor, "editorError")->isVisible());
    QCOMPARE(controls->writes.load(), 0);
  }
  void enterInNoteAndSaveShortcut() {
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
    QSignalSpy saved(&controller, &StorageController::saved);
    ProjectEditor editor(controller, std::nullopt);
    editor.show();
    fill(editor);
    auto* note = child<QPlainTextEdit>(editor, "projectNote");
    note->setPlainText(QStringLiteral("A"));
    note->moveCursor(QTextCursor::End);
    note->setFocus();
    QTest::keyClick(note, Qt::Key_Return);
    QTest::keyClicks(note, "B");
    QCOMPARE(note->toPlainText(), QStringLiteral("A\nB"));
    QCOMPARE(saved.size(), 0);
    QVERIFY(QTest::qWaitForWindowActive(&editor));
    QTest::keySequence(&editor, QKeySequence::Save);
    QVERIFY(QTest::qWaitFor([&] { return (saved.size()) == (1); }, 5000));
    QVERIFY(!editor.isVisible());
    QCOMPARE(qvariant_cast<WoodProject>(saved.first().first()).fields.note, QStringLiteral("A\nB"));
  }
  void dirtyCloseProtection_data() {
    QTest::addColumn<int>("action");
    QTest::newRow("cancel") << 0;
    QTest::newRow("escape") << 1;
    QTest::newRow("window-close") << 2;
  }
  void dirtyCloseProtection() {
    QFETCH(int, action);
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
    ProjectEditor editor(controller, std::nullopt);
    editor.show();
    fill(editor);
    const auto draft = editor.draft();
    const auto close = [&] {
      if (action == 0) {
        child<QPushButton>(editor, "cancelEdit")->click();
      } else if (action == 1) {
        QTest::keyClick(&editor, Qt::Key_Escape);
      } else {
        editor.close();
      }
    };
    bool checked = false;
    answerConfirmation(QStringLiteral("discardConfirmation"), QStringLiteral("keepEditing"),
                       QStringLiteral("keepEditing"), &checked);
    close();
    QVERIFY(checked);
    QVERIFY(editor.isVisible());
    QVERIFY(editor.draft() == draft);
    checked = false;
    answerConfirmation(QStringLiteral("discardConfirmation"), QStringLiteral("discardChanges"),
                       QStringLiteral("keepEditing"), &checked);
    close();
    QVERIFY(checked);
    QVERIFY(!editor.isVisible());
    QCOMPARE(controls->writes.load(), 0);
  }
  void unchangedEditorClosesImmediately() {
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    ProjectEditor editor(controller, WoodProject{9, {QStringLiteral("Azul"), {}, {}, {}}});
    editor.show();
    child<QLineEdit>(editor, "projectName")->setText(QStringLiteral("Änderung"));
    child<QLineEdit>(editor, "projectName")->setText(QStringLiteral("Azul"));
    QVERIFY(editor.requestClose());
    QVERIFY(!editor.isVisible());
  }
  void failedSavePreservesDraftAndRetries() {
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
    QSignalSpy failed(&controller, &StorageController::failed);
    QSignalSpy saved(&controller, &StorageController::saved);
    ProjectEditor editor(controller, std::nullopt);
    editor.show();
    fill(editor);
    const auto draft = editor.draft();
    controls->failWrites = true;
    editor.save();
    QVERIFY(QTest::qWaitFor([&] { return (failed.size()) == (1); }, 5000));
    QVERIFY(editor.isVisible());
    QVERIFY(editor.draft() == draft);
    QVERIFY(child<QPushButton>(editor, "saveProject")->isEnabled());
    QVERIFY(child<QLabel>(editor, "editorError")->isVisible());
    QCOMPARE(saved.size(), 0);
    QCOMPARE(controls->writes.load(), 0);
    controls->failWrites = false;
    editor.save();
    editor.save();
    QVERIFY(QTest::qWaitFor([&] { return (saved.size()) == (1); }, 5000));
    QCOMPARE(controls->writes.load(), 1);
    QVERIFY(!editor.isVisible());
  }
  void backgroundWorkAndTeardown() {
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    {
      StorageController controller(factory(directory.path(), controls));
      QSignalSpy loaded(&controller, &StorageController::loaded);
      controller.load();
      QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
      auto editor = std::make_unique<ProjectEditor>(controller, std::nullopt);
      editor->show();
      fill(*editor);
      controls->delay = 300;
      editor->save();
      QVERIFY(!editor->requestClose());
      QVERIFY(!controller.remove(1));
      bool responsive = false;
      QTimer::singleShot(10, [&responsive] { responsive = true; });
      QVERIFY(QTest::qWaitFor([&] { return responsive; }, 5000));
      QVERIFY(controller.isBusy());
      editor.reset(); // A late success must not call a destroyed dialog.
      QVERIFY(QTest::qWaitFor([&] { return !controller.isBusy(); }, 5000));
    }
    QVERIFY(controls->destroyed);
    QVERIFY(!controls->wrongThread);
  }
  void listSelectionFilteringAndDeletion() {
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    MainWindow window(controller, directory.path());
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
    QVERIFY(child<QLabel>(window, "collectionState")->text().contains(QStringLiteral("noch leer")));
    auto* add = child<QPushButton>(window, "addProject");
    add->click();
    auto* editor = window.findChild<ProjectEditor*>();
    QVERIFY(editor);
    fill(*editor);
    editor->save();
    auto* table = child<QTableWidget>(window, "projectsTable");
    QVERIFY(QTest::qWaitFor([&] { return (table->rowCount()) == (1); }, 5000));
    QVERIFY(QTest::qWaitFor([&] { return add->isEnabled(); }, 5000));
    const auto id = table->item(0, 0)->data(Qt::UserRole).toLongLong();
    auto* search = child<QLineEdit>(window, "searchProjects");
    search->setText(QStringLiteral("AZUL"));
    QVERIFY(QTest::qWaitFor([&] { return child<QPushButton>(window, "editProject")->isEnabled(); },
                            5000));
    child<QPushButton>(window, "editProject")->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    editor = window.findChild<ProjectEditor*>();
    QVERIFY(editor);
    child<QLineEdit>(*editor, "projectName")->setText(QStringLiteral("Catan"));
    editor->save();
    QVERIFY(QTest::qWaitFor([&] { return (table->rowCount()) == (0); }, 5000));
    QVERIFY(child<QLabel>(window, "mainFeedback")
                ->text()
                .contains(QStringLiteral("wurde gespeichert, passt aber nicht")));
    QVERIFY(child<QLabel>(window, "collectionState")
                ->text()
                .contains(QStringLiteral("Keine Projekte")));
    search->clear();
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 0)->data(Qt::UserRole).toLongLong(), id);
    QVERIFY(QTest::qWaitFor(
        [&] { return child<QPushButton>(window, "removeProject")->isEnabled(); }, 5000));
    bool checked = false;
    answerConfirmation(QStringLiteral("deleteConfirmation"), QStringLiteral("cancelDelete"),
                       QStringLiteral("cancelDelete"), &checked);
    child<QPushButton>(window, "removeProject")->click();
    QVERIFY(checked);
    QCOMPARE(table->rowCount(), 1);
    checked = false;
    answerConfirmation(QStringLiteral("deleteConfirmation"), QStringLiteral("confirmDelete"),
                       QStringLiteral("cancelDelete"), &checked);
    child<QPushButton>(window, "removeProject")->click();
    QVERIFY(checked);
    QVERIFY(QTest::qWaitFor([&] { return (table->rowCount()) == (0); }, 5000));
    QVERIFY(child<QLabel>(window, "collectionState")->text().contains(QStringLiteral("noch leer")));
  }
  void mainWindowProtectsDraftAndBusySave() {
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    MainWindow window(controller, directory.path());
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
    child<QPushButton>(window, "addProject")->click();
    auto* editor = window.findChild<ProjectEditor*>();
    QVERIFY(editor);
    fill(*editor);
    bool checked = false;
    answerConfirmation(QStringLiteral("discardConfirmation"), QStringLiteral("keepEditing"),
                       QStringLiteral("keepEditing"), &checked);
    QVERIFY(!window.close());
    QVERIFY(checked);
    QVERIFY(editor->isVisible());
    controls->delay = 200;
    editor->save();
    QVERIFY(!window.close());
    QVERIFY(QTest::qWaitFor([&] { return !controller.isBusy(); }, 5000));
    QVERIFY(window.close());
  }
  void startupErrorIsNotAnEmptyCollection() {
    QTemporaryDir directory;
    class FailingRepository final : public ProjectRepository {
    public:
      void initialize() override { throw StorageError(QStringLiteral("Synthetischer Pfadfehler")); }
      ProjectList findAll() override { return {}; }
      ProjectDraft find(qint64) override { return {}; }
      WoodProject create(const ProjectDraft&) override { return {}; }
      WoodProject update(qint64, const ProjectDraft&) override { return {}; }
      void remove(qint64) override {}
    };
    StorageController failing([] { return std::make_unique<FailingRepository>(); });
    QSignalSpy failed(&failing, &StorageController::failed);
    MainWindow window(failing, directory.path());
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return (failed.size()) == (1); }, 5000));
    QVERIFY(child<QLabel>(window, "collectionState")
                ->text()
                .contains(QStringLiteral("nicht geöffnet")));
    QVERIFY(!child<QPushButton>(window, "addProject")->isEnabled());
    QVERIFY(child<QPushButton>(window, "reloadProjects")->isVisible());
  }
  void discardedEditLeavesStoredOriginal() {
    QTemporaryDir directory;
    WoodProject original;
    {
      SqliteProjectRepository repository(directory.path());
      repository.initialize();
      original = repository.create({QStringLiteral("Original"), {}, {}, QStringLiteral("Notiz")});
    }
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }, 5000));
    ProjectEditor editor(controller, original);
    editor.show();
    fill(editor, QStringLiteral("Geändert"));
    bool checked = false;
    answerConfirmation(QStringLiteral("discardConfirmation"), QStringLiteral("discardChanges"),
                       QStringLiteral("keepEditing"), &checked);
    QVERIFY(editor.requestClose());
    QVERIFY(checked);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 2; }, 5000));
    const auto projects = qvariant_cast<ProjectList>(loaded.last().first());
    QVERIFY(projects == ProjectList{original});
  }
  void failedDeletePreservesRowAndSelection() {
    QTemporaryDir directory;
    {
      SqliteProjectRepository repository(directory.path());
      repository.initialize();
      repository.create({QStringLiteral("Original"), {}, {}, {}});
    }
    auto controls = std::make_shared<Controls>();
    controls->failWrites = true;
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    QSignalSpy failed(&controller, &StorageController::failed);
    MainWindow window(controller, directory.path());
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }, 5000));
    auto* table = child<QTableWidget>(window, "projectsTable");
    const auto id = table->item(0, 0)->data(Qt::UserRole);
    bool checked = false;
    answerConfirmation(QStringLiteral("deleteConfirmation"), QStringLiteral("confirmDelete"),
                       QStringLiteral("cancelDelete"), &checked);
    child<QPushButton>(window, "removeProject")->click();
    QVERIFY(QTest::qWaitFor([&] { return failed.size() == 1; }, 5000));
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->currentRow(), 0);
    QCOMPARE(table->item(0, 0)->data(Qt::UserRole), id);
    QCOMPARE(controls->writes.load(), 0);
  }
  void largeFontSmallDialogKeepsActionsReachable() {
    const auto originalFont = QApplication::font();
    const auto restoreFont = qScopeGuard([originalFont] { QApplication::setFont(originalFont); });
    auto largerFont = originalFont;
    largerFont.setPointSize(18);
    QApplication::setFont(largerFont);
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }, 5000));
    ProjectEditor editor(controller, std::nullopt);
    editor.resize(400, 340);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.save();
    auto* save = child<QPushButton>(editor, "saveProject");
    auto* cancel = child<QPushButton>(editor, "cancelEdit");
    for (auto* button : {save, cancel}) {
      QVERIFY(button->isVisible());
      const QRect bounds(button->mapTo(&editor, QPoint(0, 0)), button->size());
      QVERIFY(editor.rect().contains(bounds));
    }
    auto* scroll = editor.findChild<QScrollArea*>();
    QVERIFY(scroll);
    QVERIFY(scroll->widget()->height() > scroll->viewport()->height());
    const auto screenshot = qEnvironmentVariable("HOLZWERKPROJEKT_TEST_SCREENSHOT");
    if (!screenshot.isEmpty()) {
      QVERIFY(editor.grab().save(screenshot));
    }
  }
};

QTEST_MAIN(DialogsTest)
#include "dialogs_test.moc"
