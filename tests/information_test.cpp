#include "i18n/languages.h"
#include "i18n/privacy_notice.h"
#include "persistence/language_preferences.h"
#include "persistence/sqlite_project_repository.h"
#include "ui/main_window.h"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

using namespace woodwork;

namespace {
QString translated(const char* source) { return QCoreApplication::translate("App", source); }
QByteArray readBytes(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.readAll();
}
} // namespace

class InformationTest final : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() { QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0")); }
  void separateInformationPages_data() {
    QTest::addColumn<QString>("tag");
    for (const auto& tag : supportedLanguages()) {
      QTest::newRow(qPrintable(tag)) << tag;
    }
  }
  void separateInformationPages() {
    QFETCH(QString, tag);
    Translations translations;
    translations.install(tag);
    QTemporaryDir data;
    saveLanguagePreference(data.path(), tag);
    StorageController controller(
        [&data] { return std::make_unique<SqliteProjectRepository>(data.path()); });
    QSignalSpy loaded(&controller, &StorageController::loaded);
    MainWindow window(controller, data.path(), tag);
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }));
    QCOMPARE(window.windowTitle(), QStringLiteral("Woodworking Projects"));
    auto* info = window.findChild<QPushButton*>(QStringLiteral("aboutApp"));
    QVERIFY(info);
    QVERIFY(info->menu());
    const auto actions = info->menu()->actions();
    QCOMPARE(actions.size(), 3);
    QCOMPARE(actions[0]->text(), translated("Über die App"));
    QCOMPARE(actions[1]->text(), translated("Datenschutz"));
    QCOMPARE(actions[2]->text(), translated("Lizenzen"));
    const auto databasePath = QDir(data.path()).filePath(QStringLiteral("holzwerkprojekt.sqlite3"));
    const auto preferencePath = QDir(data.path()).filePath(QStringLiteral("language.txt"));
    const auto database = readBytes(databasePath);
    const auto preference = readBytes(preferencePath);
    QVERIFY(!database.isEmpty());
    const QStringList names{QStringLiteral("About"), QStringLiteral("Privacy"),
                            QStringLiteral("Licenses")};
    for (int page = 0; page < 3; ++page) {
      info->menu()->popup(info->mapToGlobal(QPoint(0, info->height())));
      QVERIFY(QTest::qWaitForWindowExposed(info->menu()));
      info->menu()->setActiveAction(nullptr);
      for (int step = 0; step <= page; ++step) {
        QTest::keyClick(info->menu(), Qt::Key_Down);
      }
      QCOMPARE(info->menu()->activeAction(), actions[page]);
      bool inspected = false;
      QTimer inspection;
      inspection.setInterval(10);
      connect(&inspection, &QTimer::timeout, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog) {
          return;
        }
        inspection.stop();
        const auto name = names[page];
        auto* text = dialog->findChild<QPlainTextEdit*>(name.toLower() + QStringLiteral("Text"));
        auto* close = dialog->findChild<QPushButton*>(QStringLiteral("close") + name);
        if (!text || !close) {
          dialog->reject();
          QFAIL("Missing text or close action");
        }
        const auto content = text->toPlainText();
        bool correct = text->isReadOnly() && close->text() == translated("Schließen");
        if (page == 0) {
          correct &= content.startsWith(QStringLiteral("Woodworking Projects\n")) &&
                     content.contains(QStringLiteral("0.1.0")) && content.contains(data.path()) &&
                     !content.contains(QStringLiteral("LGPL"));
        } else if (page == 1) {
          correct &= content == privacyNotice() && content.size() > 2500 &&
                     content.contains(QStringLiteral("contact@appfabrik-ai.de")) &&
                     content.contains(QStringLiteral("JSON")) &&
                     !content.contains(QStringLiteral("Bookwoodwork"));
          text->selectAll();
          correct &=
              text->textCursor().selectedText().contains(QStringLiteral("contact@appfabrik-ai.de"));
          text->moveCursor(QTextCursor::End);
          correct &= text->verticalScrollBar()->value() > 0;
          text->moveCursor(QTextCursor::Start);
        } else {
          correct &= content.contains(QStringLiteral("GNU LESSER GENERAL PUBLIC LICENSE")) &&
                     content.contains(QStringLiteral("Qt 6.8.3")) && content.size() > 100000;
        }
        const auto background = close->grab().toImage().pixelColor(8, close->height() / 2);
        correct &= background.red() > 120 && background.green() < 100 && background.blue() < 100;
        const auto captures = qEnvironmentVariable("CPP_INFO_SCREENSHOTS");
        if (!captures.isEmpty()) {
          QDir().mkpath(captures);
          correct &= dialog->grab().save(
              QDir(captures).filePath(tag + QStringLiteral("-") + name + QStringLiteral(".png")));
        }
        inspected = correct;
        if (page == 1) {
          QTest::keyClick(dialog, Qt::Key_Escape);
        } else {
          QTest::mouseClick(close, Qt::LeftButton);
        }
      });
      inspection.start();
      QTest::keyClick(info->menu(), Qt::Key_Return);
      QVERIFY2(QTest::qWaitFor([&] { return inspected; }),
               qPrintable(names[page] + QStringLiteral(" in ") + tag));
      QVERIFY(!QApplication::activeModalWidget());
      QVERIFY(window.isVisible());
    }
    QCOMPARE(readBytes(databasePath), database);
    QCOMPARE(readBytes(preferencePath), preference);
  }
};

QTEST_MAIN(InformationTest)
#include "information_test.moc"
