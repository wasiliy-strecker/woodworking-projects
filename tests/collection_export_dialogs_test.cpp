#include "i18n/languages.h"
#include "ui/export_file_dialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

using namespace woodwork;

namespace {
void selectFolder(const QString& path, bool cancel = false) {
  QTimer::singleShot(0, qApp, [path, cancel] {
    auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    QVERIFY(dialog);
    QCOMPARE(dialog->objectName(), QStringLiteral("exportFolderDialog"));
    QCOMPARE(dialog->fileMode(), QFileDialog::Directory);
    QVERIFY(dialog->testOption(QFileDialog::ShowDirsOnly));
    if (cancel) {
      dialog->reject();
    } else {
      dialog->setDirectory(path);
      QVERIFY(QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection));
    }
  });
}
} // namespace

class ExportDialogsTest final : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() { QApplication::setAttribute(Qt::AA_DontUseNativeDialogs); }

  void filenamesStayWithinSelectedDirectory() {
    for (const auto& invalid :
         {QString(), QStringLiteral("  "), QStringLiteral("."), QStringLiteral(".."),
          QStringLiteral("../escape"), QStringLiteral("folder/file"),
          QStringLiteral("folder\\file"), QStringLiteral("C:name"), QStringLiteral("bad\nname"),
          QStringLiteral("bad\tname"), QString(251, QChar('x')), QString(100, QChar(0x65e5)),
          QString(QChar(0xd800))}) {
      QVERIFY2(exportFilename(invalid).isEmpty(), qPrintable(invalid));
    }
    QCOMPARE(exportFilename(QStringLiteral("  Sammlung 東京 🎲  ")),
             QStringLiteral("Sammlung 東京 🎲.json"));
    QCOMPARE(exportFilename(QStringLiteral("backup.JSON")), QStringLiteral("backup.JSON"));
    QCOMPARE(exportFilename(QString(250, QChar('x'))).toUtf8().size(), 255);
  }

  void directoryAndFilenameWorkflow_data() {
    QTest::addColumn<QString>("language");
    for (const auto& language : supportedLanguages()) {
      QTest::newRow(qPrintable(language)) << language;
    }
  }

  void directoryAndFilenameWorkflow() {
    QFETCH(QString, language);
    Translations translations;
    translations.install(language);
    QTemporaryDir output;
    QWidget parent;
    parent.show();
    bool checked = false;
    selectFolder(output.path());
    QTimer answer;
    answer.setInterval(10);
    connect(&answer, &QTimer::timeout, [&] {
      auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
      if (!dialog) {
        return;
      }
      answer.stop();
      QCOMPARE(dialog->objectName(), QStringLiteral("exportFilenameDialog"));
      QCOMPARE(dialog->textValue(), QStringLiteral("suggested.json"));
      auto* buttons = dialog->findChild<QDialogButtonBox*>();
      QVERIFY(buttons);
      auto* save = buttons->button(QDialogButtonBox::Ok);
      dialog->setTextValue(QStringLiteral("../outside"));
      QVERIFY(!save->isEnabled());
      dialog->setTextValue(QStringLiteral("Sammlung 東京"));
      QVERIFY(save->isEnabled());
      QCOMPARE(save->text(), QCoreApplication::translate("App", "Exportieren"));
      const auto screenshots = qEnvironmentVariable("BOARD_GAME_EXPORT_SCREENSHOTS");
      if (!screenshots.isEmpty()) {
        QVERIFY(dialog->grab().save(QDir(screenshots).filePath(language + QStringLiteral(".png"))));
      }
      checked = true;
      save->click();
    });
    answer.start();
    QCOMPARE(chooseExportFile(&parent, QDir(output.path()), QStringLiteral("suggested.json"), true),
             output.filePath(QStringLiteral("Sammlung 東京.json")));
    QVERIFY(checked);
    QVERIFY(QDir(output.path()).entryList(QDir::Files | QDir::Hidden).isEmpty());
  }

  void cancellingEitherDialogCreatesNoFile() {
    QTemporaryDir output;
    QWidget parent;
    parent.show();
    selectFolder(output.path(), true);
    QVERIFY(chooseExportFile(&parent, QDir(output.path()), QStringLiteral("backup.json"), true)
                .isEmpty());
    selectFolder(output.path());
    QTimer cancel;
    cancel.setInterval(10);
    connect(&cancel, &QTimer::timeout, [&] {
      if (auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget())) {
        cancel.stop();
        QTest::keyClick(dialog, Qt::Key_Escape);
      }
    });
    cancel.start();
    QVERIFY(chooseExportFile(&parent, QDir(output.path()), QStringLiteral("backup.json"), true)
                .isEmpty());
    QVERIFY(QDir(output.path()).entryList(QDir::Files | QDir::Hidden).isEmpty());
  }
};

QTEST_MAIN(ExportDialogsTest)
#include "collection_export_dialogs_test.moc"
