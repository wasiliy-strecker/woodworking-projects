#include "i18n/languages.h"
#include "ui/native_edit_menus.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QtTest>

using namespace woodwork;

namespace {
QAction* action(QWidget& window, const char* name) {
  auto* result = window.findChild<QAction*>(QString::fromLatin1(name));
  Q_ASSERT(result);
  return result;
}
void focus(QWidget& window, QWidget& field) {
  window.show();
  window.activateWindow();
  field.setFocus();
}
} // namespace

class NativeEditMenusTest final : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() { QApplication::setAttribute(Qt::AA_DontUseNativeDialogs); }

  void editsCurrentFieldAndPreservesUndo() {
    NativeEditMenus menus;
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    QLineEdit first;
    QLineEdit second;
    layout->addWidget(&first);
    layout->addWidget(&second);
    first.setText(QStringLiteral("Holz 東京"));
    focus(window, first);
    QVERIFY(QTest::qWaitFor([&] { return QApplication::focusWidget() == &first; }, 2000));
    action(window, "nativeSelectAll")->trigger();
    action(window, "nativeCopy")->trigger();
    QCOMPARE(QApplication::clipboard()->text(), first.text());
    second.setFocus();
    QVERIFY(QTest::qWaitFor([&] { return QApplication::focusWidget() == &second; }, 2000));
    QTest::keySequence(&second, QKeySequence::Paste);
    QCOMPARE(second.text(), first.text());
    action(window, "nativeSelectAll")->trigger();
    action(window, "nativeCut")->trigger();
    QVERIFY(second.text().isEmpty());
    QCOMPARE(first.text(), QStringLiteral("Holz 東京"));
    second.undo();
    QCOMPARE(second.text(), first.text());
  }

  void editsMultilineAndRichText() {
    NativeEditMenus menus;
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    QPlainTextEdit plain;
    QTextEdit rich;
    layout->addWidget(&plain);
    layout->addWidget(&rich);
    const auto text = QStringLiteral("Zeile 1\nÖl und Wachs 🪵");
    plain.setPlainText(text);
    focus(window, plain);
    QVERIFY(QTest::qWaitFor([&] { return QApplication::focusWidget() == &plain; }, 2000));
    action(window, "nativeSelectAll")->trigger();
    action(window, "nativeCopy")->trigger();
    QCOMPARE(QApplication::clipboard()->text(), text);
    rich.setFocus();
    QVERIFY(QTest::qWaitFor([&] { return QApplication::focusWidget() == &rich; }, 2000));
    action(window, "nativePaste")->trigger();
    QCOMPARE(rich.toPlainText(), text);
    action(window, "nativeSelectAll")->trigger();
    action(window, "nativeCut")->trigger();
    QVERIFY(rich.toPlainText().isEmpty());
    plain.clear();
    plain.setFocus();
    QVERIFY(QTest::qWaitFor([&] { return QApplication::focusWidget() == &plain; }, 2000));
    action(window, "nativePaste")->trigger();
    QCOMPARE(plain.toPlainText(), text);
  }

  void protectsReadOnlyDisabledAndUnfocusedFields() {
    NativeEditMenus menus;
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    QLineEdit line(QStringLiteral("saved"));
    QPlainTextEdit plain(QStringLiteral("saved"));
    QTextEdit rich(QStringLiteral("saved"));
    QPushButton button(QStringLiteral("No text editor"));
    for (QWidget* field : QList<QWidget*>{&line, &plain, &rich, &button})
      layout->addWidget(field);
    line.setReadOnly(true);
    plain.setReadOnly(true);
    rich.setReadOnly(true);
    for (QWidget* field : QList<QWidget*>{&line, &plain, &rich}) {
      focus(window, *field);
      QVERIFY(QTest::qWaitFor([&] { return QApplication::focusWidget() == field; }, 2000));
      action(window, "nativeSelectAll")->trigger();
      action(window, "nativeCopy")->trigger();
      QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("saved"));
      QApplication::clipboard()->setText(QStringLiteral("replacement"));
      action(window, "nativeCut")->trigger();
      action(window, "nativePaste")->trigger();
    }
    line.setReadOnly(false);
    line.setEnabled(false);
    focus(window, button);
    QVERIFY(QTest::qWaitFor([&] { return QApplication::focusWidget() == &button; }, 2000));
    action(window, "nativePaste")->trigger();
    QCOMPARE(line.text(), QStringLiteral("saved"));
    QCOMPARE(plain.toPlainText(), QStringLiteral("saved"));
    QCOMPARE(rich.toPlainText(), QStringLiteral("saved"));
  }

  void attachesToModalFilenameDialogAndDoesNotTouchFilePicker() {
    NativeEditMenus menus;
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    QLineEdit original(QStringLiteral("unchanged"));
    layout->addWidget(&original);
    focus(window, original);
    QVERIFY(QTest::qWaitFor([&] { return QApplication::focusWidget() == &original; }, 2000));
    QInputDialog filename(&window);
    filename.setInputMode(QInputDialog::TextInput);
    filename.setWindowModality(Qt::WindowModal);
    filename.setTextValue(QStringLiteral("backup.json"));
    filename.show();
    auto* field = filename.findChild<QLineEdit*>();
    QVERIFY(field);
    focus(filename, *field);
    QVERIFY(QTest::qWaitFor([&] { return QApplication::focusWidget() == field; }, 2000));
    QVERIFY(filename.findChild<QMenuBar*>(QString(), Qt::FindDirectChildrenOnly));
    QApplication::clipboard()->setText(QStringLiteral("Sammlung 東京.json"));
    action(filename, "nativeSelectAll")->trigger();
    action(filename, "nativePaste")->trigger();
    QCOMPARE(filename.textValue(), QStringLiteral("Sammlung 東京.json"));
    QCOMPARE(original.text(), QStringLiteral("unchanged"));
    filename.reject();
    QFileDialog picker(&window);
    picker.setFileMode(QFileDialog::ExistingFile);
    picker.show();
    QVERIFY(!picker.findChild<QMenuBar*>(QString(), Qt::FindDirectChildrenOnly));
    picker.reject();
    window.hide();
    window.show();
    QCOMPARE(window.findChildren<QMenuBar*>(QString(), Qt::FindDirectChildrenOnly).size(), 1);
  }

  void usesRecognizableTranslatedStandardActions_data() {
    QTest::addColumn<QString>("language");
    for (const auto& language : supportedLanguages())
      QTest::newRow(qPrintable(language)) << language;
  }

  void usesRecognizableTranslatedStandardActions() {
    QFETCH(QString, language);
    Translations translations;
    translations.install(language);
    NativeEditMenus menus;
    QWidget window;
    QVBoxLayout layout(&window);
    window.show();
    auto* bar = window.findChild<QMenuBar*>();
    QVERIFY(bar);
    QCOMPARE(bar->actions().size(), 1);
    const auto* copy = action(window, "nativeCopy");
    QCOMPARE(copy->menuRole(), QAction::TextHeuristicRole);
    QCOMPARE(copy->shortcut(), QKeySequence(QKeySequence::Copy));
    if (language != QStringLiteral("en-US"))
      QVERIFY(copy->text() != QStringLiteral("Copy"));
    QCOMPARE(action(window, "nativeCut")->shortcut(), QKeySequence(QKeySequence::Cut));
    QCOMPARE(action(window, "nativePaste")->shortcut(), QKeySequence(QKeySequence::Paste));
    QCOMPARE(action(window, "nativeSelectAll")->shortcut(), QKeySequence(QKeySequence::SelectAll));
  }
};

QTEST_MAIN(NativeEditMenusTest)
#include "native_edit_menus_test.moc"
