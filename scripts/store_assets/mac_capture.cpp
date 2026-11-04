#include "i18n/languages.h"
#include "persistence/sqlite_project_repository.h"
#include "ui/main_window.h"
#include "ui/photo_view.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPixmap>
#include <QPixmapCache>
#include <QPushButton>
#include <QScreen>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>
#include <stdexcept>

// Separate developer executable. It renders the real production widgets with
// synthetic fixtures into native-size images; it is never included in the app.
int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("Woodworking Projects Store Capture"));
  QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));
  app.setQuitOnLastWindowClosed(false);
  try {
    if (argc != 4) {
      throw std::runtime_error("Arguments: language demo-json new-output-directory");
    }
    const QString language = QString::fromLocal8Bit(argv[1]);
    woodwork::Translations translations;
    translations.install(language);
    QFile input(QString::fromLocal8Bit(argv[2]));
    if (!input.open(QIODevice::ReadOnly)) {
      throw std::runtime_error("Cannot read synthetic fixture");
    }
    const auto fixture = QJsonDocument::fromJson(input.readAll()).object();
    const QString output = QString::fromLocal8Bit(argv[3]);
    if (QDir(output).exists() || !QDir().mkpath(output)) {
      throw std::runtime_error("Choose a fresh output directory");
    }
    QTemporaryDir data;
    if (!data.isValid()) {
      throw std::runtime_error("No temporary collection available");
    }
    {
      woodwork::SqliteProjectRepository repository(data.path());
      repository.initialize();
      const auto photos = fixture.value(QStringLiteral("photos")).toArray();
      const QStringList colors{QStringLiteral("#7AAAB3"), QStringLiteral("#B86550"),
                               QStringLiteral("#B9C8B0"), QString(), QStringLiteral("#B58A61")};
      int index = 0;
      for (const auto& item : fixture.value(QStringLiteral("items")).toArray()) {
        const auto fields = item.toArray();
        if (fields.size() != 4) {
          throw std::runtime_error("Invalid synthetic project");
        }
        woodwork::ProjectDraft draft{fields[0].toString(), fields[1].toString(),
                                     fields[2].toString(), fields[3].toString()};
        if (!photos.at(index).isNull()) {
          QFile photo(QFileInfo(input).dir().filePath(photos.at(index).toString()));
          if (!photo.open(QIODevice::ReadOnly)) {
            throw std::runtime_error("Synthetic photo is missing");
          }
          draft.photo = photo.readAll();
        }
        draft.status = woodwork::statusFromStorage(
            fixture.value(QStringLiteral("statuses")).toArray().at(index).toString());
        draft.color = colors.at(index++);
        qint64 stepIndex = 0;
        for (const auto& step : fixture.value(QStringLiteral("steps")).toArray()) {
          draft.steps.append({QUuid::createUuid(), step.toString(), stepIndex == 0, stepIndex});
          ++stepIndex;
        }
        repository.create(draft);
      }
    }
    woodwork::StorageController controller(
        [&data] { return std::make_unique<woodwork::SqliteProjectRepository>(data.path()); });
    woodwork::MainWindow window(controller, data.path(), language);
    window.resize(1280, 800);
    window.move(40, 60);
    window.show();
    window.raise();
    window.activateWindow();
    auto* table = window.findChild<QTableWidget*>(QStringLiteral("projectsTable"));
    auto* search = window.findChild<QLineEdit*>(QStringLiteral("searchProjects"));
    if (!table || !search) {
      throw std::runtime_error("Production widgets missing");
    }
    QElapsedTimer timer;
    timer.start();
    while (table->rowCount() != 5 && timer.elapsed() < 10000) {
      QTest::qWait(50);
    }
    if (table->rowCount() != 5) {
      throw std::runtime_error("Synthetic collection did not load");
    }
    const QString featuredName =
        fixture.value(QStringLiteral("items")).toArray().at(0).toArray().at(0).toString();
    const auto matches = table->findItems(featuredName, Qt::MatchExactly);
    if (matches.size() != 1) {
      throw std::runtime_error("Featured project is missing");
    }
    table->selectRow(matches.at(0)->row());
    const auto capture = [&window, &output](const QString& name) {
      QTest::qWait(350);
      auto* photo = window.findChild<woodwork::PhotoView*>(QStringLiteral("selectedPhoto"));
      if (!photo || photo->pixmap().isNull()) {
        throw std::runtime_error("Featured photo has not loaded");
      }
      auto* add = window.findChild<QPushButton*>(QStringLiteral("addProject"));
      if (!add) {
        throw std::runtime_error("Add button is missing");
      }
      const QRect addBounds(add->mapTo(&window, QPoint()), add->size());
      if (!window.rect().contains(addBounds)) {
        throw std::runtime_error("Add button is clipped in the capture window");
      }
      // Capture our displayed native window. QWidget::grab() can offset native
      // macOS button painting when it renders the widgets into a new pixmap.
      QPixmapCache::clear();
      window.repaint();
      QTest::qWait(800);
      auto image = window.screen()->grabWindow(window.winId());
      if (image.size() != QSize(1280, 800) && image.size() != QSize(2560, 1600)) {
        throw std::runtime_error("Unexpected native screenshot dimensions");
      }
      if (!image.save(QDir(output).filePath(name))) {
        throw std::runtime_error("Could not save screenshot");
      }
    };
    capture(QStringLiteral("01-collection.png"));
    search->setText(fixture.value(QStringLiteral("search")).toString());
    QTest::qWait(250);
    if (table->rowCount() != 1) {
      throw std::runtime_error("Search did not find the expected project");
    }
    table->selectRow(0);
    capture(QStringLiteral("02-search.png"));
    window.close();
    return 0;
  } catch (const std::exception& error) {
    qCritical("%s", error.what());
    return 1;
  }
}
