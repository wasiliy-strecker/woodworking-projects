#include "ui/language_dialog.h"
#include "i18n/languages.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace woodwork {
LanguageDialog::LanguageDialog(StorageController& controller, const QDir& directory,
                               const QString& preference, QWidget* parent)
    : QDialog(parent) {
  setObjectName(QStringLiteral("languageDialog"));
  setWindowTitle(QCoreApplication::translate("App", "Sprache"));
  setWindowModality(Qt::WindowModal);
  resize(520, 250);
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 24, 24, 24);
  layout->setSpacing(18);
  auto* label = new QLabel(QCoreApplication::translate("App", "Sprache der App"), this);
  auto* combo = new QComboBox(this);
  combo->setObjectName(QStringLiteral("languageChoice"));
  combo->setAccessibleName(label->text());
  label->setBuddy(combo);
  combo->addItem(QCoreApplication::translate("App", "Automatisch (Systemsprache)"),
                 QStringLiteral("auto"));
  for (const auto& tag : supportedLanguages()) {
    combo->addItem(languageName(tag), tag);
  }
  combo->setCurrentIndex(combo->findData(preference));
  auto* hint = new QLabel(
      QCoreApplication::translate("App", "Die gewählte Sprache gilt beim nächsten Start. Deine "
                                         "Projekte und Fotos bleiben unverändert."),
      this);
  hint->setWordWrap(true);
  auto* error = new QLabel(this);
  error->setObjectName(QStringLiteral("languageError"));
  error->setWordWrap(true);
  error->setTextFormat(Qt::PlainText);
  error->hide();
  auto* buttons = new QDialogButtonBox(this);
  auto* cancel = buttons->addButton(QCoreApplication::translate("App", "Abbrechen"),
                                    QDialogButtonBox::RejectRole);
  auto* save = buttons->addButton(QCoreApplication::translate("App", "Speichern"),
                                  QDialogButtonBox::AcceptRole);
  save->setObjectName(QStringLiteral("saveLanguage"));
  cancel->setObjectName(QStringLiteral("cancelLanguage"));
  save->setDefault(true);
  layout->addWidget(label);
  layout->addWidget(combo);
  layout->addWidget(hint);
  layout->addWidget(error);
  layout->addStretch();
  layout->addWidget(buttons);
  connect(cancel, &QPushButton::clicked, this, &LanguageDialog::reject);
  connect(save, &QPushButton::clicked, this, [this, &controller, directory, combo, buttons, error] {
    if (saving_ || controller.isBusy()) {
      return;
    }
    saving_ = true;
    error->hide();
    combo->setEnabled(false);
    buttons->setEnabled(false);
    controller.saveLanguage(directory.path(), combo->currentData().toString());
  });
  connect(&controller, &StorageController::languageSaved, this, [this](const QString&) {
    if (saving_) {
      saving_ = false;
      accept();
    }
  });
  connect(&controller, &StorageController::failed, this,
          [this, error, combo, buttons](Operation operation, const QString& message) {
            if (operation == Operation::Language && saving_) {
              saving_ = false;
              combo->setEnabled(true);
              buttons->setEnabled(true);
              error->setText(message);
              error->show();
            }
          });
}

void LanguageDialog::reject() {
  if (!saving_) {
    QDialog::reject();
  }
}

void LanguageDialog::closeEvent(QCloseEvent* event) {
  if (saving_) {
    event->ignore();
  } else {
    QDialog::closeEvent(event);
  }
}
} // namespace woodwork
