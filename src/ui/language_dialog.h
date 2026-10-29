#pragma once

#include "ui/storage_controller.h"
#include <QDialog>
#include <QDir>

namespace woodwork {
class LanguageDialog final : public QDialog {
  Q_OBJECT
public:
  LanguageDialog(StorageController& controller, const QDir& directory, const QString& preference,
                 QWidget* parent = nullptr);
  void reject() override;

protected:
  void closeEvent(QCloseEvent* event) override;

private:
  bool saving_ = false;
};
} // namespace woodwork
