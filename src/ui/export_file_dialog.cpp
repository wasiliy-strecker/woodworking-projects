#include "ui/export_file_dialog.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QPushButton>
#include <algorithm>

namespace woodwork {
QString exportFilename(const QString& entered) {
  auto name = entered.trimmed();
  if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("..") ||
      !name.isValidUtf16() || std::any_of(name.cbegin(), name.cend(), [](QChar character) {
        return character.category() == QChar::Other_Control ||
               QStringLiteral("/\\:").contains(character);
      })) {
    return {};
  }
  if (!name.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
    name += QStringLiteral(".json");
  }
  return name.toUtf8().size() <= 255 ? name : QString();
}

QString chooseExportFile(QWidget* parent, const QDir& directory, const QString& suggestedName,
                         bool selectDirectory) {
  if (selectDirectory) {
    QString selected;
    {
      QFileDialog folder(parent,
                         QCoreApplication::translate("App", "Ordner für den Export wählen"));
      folder.setObjectName(QStringLiteral("exportFolderDialog"));
      folder.setFileMode(QFileDialog::Directory);
      folder.setOption(QFileDialog::ShowDirsOnly);
      folder.setDirectory(directory);
      if (folder.exec() != QDialog::Accepted || folder.selectedFiles().isEmpty()) {
        return {};
      }
      selected = folder.selectedFiles().first();
    }
    // A native directory selection grants access to the temporary sibling too.
    QInputDialog filename(parent);
    filename.setObjectName(QStringLiteral("exportFilenameDialog"));
    filename.setWindowTitle(QCoreApplication::translate("App", "Gesamte Sammlung exportieren"));
    filename.setLabelText(QCoreApplication::translate(
        "App", "Dateiname ohne Ordnerpfad. Die Endung .json wird bei Bedarf ergänzt."));
    filename.setInputMode(QInputDialog::TextInput);
    filename.setTextValue(suggestedName);
    filename.setOkButtonText(QCoreApplication::translate("App", "Exportieren"));
    filename.setCancelButtonText(QCoreApplication::translate("App", "Abbrechen"));
    const auto validate = [&filename](const QString& text) {
      auto* buttons = filename.findChild<QDialogButtonBox*>();
      if (buttons) {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!exportFilename(text).isEmpty());
      }
    };
    QObject::connect(&filename, &QInputDialog::textValueChanged, &filename, validate);
    filename.resize(540, filename.sizeHint().height());
    validate(filename.textValue());
    if (filename.exec() != QDialog::Accepted) {
      return {};
    }
    const auto name = exportFilename(filename.textValue());
    return name.isEmpty() ? QString() : QDir(selected).filePath(name);
  }
  QFileDialog dialog(parent, QCoreApplication::translate("App", "Gesamte Sammlung exportieren"));
  dialog.setObjectName(QStringLiteral("exportCollectionDialog"));
  dialog.setAcceptMode(QFileDialog::AcceptSave);
  dialog.setFileMode(QFileDialog::AnyFile);
  dialog.setOption(QFileDialog::DontConfirmOverwrite);
  dialog.setNameFilter(
      QCoreApplication::translate("App", "Woodworking Projects-Sammlung (*.json *.JSON)"));
  dialog.setDefaultSuffix(QStringLiteral("json"));
  dialog.setDirectory(directory);
  dialog.selectFile(suggestedName);
  return dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()
             ? dialog.selectedFiles().first()
             : QString();
}
} // namespace woodwork
