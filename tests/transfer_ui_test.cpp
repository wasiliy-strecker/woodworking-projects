#include "persistence/json_collection_file.h"
#include "persistence/sqlite_project_repository.h"
#include "ui/main_window.h"

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QtTest>

using namespace woodwork;

namespace {
template <typename Widget> Widget* child(QWidget& parent, const char* name) {
  auto* widget = parent.findChild<Widget*>(QString::fromLatin1(name));
  Q_ASSERT(widget);
  return widget;
}

void chooseFile(const QString& name, const QString& path, bool cancel = false) {
  QTimer::singleShot(0, qApp, [name, path, cancel] {
    auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    if (dialog && name == QStringLiteral("exportCollectionDialog") &&
        dialog->objectName() == QStringLiteral("exportFolderDialog")) {
      if (cancel) {
        dialog->reject();
        return;
      }
      dialog->setDirectory(QFileInfo(path).absolutePath());
      QVERIFY(QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection));
      QTimer::singleShot(0, qApp, [path] {
        auto* filename = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        QVERIFY(filename);
        filename->setTextValue(QFileInfo(path).fileName());
        filename->accept();
      });
      return;
    }
    if (!dialog || dialog->objectName() != name) {
      if (auto* unexpected = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
        unexpected->reject();
      }
      QFAIL("Expected a collection file chooser");
    }
    if (cancel) {
      dialog->reject();
      return;
    }
    dialog->setDirectory(QFileInfo(path).absolutePath());
    dialog->selectFile(QFileInfo(path).fileName());
    QVERIFY(QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection));
  });
}

class ConfirmationAnswer final {
public:
  ConfirmationAnswer(const QString& name, const QString& button) {
    timer_.setInterval(10);
    QObject::connect(&timer_, &QTimer::timeout, [this, name, button] {
      auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
      if (!box || box->objectName() != name) {
        return;
      }
      timer_.stop();
      safeDefault = box->defaultButton() &&
                    box->defaultButton()->objectName().startsWith(QStringLiteral("cancel")) &&
                    box->escapeButton() == box->defaultButton();
      auto* action = box->findChild<QPushButton*>(button);
      answered = action != nullptr;
      if (action) {
        action->click();
      } else {
        box->reject();
      }
    });
    timer_.start();
  }
  bool answered = false;
  bool safeDefault = false;

private:
  QTimer timer_;
};

QByteArray bytes(const QString& path) {
  QFile input(path);
  if (!input.open(QIODevice::ReadOnly)) {
    return {};
  }
  return input.readAll();
}
} // namespace

