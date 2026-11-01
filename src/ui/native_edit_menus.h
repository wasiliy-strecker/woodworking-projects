#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <memory>

class QMenuBar;
class QWidget;

namespace woodwork {

// Owns a fallback menu and installs window-specific menus for modal Qt editors.
// Only instantiated by the application on macOS. Also testable with Qt offscreen.
class NativeEditMenus final : public QObject {
public:
  explicit NativeEditMenus(QObject* parent = nullptr);
  ~NativeEditMenus() override;

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  void installFor(QWidget* window);
  std::unique_ptr<QMenuBar> fallback_;
  QList<QPointer<QMenuBar>> windowMenus_;
};

} // namespace woodwork
