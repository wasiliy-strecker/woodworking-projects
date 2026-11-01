#include "ui/ui_support.h"
#include <QCoreApplication>
#include <QDialogButtonBox>

#include <QLocale>
#include <QMessageBox>
#include <QPushButton>

namespace woodwork {
bool confirmDiscard(QWidget* parent) {
  QMessageBox box(
      QMessageBox::Question, QCoreApplication::translate("App", "Änderungen verwerfen?"),
      QCoreApplication::translate("App", "Deine Änderungen sind noch nicht gespeichert."),
      QMessageBox::NoButton, parent);
  box.setObjectName(QStringLiteral("discardConfirmation"));
  box.setOption(QMessageBox::Option::DontUseNativeDialog);
  // Keep editing stays left of Discard with its safe default and reject role.
  box.setStyleSheet(QStringLiteral("QDialogButtonBox { button-layout: %1; }")
                        .arg(QDialogButtonBox::AndroidLayout));
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
  box.setOption(QMessageBox::Option::DontUseNativeDialog);
  // Keep Cancel left of Delete while retaining their reject/destructive roles.
  box.setStyleSheet(QStringLiteral("QDialogButtonBox { button-layout: %1; }")
                        .arg(QDialogButtonBox::AndroidLayout));
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
bool confirmImport(QWidget* parent, qsizetype count) {
  QMessageBox box(QMessageBox::Question,
                  QCoreApplication::translate("App", "Einträge importieren?"),
                  QCoreApplication::translate(
                      "App", "Einträge in der Datei: %1. Vorhandene Einträge bleiben erhalten. "
                             "Bereits vorhandene identische Exemplare werden übersprungen. "
                             "Nach dem Import wird die Suche geleert.")
                      .arg(QLocale().toString(count)),
                  QMessageBox::NoButton, parent);
  box.setObjectName(QStringLiteral("importConfirmation"));
  box.setTextFormat(Qt::PlainText);
  auto* cancel =
      box.addButton(QCoreApplication::translate("App", "Abbrechen"), QMessageBox::RejectRole);
  cancel->setObjectName(QStringLiteral("cancelImport"));
  auto* import =
      box.addButton(QCoreApplication::translate("App", "Importieren"), QMessageBox::AcceptRole);
  import->setObjectName(QStringLiteral("confirmImport"));
  box.setDefaultButton(cancel);
  box.setEscapeButton(cancel);
  box.exec();
  return box.clickedButton() == import;
}

bool confirmExportReplacement(QWidget* parent, const QString& filename) {
  QMessageBox box(
      QMessageBox::Question, QCoreApplication::translate("App", "Export ersetzen?"),
      QCoreApplication::translate(
          "App", "„%1“ ist bereits vorhanden. Durch die gesamte gespeicherte Sammlung ersetzen?")
          .arg(filename),
      QMessageBox::NoButton, parent);
  box.setObjectName(QStringLiteral("replaceExportConfirmation"));
  box.setTextFormat(Qt::PlainText);
  auto* cancel =
      box.addButton(QCoreApplication::translate("App", "Abbrechen"), QMessageBox::RejectRole);
  cancel->setObjectName(QStringLiteral("cancelReplaceExport"));
  auto* replace =
      box.addButton(QCoreApplication::translate("App", "Ersetzen"), QMessageBox::AcceptRole);
  replace->setObjectName(QStringLiteral("confirmReplaceExport"));
  box.setDefaultButton(cancel);
  box.setEscapeButton(cancel);
  box.exec();
  return box.clickedButton() == replace;
}

} // namespace woodwork
