#include "application/photo_importer.h"
#include "persistence/sqlite_project_repository.h"
#include "ui/main_window.h"
#include "ui/photo_view.h"

#include <QApplication>
#include <QBuffer>
#include <QComboBox>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QtTest>
#include <atomic>

using namespace woodwork;
namespace {
QByteArray picture(QColor color) {
  QImage image(64, 48, QImage::Format_RGB32);
  image.fill(color);
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");
  return bytes;
}
void writePhoto(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size())
    throw std::runtime_error("Synthetic image could not be written");
}
void answer(bool* checked, bool keep = false) {
  QTimer::singleShot(0, qApp, [checked, keep] {
    auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (!box)
      return;
    *checked = box->defaultButton()->objectName() == QStringLiteral("keepEditing");
    box->findChild<QPushButton*>(keep ? QStringLiteral("keepEditing")
                                      : QStringLiteral("discardChanges"))
        ->click();
  });
}
RepositoryFactory factory(const QString& directory) {
  return [directory] { return std::make_unique<SqliteProjectRepository>(directory); };
}
class SlowRepository final : public ProjectRepository {
public:
  TransferDraftList transferSnapshot() override {
    throw std::logic_error("Unexpected test transfer");
  }
  CollectionImportResult importEntries(const TransferDraftList&) override {
    throw std::logic_error("Unexpected test transfer");
  }
  SlowRepository(const QString& path, std::shared_ptr<std::atomic<int>> reads)
      : inner_(path), reads_(std::move(reads)) {}
  void initialize() override { inner_.initialize(); }
  ProjectList findAll() override { return inner_.findAll(); }
  ProjectDraft find(qint64 id) override {
    if (QThread::currentThread() == qApp->thread())
      throw StorageError(QStringLiteral("Falscher Thread"));
    ++(*reads_);
    QThread::msleep(180);
    return inner_.find(id);
  }
  WoodProject create(const ProjectDraft& draft) override { return inner_.create(draft); }
  WoodProject update(qint64 id, const ProjectDraft& draft) override {
    return inner_.update(id, draft);
  }
  void remove(qint64 id) override { inner_.remove(id); }

private:
  SqliteProjectRepository inner_;
  std::shared_ptr<std::atomic<int>> reads_;
};
} // namespace
class PhotoDialogsTest final : public QObject {
  Q_OBJECT
private slots:
  void photoOnlyDirtyDraft_data() {
    QTest::addColumn<int>("action");
    QTest::newRow("add") << 0;
    QTest::newRow("replace-same-path") << 1;
    QTest::newRow("remove") << 2;
  }
  void photoOnlyDirtyDraft() {
    QFETCH(int, action);
    QTemporaryDir directory;
    const auto originalBytes = action == 0 ? QByteArray() : picture(Qt::red);
    ProjectDraft draft{QStringLiteral("Modell"), {}, {}, {}, ProjectStatus::Started, originalBytes};
    WoodProject original;
    {
      SqliteProjectRepository repo(directory.path());
      repo.initialize();
      original = repo.create(draft);
    }
    original.fields = draft;
    StorageController controller(factory(directory.path()));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }, 5000));
    ProjectEditor editor(controller, original, nullptr, QImage::fromData(originalBytes));
    editor.show();
    if (action == 2)
      editor.findChild<QPushButton*>(QStringLiteral("removePhoto"))->click();
    else {
      const auto path = directory.filePath(QStringLiteral("same-name.png"));
      writePhoto(path, picture(Qt::blue));
      editor.importFromPath(path);
      QVERIFY(!editor.requestClose());
      editor.save(); // Import locks competing save actions.
      QVERIFY(QTest::qWaitFor([&] { return !controller.isBusy(); }, 5000));
    }
    QVERIFY(editor.draft().photo != originalBytes);
    bool checked = false;
    answer(&checked, true);
    QVERIFY(!editor.requestClose());
    QVERIFY(checked);
    checked = false;
    answer(&checked);
    QVERIFY(editor.requestClose());
    QVERIFY(checked);
    QSignalSpy detail(&controller, &StorageController::readReady);
    controller.read(original.id, 23);
    QVERIFY(QTest::qWaitFor([&] { return detail.size() == 1; }, 5000));
    QVERIFY(qvariant_cast<ProjectDraft>(detail.first().at(2)) == draft);
  }
  void invalidImportAndCancelledPickerPreserveFullDraft() {
    QTemporaryDir directory;
    const auto bytes = picture(Qt::red);
    const ProjectDraft draft{QStringLiteral("\u00a0"), QStringLiteral("Material"),
                             QStringLiteral("Ort"),    QStringLiteral("Notiz\n"),
                             ProjectStatus::Finished,  bytes};
    StorageController controller(factory(directory.path()));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    QSignalSpy failed(&controller, &StorageController::failed);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }, 5000));
    ProjectEditor editor(controller, WoodProject{0, draft, true}, nullptr, QImage::fromData(bytes));
    editor.show();
    const auto before = editor.draft();
    editor.save();
    QVERIFY(editor.draft() == before);
    QVERIFY(editor.findChild<QLabel*>(QStringLiteral("editorError"))->isVisible());
    const auto path = directory.filePath(QStringLiteral("bad.jpg"));
    writePhoto(path, QByteArray("broken"));
    editor.importFromPath(path);
    QVERIFY(QTest::qWaitFor([&] { return failed.size() == 1; }, 5000));
    QVERIFY(editor.draft() == before);
    editor.importFromPath({});
    QVERIFY(editor.draft() == before);
    bool pickerSeen = false;
    QTimer::singleShot(0, qApp, [&pickerSeen] {
      auto* picker = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
      if (picker) {
        pickerSeen = true;
        picker->reject();
      }
    });
    editor.findChild<QPushButton*>(QStringLiteral("choosePhoto"))->click();
    QVERIFY(pickerSeen);
    QVERIFY(editor.draft() == before);
  }
  void completelyRestoredPhotoTextStatusCloseWithoutQuestion() {
    QTemporaryDir directory;
    const auto bytes = picture(Qt::red);
    const ProjectDraft draft{QStringLiteral("Original"), {}, {}, {}, ProjectStatus::Started, bytes};
    StorageController controller(factory(directory.path()));
    ProjectEditor editor(controller, WoodProject{7, draft, true}, nullptr, QImage::fromData(bytes));
    editor.show();
    const auto path = directory.filePath(QStringLiteral("unchanged-name.png"));
    writePhoto(path, picture(Qt::blue));
    editor.importFromPath(path);
    QVERIFY(QTest::qWaitFor([&] { return !controller.isBusy(); }, 5000));
    QVERIFY(editor.draft().photo != bytes);
    editor.findChild<QPushButton*>(QStringLiteral("removePhoto"))->click();
    writePhoto(path, bytes);
    editor.importFromPath(path);
    QVERIFY(QTest::qWaitFor([&] { return !controller.isBusy(); }, 5000));
    editor.findChild<QLineEdit*>(QStringLiteral("projectName"))->setText(QStringLiteral("Neu"));
    editor.findChild<QComboBox*>(QStringLiteral("projectStatus"))->setCurrentIndex(2);
    editor.findChild<QLineEdit*>(QStringLiteral("projectName"))->setText(draft.name);
    editor.findChild<QComboBox*>(QStringLiteral("projectStatus"))->setCurrentIndex(1);
    QVERIFY(editor.draft() == draft);
    QVERIFY(editor.requestClose());
  }
  void stalePreviewAndDestroyedWindows() {
    QTemporaryDir directory;
    {
      SqliteProjectRepository repo(directory.path());
      repo.initialize();
      repo.create({QStringLiteral("A Rot"), {}, {}, {}, ProjectStatus::Started, picture(Qt::red)});
      repo.create(
          {QStringLiteral("B Blau"), {}, {}, {}, ProjectStatus::Finished, picture(Qt::blue)});
      repo.create({QStringLiteral("C Ohne"), {}, {}, {}});
    }
    auto reads = std::make_shared<std::atomic<int>>(0);
    StorageController controller(
        [path = directory.path(), reads] { return std::make_unique<SlowRepository>(path, reads); });
    QSignalSpy loaded(&controller, &StorageController::loaded);
    QSignalSpy ready(&controller, &StorageController::readReady);
    auto window = std::make_unique<MainWindow>(controller, directory.path());
    window->show();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }, 5000));
    auto* table = window->findChild<QTableWidget*>(QStringLiteral("projectsTable"));
    table->selectRow(1);
    table->selectRow(2);
    bool responsive = false;
    QTimer::singleShot(10, qApp, [&responsive] { responsive = true; });
    QVERIFY(QTest::qWaitFor([&] { return responsive; }, 100));
    QVERIFY(QTest::qWaitFor([&] { return ready.size() >= 3; }, 5000));
    auto* photo = window->findChild<PhotoView*>(QStringLiteral("selectedPhoto"));
    QCOMPARE(photo->text(), QStringLiteral("Kein Foto"));
    table->selectRow(1);
    QVERIFY(QTest::qWaitFor([&] { return ready.size() >= 4; }, 5000));
    QVERIFY(!photo->pixmap().isNull());
    const auto image = photo->pixmap().toImage();
    QCOMPARE(image.pixelColor(image.width() / 2, image.height() / 2), QColor(Qt::blue));
    table->selectRow(0);
    window.reset();
    QVERIFY(QTest::qWaitFor([&] { return ready.size() >= 5; }, 5000));
    QVERIFY(reads->load() >= 5);
    auto editor = std::make_unique<ProjectEditor>(controller, std::nullopt);
    const auto path = directory.filePath(QStringLiteral("late.png"));
    writePhoto(path, picture(Qt::red));
    QSignalSpy imported(&controller, &StorageController::imported);
    editor->importFromPath(path);
    editor.reset();
    QVERIFY(QTest::qWaitFor([&] { return imported.size() == 1; }, 5000));
    ProjectEditor fresh(controller, std::nullopt);
    QVERIFY(fresh.draft().photo.isEmpty());
  }
  void corruptStoredPhotoNeverOpensEmptyEditor() {
    QTemporaryDir directory;
    {
      SqliteProjectRepository repo(directory.path());
      repo.initialize();
      repo.create({QStringLiteral("Beschädigtes Foto"),
                   {},
                   {},
                   {},
                   ProjectStatus::Started,
                   picture(Qt::red)});
    }
    const auto connection = QUuid::createUuid().toString();
    {
      auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
      database.setDatabaseName(directory.filePath(QStringLiteral("holzwerkprojekt.sqlite3")));
      QVERIFY(database.open());
      QSqlQuery query(database);
      QVERIFY(query.exec(QStringLiteral("UPDATE projects SET photo=x'62726f6b656e'")));
    }
    QSqlDatabase::removeDatabase(connection);
    StorageController controller(factory(directory.path()));
    QSignalSpy failed(&controller, &StorageController::readFailed);
    MainWindow window(controller, directory.path());
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return failed.size() == 1; }, 5000));
    auto* edit = window.findChild<QPushButton*>(QStringLiteral("editProject"));
    QVERIFY(!edit->isEnabled());
    QTest::keyClick(window.findChild<QTableWidget*>(QStringLiteral("projectsTable")),
                    Qt::Key_Return);
    QVERIFY(!window.findChild<ProjectEditor*>());
    QVERIFY(window.findChild<PhotoView*>(QStringLiteral("selectedPhoto"))
                ->text()
                .contains(QStringLiteral("nicht geladen")));
  }
  void mainWindowProtectsStatusOnlyDraft() {
    QTemporaryDir directory;
    StorageController controller(factory(directory.path()));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    MainWindow window(controller, directory.path());
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }, 5000));
    window.findChild<QPushButton*>(QStringLiteral("addProject"))->click();
    auto* editor = window.findChild<ProjectEditor*>();
    QVERIFY(editor);
    editor->findChild<QComboBox*>(QStringLiteral("projectStatus"))->setCurrentIndex(2);
    bool checked = false;
    answer(&checked, true);
    QVERIFY(!window.close());
    QVERIFY(checked);
    QVERIFY(editor->isVisible());
    checked = false;
    answer(&checked);
    QVERIFY(window.close());
    QVERIFY(checked);
  }
};
QTEST_MAIN(PhotoDialogsTest)
#include "photo_dialogs_test.moc"
