#include "persistence/language_preferences.h"
#include "application/project_repository.h"
#include "i18n/languages.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace woodwork {
LanguagePreference readLanguagePreference(const QString& directory) {
  QFile file(QDir(directory).filePath(QStringLiteral("language.txt")));
  if (!QFileInfo::exists(file.fileName())) {
    return {};
  }
  if (!file.open(QIODevice::ReadOnly) || file.size() > 128) {
    return {QStringLiteral("en-US"), true};
  }
  const auto value = QString::fromUtf8(file.readAll()).trimmed();
  if (file.error() != QFileDevice::NoError ||
      (value != QStringLiteral("auto") && !supportedLanguages().contains(value))) {
    return {QStringLiteral("en-US"), true};
  }
  return {value, false};
}

void saveLanguagePreference(const QDir& directory, const QString& preference) {
  if (preference != QStringLiteral("auto") && !supportedLanguages().contains(preference)) {
    throw StorageError(QCoreApplication::translate("App", "Diese Sprache wird nicht unterstützt."));
  }
  QSaveFile file(directory.filePath(QStringLiteral("language.txt")));
  const auto content = preference.toUtf8() + '\n';
  if (!directory.mkpath(QStringLiteral(".")) || !file.open(QIODevice::WriteOnly) ||
      file.write(content) != content.size() || !file.commit()) {
    throw StorageError(QCoreApplication::translate(
        "App", "Die Sprache konnte nicht gespeichert werden. Prüfe das Datenverzeichnis und deine "
               "Zugriffsrechte. Die bisherige Einstellung bleibt erhalten."));
  }
}
} // namespace woodwork
