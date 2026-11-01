#include "ui/native_edit_menus.h"

#include <QAction>
#include <QApplication>
#include <QColorDialog>
#include <QEvent>
#include <QFileDialog>
#include <QFontDialog>
#include <QLayout>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QTextEdit>

namespace woodwork {
namespace {
template <typename Editor> void edit(Editor* editor, QKeySequence::StandardKey operation) {
  switch (operation) {
  case QKeySequence::Cut:
    if (!editor->isReadOnly())
      editor->cut();
    break;
  case QKeySequence::Copy:
    editor->copy();
    break;
  case QKeySequence::Paste:
    if (!editor->isReadOnly())
      editor->paste();
    break;
  case QKeySequence::SelectAll:
    editor->selectAll();
    break;
  default:
    break;
  }
}

void editFocusedWidget(QKeySequence::StandardKey operation) {
  auto* field = QApplication::focusWidget();
  const auto* modal = QApplication::activeModalWidget();
  if (!field || !field->isVisible() || !field->isEnabled() || (modal && field->window() != modal)) {
    return;
  }
  if (auto* line = qobject_cast<QLineEdit*>(field)) {
    edit(line, operation);
  } else if (auto* plain = qobject_cast<QPlainTextEdit*>(field)) {
    edit(plain, operation);
  } else if (auto* rich = qobject_cast<QTextEdit*>(field)) {
    edit(rich, operation);
  }
}

QMenuBar* createMenu(QWidget* window) {
  auto* bar = new QMenuBar(window);
  bar->setObjectName(QStringLiteral("nativeEditMenuBar"));
  bar->setNativeMenuBar(true);
  auto* menu = bar->addMenu(QCoreApplication::translate("App", "&Bearbeiten"));
  const auto add = [menu](const QString& text, const char* name,
                          QKeySequence::StandardKey operation) {
    auto* action = menu->addAction(text);
    action->setObjectName(QString::fromLatin1(name));
    action->setMenuRole(QAction::TextHeuristicRole);
    action->setShortcut(QKeySequence(operation));
    QObject::connect(action, &QAction::triggered, menu,
                     [operation] { editFocusedWidget(operation); });
  };
  // Qt 6.8 recognizes these exact translated captions and routes native dialogs
  // through AppKit's responder chain. Native controls validate their own actions.
  // Do not disable actions using stale Qt focus while an NSOpenPanel is active.
  add(QCoreApplication::translate("QCocoaMenuItem", "Cut"), "nativeCut", QKeySequence::Cut);
  add(QCoreApplication::translate("QCocoaMenuItem", "Copy"), "nativeCopy", QKeySequence::Copy);
  add(QCoreApplication::translate("QCocoaMenuItem", "Paste"), "nativePaste", QKeySequence::Paste);
  add(QCoreApplication::translate("QCocoaMenuItem", "Select All"), "nativeSelectAll",
      QKeySequence::SelectAll);
  return bar;
}
} // namespace

NativeEditMenus::NativeEditMenus(QObject* parent)
    : QObject(parent), fallback_(createMenu(nullptr)) {
  qApp->installEventFilter(this);
  for (auto* window : QApplication::topLevelWidgets()) {
    if (window->isVisible())
      installFor(window);
  }
}

NativeEditMenus::~NativeEditMenus() {
  qApp->removeEventFilter(this);
  for (const auto& menu : windowMenus_)
    delete menu.data();
}

bool NativeEditMenus::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::Show) {
    if (auto* window = qobject_cast<QWidget*>(watched))
      installFor(window);
  }
  return QObject::eventFilter(watched, event);
}

void NativeEditMenus::installFor(QWidget* window) {
  if (!window->isWindow() || window == fallback_.get() ||
      (window->windowType() != Qt::Window && window->windowType() != Qt::Dialog &&
       window->windowType() != Qt::Sheet) ||
      qobject_cast<QFileDialog*>(window) || qobject_cast<QColorDialog*>(window) ||
      qobject_cast<QFontDialog*>(window) || qobject_cast<QMessageBox*>(window) ||
      window->findChild<QMenuBar*>(QString(), Qt::FindDirectChildrenOnly)) {
    return;
  }
  // A global menu alone is disabled by Qt for modal Qt windows. Attach a menu
  // to each such window as well. Never customize platform-native dialog widgets.
  windowMenus_.removeIf([](const auto& menu) { return menu.isNull(); });
  auto* bar = createMenu(window);
  windowMenus_.append(bar);
  if (auto* main = qobject_cast<QMainWindow*>(window)) {
    main->setMenuBar(bar);
  } else if (window->layout()) {
    window->layout()->setMenuBar(bar);
  }
  bar->show();
}

} // namespace woodwork
