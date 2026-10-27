#include "persistence/data_directory.h"
#include "application/project_repository.h"

#include <QDir>
#include <QStandardPaths>

namespace woodwork {
QString defaultDataDirectory() {
  return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
}

QString resolveDataDirectory(const QString& overridePath, bool explicitlyProvided) {
  if (explicitlyProvided) {
    if (overridePath.trimmed().isEmpty() || !QDir::isAbsolutePath(overridePath)) {
      throw StorageError(
          QStringLiteral("--data-dir benötigt ein absolutes, nicht leeres Verzeichnis."));
    }
    return QDir::cleanPath(overridePath);
  }
  const auto path = defaultDataDirectory();
  if (path.isEmpty()) {
    throw StorageError(QStringLiteral("Das persönliche Datenverzeichnis ist nicht verfügbar."));
  }
  return path;
}
} // namespace woodwork