class TransferUiTest final : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() { QApplication::setAttribute(Qt::AA_DontUseNativeDialogs); }

  void exportAllImportMergeAndKeepSorting() {
    QTemporaryDir data;
    QTemporaryDir output;
    const ProjectDraft alpha{
        QStringLiteral("Alpha"), QStringLiteral("Zulu"), {}, QStringLiteral("Keep note")};
    const ProjectDraft beta{QStringLiteral("Beta"), QStringLiteral("Alpha"), {}, {}};
    const ProjectDraft gamma{QStringLiteral("Gamma"), QStringLiteral("Middle"), {}, {}};
    {
      SqliteProjectRepository repository(data.path());
      repository.initialize();
      repository.create(alpha);
      repository.create(beta);
    }
    StorageController controller(
        [&data] { return std::make_unique<SqliteProjectRepository>(data.path()); });
    QSignalSpy loaded(&controller, &StorageController::loaded);
    QSignalSpy exported(&controller, &StorageController::collectionExported);
    QSignalSpy imported(&controller, &StorageController::collectionImported);
    MainWindow window(controller, data.path());
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }));
    auto* table = child<QTableWidget>(window, "projectsTable");
    auto* search = child<QLineEdit>(window, "searchProjects");
    auto* exportButton = child<QPushButton>(window, "exportCollection");
    auto* importButton = child<QPushButton>(window, "importCollection");
    connect(&controller, &StorageController::inspectCollectionExportRequested, &window,
            [&window] { QVERIFY(window.findChildren<QFileDialog*>().isEmpty()); });
    connect(&controller, &StorageController::readCollectionImportRequested, &window,
            [&window] { QVERIFY(window.findChildren<QFileDialog*>().isEmpty()); });
    table->horizontalHeader()->setSortIndicator(1, Qt::DescendingOrder);
    search->setText(QStringLiteral("Alpha"));
    // Both rows match the text across all fields. The note isolates Alpha.
    search->setText(QStringLiteral("Keep note"));
    QCOMPARE(table->rowCount(), 1);
    const auto selected = table->item(0, 0)->data(Qt::UserRole);
    const auto exportPath = output.filePath(QStringLiteral("complete.json"));
    chooseFile(QStringLiteral("exportCollectionDialog"),
               output.filePath(QStringLiteral("complete")));
    exportButton->click();
    QVERIFY(!importButton->isEnabled());
    QVERIFY(QTest::qWaitFor([&] { return exported.size() == 1; }));
    QCOMPARE(exported.first().first().toLongLong(), 2);
    JsonCollectionFile files(data.path());
    QCOMPARE(files.read(exportPath), (TransferDraftList{alpha, beta}));
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(search->text(), QStringLiteral("Keep note"));

    const auto input = output.filePath(QStringLiteral("incoming.json"));
    files.write({alpha, beta, gamma}, files.inspect(input), false);
    {
      ConfirmationAnswer cancel(QStringLiteral("importConfirmation"),
                                QStringLiteral("cancelImport"));
      chooseFile(QStringLiteral("importCollectionDialog"), input);
      importButton->click();
      QVERIFY(QTest::qWaitFor([&] { return cancel.answered; }));
      QVERIFY(cancel.safeDefault);
      QCOMPARE(imported.size(), 0);
      QCOMPARE(search->text(), QStringLiteral("Keep note"));
      QCOMPARE(table->rowCount(), 1);
    }
    for (int attempt = 0; attempt < 2; ++attempt) {
      ConfirmationAnswer confirm(QStringLiteral("importConfirmation"),
                                 QStringLiteral("confirmImport"));
      chooseFile(QStringLiteral("importCollectionDialog"), input);
      importButton->click();
      QVERIFY(QTest::qWaitFor([&] { return imported.size() == attempt + 1; }));
      QVERIFY(confirm.answered);
      QVERIFY(confirm.safeDefault);
      const auto result = qvariant_cast<CollectionImportResult>(imported.last().first());
      QCOMPARE(result.added, attempt == 0 ? 1 : 0);
      QCOMPARE(result.skipped, attempt == 0 ? 2 : 3);
      QCOMPARE(table->rowCount(), 3);
      QVERIFY(search->text().isEmpty());
      QVERIFY(!child<QPushButton>(window, "clearSearch")->isEnabled());
      for (auto* clearIcon : search->findChildren<QToolButton*>()) {
        QVERIFY(QTest::qWaitFor([clearIcon] { return !clearIcon->isVisible(); }));
      }
      QCOMPARE(table->horizontalHeader()->sortIndicatorSection(), 1);
      QCOMPARE(table->horizontalHeader()->sortIndicatorOrder(), Qt::DescendingOrder);
      QCOMPARE(table->item(0, 0)->text(), QStringLiteral("Alpha"));
      QCOMPARE(table->item(1, 0)->text(), QStringLiteral("Gamma"));
      QCOMPARE(table->item(2, 0)->text(), QStringLiteral("Beta"));
      QCOMPARE(table->item(table->currentRow(), 0)->data(Qt::UserRole), selected);
      QCOMPARE(child<QPlainTextEdit>(window, "selectedNote")->toPlainText(), alpha.note);
    }
    QVERIFY(exportButton->isEnabled());
    QVERIFY(importButton->isEnabled());
  }

  void exportReplacementAndCancellation() {
    QTemporaryDir data;
    QTemporaryDir output;
    JsonCollectionFile files(data.path());
    const auto path = output.filePath(QStringLiteral("backup.json"));
    files.write({{QStringLiteral("Old backup"), {}, {}, {}}}, files.inspect(path), false);
    const auto original = bytes(path);
    StorageController controller(
        [&data] { return std::make_unique<SqliteProjectRepository>(data.path()); });
    QSignalSpy loaded(&controller, &StorageController::loaded);
    QSignalSpy exported(&controller, &StorageController::collectionExported);
    MainWindow window(controller, data.path());
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }));
    auto* button = child<QPushButton>(window, "exportCollection");
    chooseFile(QStringLiteral("exportCollectionDialog"), path, true);
    button->click();
    QVERIFY(button->isEnabled());
    QCOMPARE(bytes(path), original);
    {
      ConfirmationAnswer cancel(QStringLiteral("replaceExportConfirmation"),
                                QStringLiteral("cancelReplaceExport"));
      chooseFile(QStringLiteral("exportCollectionDialog"), path);
      button->click();
      QVERIFY(QTest::qWaitFor([&] { return cancel.answered; }));
      QVERIFY(cancel.safeDefault);
      QCOMPARE(bytes(path), original);
      QCOMPARE(exported.size(), 0);
    }
    ConfirmationAnswer replace(QStringLiteral("replaceExportConfirmation"),
                               QStringLiteral("confirmReplaceExport"));
    chooseFile(QStringLiteral("exportCollectionDialog"), path);
    button->click();
    QVERIFY(QTest::qWaitFor([&] { return exported.size() == 1; }));
    QVERIFY(replace.answered);
    QVERIFY(replace.safeDefault);
    QVERIFY(files.read(path).isEmpty());
  }

  void failuresAndEditorKeepActionsSafe() {
    QTemporaryDir data;
    QTemporaryDir output;
    const auto invalid = output.filePath(QStringLiteral("invalid.json"));
    {
      QFile file(invalid);
      QVERIFY(file.open(QIODevice::WriteOnly));
      file.write("invalid json");
    }
    StorageController controller(
        [&data] { return std::make_unique<SqliteProjectRepository>(data.path()); });
    QSignalSpy loaded(&controller, &StorageController::loaded);
    QSignalSpy failed(&controller, &StorageController::failed);
    MainWindow window(controller, data.path());
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }));
    auto* importButton = child<QPushButton>(window, "importCollection");
    auto* exportButton = child<QPushButton>(window, "exportCollection");
    chooseFile(QStringLiteral("importCollectionDialog"), invalid, true);
    importButton->click();
    QVERIFY(importButton->isEnabled());
    QCOMPARE(failed.size(), 0);
    chooseFile(QStringLiteral("importCollectionDialog"), invalid);
    importButton->click();
    QVERIFY(QTest::qWaitFor([&] { return failed.size() == 1; }));
    QVERIFY(importButton->isEnabled());
    QVERIFY(exportButton->isEnabled());
    QCOMPARE(child<QTableWidget>(window, "projectsTable")->rowCount(), 0);
    QVERIFY(!child<QLabel>(window, "mainFeedback")->text().isEmpty());
    chooseFile(QStringLiteral("exportCollectionDialog"),
               data.filePath(QStringLiteral("forbidden.json")));
    exportButton->click();
    QVERIFY(QTest::qWaitFor([&] { return failed.size() == 2; }));
    QVERIFY(!QFileInfo::exists(data.filePath(QStringLiteral("forbidden.json"))));
    QVERIFY(exportButton->isEnabled());
    child<QPushButton>(window, "addProject")->click();
    auto* editor = window.findChild<ProjectEditor*>();
    QVERIFY(editor);
    QVERIFY(!importButton->isEnabled());
    QVERIFY(!exportButton->isEnabled());
    editor->reject();
    QVERIFY(exportButton->isEnabled());
  }

  void aboutCloseButtonIsRedAndClosesDialog() {
    QTemporaryDir data;
    StorageController controller(
        [&data] { return std::make_unique<SqliteProjectRepository>(data.path()); });
    QSignalSpy loaded(&controller, &StorageController::loaded);
    MainWindow window(controller, data.path());
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }));
    bool inspected = false;
    QTimer::singleShot(0, qApp, [&inspected] {
      auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      QVERIFY(dialog);
      auto* close = child<QPushButton>(*dialog, "closeAbout");
      const auto background = close->grab().toImage().pixelColor(8, close->height() / 2);
      inspected = background.red() > 120 && background.green() < 100 && background.blue() < 100;
      const auto screenshot = qEnvironmentVariable("BRETTSPIELREGAL_ABOUT_SCREENSHOT");
      if (!screenshot.isEmpty()) {
        QVERIFY(dialog->grab().save(screenshot));
      }
      close->click();
    });
    child<QAction>(window, "showAbout")->trigger();
    QVERIFY(inspected);
    QVERIFY(!QApplication::activeModalWidget());
  }
};

QTEST_MAIN(TransferUiTest)
#include "transfer_ui_test.moc"
