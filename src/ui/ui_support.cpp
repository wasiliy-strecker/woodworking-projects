#include "ui/ui_support.h"
#include <QCoreApplication>

#include <QMessageBox>
#include <QPushButton>

namespace woodwork {
bool confirmDiscard(QWidget* parent) {
  QMessageBox box(
      QMessageBox::Question, QCoreApplication::translate("App", "Änderungen verwerfen?"),
      QCoreApplication::translate("App", "Deine Änderungen sind noch nicht gespeichert."),
      QMessageBox::NoButton, parent);
  box.setObjectName(QStringLiteral("discardConfirmation"));
  box.setTextFormat(Qt::PlainText);
  auto* keep = box.addButton(QCoreApplication::translate("App", "Weiterbearbeiten"),
                             QMessageBox::RejectRole);
  keep->setObjectName(QStringLiteral("keepEditing"));
  auto* discard =
      box.addButton(QCoreApplication::translate("App", "Verwerfen"), QMessageBox::DestructiveRole);
  discard->setObjectName(QStringLiteral("discardChanges"));
  box.setDefaultButton(keep);
  box.setEscapeButton(keep);
  box.exec();
  return box.clickedButton() == discard;
}

bool confirmDelete(QWidget* parent, const QString& name) {
  QMessageBox box(
      QMessageBox::Question, QCoreApplication::translate("App", "Projekt löschen?"),
      QCoreApplication::translate("App", "Möchtest du „%1“ endgültig aus deiner Sammlung löschen?")
          .arg(name),
      QMessageBox::NoButton, parent);
  box.setObjectName(QStringLiteral("deleteConfirmation"));
  box.setTextFormat(Qt::PlainText);
  auto* cancel =
      box.addButton(QCoreApplication::translate("App", "Abbrechen"), QMessageBox::RejectRole);
  cancel->setObjectName(QStringLiteral("cancelDelete"));
  auto* remove =
      box.addButton(QCoreApplication::translate("App", "Löschen"), QMessageBox::DestructiveRole);
  remove->setObjectName(QStringLiteral("confirmDelete"));
  box.setDefaultButton(cancel);
  box.setEscapeButton(cancel);
  box.exec();
  return box.clickedButton() == remove;
}
} // namespace woodwork
