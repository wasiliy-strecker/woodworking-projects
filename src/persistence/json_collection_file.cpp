#include "persistence/json_collection_file.h"

#include "application/collection_codec.h"
#include "application/photo_importer.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryFile>
#include <exception>
#include <filesystem>
#include <utility>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace woodwork {
namespace {
[[noreturn]] void invalidFormat() {
  throw TransferError(QCoreApplication::translate(
      "App", "Dies ist keine gültige Woodworking Projects-Sammlung. "
             "Wähle eine mit Exportieren erzeugte JSON-Datei. Die Sammlung bleibt unverändert."));
}

[[noreturn]] void limitExceeded() {
  throw TransferError(QCoreApplication::translate(
      "App", "Eine Sammlungsdatei darf höchstens 10.000 Einträge und 256 MiB enthalten. "
             "Es wurde keine unvollständige Datei oder Sammlung gespeichert."));
}

[[noreturn]] void invalidTarget() {
  throw TransferError(QCoreApplication::translate(
      "App", "Wähle eine normale .json-Datei in einem vorhandenen Ordner außerhalb der "
             "App-Daten. Dateiverknüpfungen sind als Exportziel nicht erlaubt."));
}

[[noreturn]] void changedTarget() {
  throw TransferError(QCoreApplication::translate(
      "App", "Die Zieldatei wurde nach der Auswahl verändert. Wähle sie erneut. "
             "Sie wurde nicht ersetzt."));
}

[[noreturn]] void writeFailure() {
  throw TransferError(QCoreApplication::translate(
      "App", "Der Export konnte nicht sicher gespeichert werden. Prüfe Speicherplatz und "
             "Zugriffsrechte oder wähle einen anderen lokalen Ordner. "
             "Ein vorhandener Export bleibt erhalten."));
}

std::filesystem::path nativePath(const QString& path) {
#ifdef Q_OS_WIN
  return std::filesystem::path(path.toStdWString());
#else
  return std::filesystem::path(QFile::encodeName(path).constData());
#endif
}

QByteArray identity(const QString& path) {
#ifdef Q_OS_WIN
  const HANDLE handle =
      CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), FILE_READ_ATTRIBUTES,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                  FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    invalidTarget();
  }
  BY_HANDLE_FILE_INFORMATION info{};
  const bool ok = GetFileInformationByHandle(handle, &info) != 0;
  CloseHandle(handle);
  if (!ok || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
    invalidTarget();
  }
  return QByteArray::number(static_cast<qulonglong>(info.dwVolumeSerialNumber)) + ':' +
         QByteArray::number(static_cast<qulonglong>(info.nFileIndexHigh)) + ':' +
         QByteArray::number(static_cast<qulonglong>(info.nFileIndexLow));
#else
  struct stat info{};
  if (::lstat(QFile::encodeName(path).constData(), &info) != 0 || S_ISLNK(info.st_mode)) {
    invalidTarget();
  }
  return QByteArray::number(static_cast<qulonglong>(info.st_dev)) + ':' +
         QByteArray::number(static_cast<qulonglong>(info.st_ino));
#endif
}

QByteArray revision(const QString& path) {
#ifdef Q_OS_WIN
  const QFileInfo info(path);
  return QByteArray::number(info.size()) + ':' +
         QByteArray::number(info.lastModified().toMSecsSinceEpoch()) + ':' +
         QByteArray::number(info.birthTime().toMSecsSinceEpoch());
#else
  struct stat info{};
  if (::lstat(QFile::encodeName(path).constData(), &info) != 0) {
    changedTarget();
  }
#ifdef Q_OS_MACOS
  const auto modified = info.st_mtimespec;
  const auto changed = info.st_ctimespec;
#else
  const auto modified = info.st_mtim;
  const auto changed = info.st_ctim;
#endif
  return QByteArray::number(static_cast<qlonglong>(info.st_size)) + ':' +
         QByteArray::number(static_cast<qlonglong>(modified.tv_sec)) + ':' +
         QByteArray::number(static_cast<qlonglong>(modified.tv_nsec)) + ':' +
         QByteArray::number(static_cast<qlonglong>(changed.tv_sec)) + ':' +
         QByteArray::number(static_cast<qlonglong>(changed.tv_nsec));
#endif
}

// QJsonDocument validates the grammar but retains only the last duplicate key.
// Inspect the already valid token stream as well, including escaped key spellings.
void validateStringsAndKeys(const QByteArray& bytes) {
  QList<QSet<QString>> objects;
  for (qsizetype index = 0; index < bytes.size(); ++index) {
    if (bytes[index] == '{') {
      objects.append(QSet<QString>{});
    } else if (bytes[index] == '}') {
      objects.removeLast();
    } else if (bytes[index] == '"') {
      const auto start = index++;
      while (index < bytes.size() && bytes[index] != '"') {
        if (bytes[index] == '\\') {
          ++index;
        }
        ++index;
      }
      const auto token = bytes.mid(start, index - start + 1);
      const auto value = QJsonDocument::fromJson('[' + token + ']').array().first().toString();
      if (!value.isValidUtf16()) {
        invalidFormat();
      }
      auto next = index + 1;
      while (next < bytes.size() && QByteArray(" \r\n\t").contains(bytes[next])) {
        ++next;
      }
      if (next < bytes.size() && bytes[next] == ':') {
        if (objects.isEmpty() || objects.last().contains(value)) {
          invalidFormat();
        }
        objects.last().insert(value);
      }
    }
  }
}

void appendBounded(QByteArray& bytes, const QByteArray& part) {
  if (part.size() > maxTransferBytes - bytes.size()) {
    limitExceeded();
  }
  bytes.append(part);
}

QByteArray encode(const TransferDraftList& games) {
  if (games.size() > maxTransferEntries) {
    limitExceeded();
  }
  QByteArray bytes = "{\n  \"format\": \"woodworking-projects.collection\",\n  \"version\": 1,\n  "
                     "\"projects\": [\n";
  for (qsizetype index = 0; index < games.size(); ++index) {
    const auto& game = games[index];
    const auto object = encodeEntry(game);
    appendBounded(bytes, index == 0 ? "    " : ",\n    ");
    appendBounded(bytes, QJsonDocument(object).toJson(QJsonDocument::Compact));
  }
  appendBounded(bytes, "\n  ]\n}\n");
  return bytes;
}
} // namespace

JsonCollectionFile::JsonCollectionFile(QString dataDirectory, LinkAttempt link)
    : dataDirectory_(std::move(dataDirectory)), link_(std::move(link)) {
  if (!link_) {
    link_ = [](const QString& source, const QString& destination) {
      std::error_code error;
      std::filesystem::create_hard_link(nativePath(source), nativePath(destination), error);
      return error;
    };
  }
}

CollectionExportTarget JsonCollectionFile::inspect(const QString& path) {
  const QFileInfo file(path);
  if (path.isEmpty() || !file.isAbsolute() ||
      file.suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) != 0) {
    invalidTarget();
  }
  const auto directory = file.dir().canonicalPath();
  if (directory.isEmpty() || !QFileInfo(directory).isWritable()) {
    invalidTarget();
  }
  const auto protectedDirectory = QFileInfo(dataDirectory_).canonicalFilePath();
  if (!protectedDirectory.isEmpty() &&
      (directory == protectedDirectory || directory.startsWith(protectedDirectory + QChar('/')))) {
    invalidTarget();
  }
  CollectionExportTarget target;
  target.path = QDir(directory).filePath(file.fileName());
  target.directoryIdentity = identity(directory);
  const QFileInfo resolved(target.path);
  if (resolved.isSymLink()) {
    invalidTarget();
  }
  target.exists = resolved.exists();
  if (!target.exists) {
    return target;
  }
  if (!resolved.isFile() || !resolved.isReadable() || !resolved.isWritable()) {
    invalidTarget();
  }
  target.identity = identity(target.path);
  target.revision = revision(target.path);
  for (const auto& name : {QStringLiteral("holzwerkprojekt.sqlite3"),
                           QStringLiteral("holzwerkprojekt.sqlite3-journal"),
                           QStringLiteral("holzwerkprojekt.sqlite3-wal"),
                           QStringLiteral("holzwerkprojekt.sqlite3-shm"),
                           QStringLiteral("collection.lock"), QStringLiteral("language.txt")}) {
    const auto protectedFile = QDir(dataDirectory_).filePath(name);
    if (QFileInfo::exists(protectedFile) && identity(protectedFile) == target.identity) {
      invalidTarget();
    }
  }
  QFile input(target.path);
  if (!input.open(QIODevice::ReadOnly)) {
    invalidTarget();
  }
  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (!hash.addData(&input)) {
    invalidTarget();
  }
  target.digest = hash.result();
  if (identity(target.path) != target.identity || revision(target.path) != target.revision) {
    changedTarget();
  }
  return target;
}

TransferDraftList JsonCollectionFile::read(const QString& path) {
  QFile input(path);
  if (!QFileInfo(path).isFile() || !input.open(QIODevice::ReadOnly)) {
    throw TransferError(QCoreApplication::translate(
        "App", "Die Importdatei konnte nicht gelesen werden. Prüfe die Zugriffsrechte. "
               "Die Sammlung bleibt unverändert."));
  }
  const auto bytes = input.read(maxTransferBytes + 1);
  if (input.error() != QFileDevice::NoError) {
    throw TransferError(QCoreApplication::translate(
        "App", "Die Importdatei konnte nicht gelesen werden. Prüfe die Zugriffsrechte. "
               "Die Sammlung bleibt unverändert."));
  }
  if (bytes.size() > maxTransferBytes) {
    limitExceeded();
  }
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(bytes, &error);
  if (!bytes.isValidUtf8() || error.error != QJsonParseError::NoError || !document.isObject()) {
    invalidFormat();
  }
  validateStringsAndKeys(bytes);
  const auto root = document.object();
  if (root.size() != 3 ||
      root.value(QStringLiteral("format")) !=
          QJsonValue(QStringLiteral("woodworking-projects.collection")) ||
      !root.value(QStringLiteral("version")).isDouble() ||
      !root.value(QStringLiteral("projects")).isArray()) {
    invalidFormat();
  }
  if (root.value(QStringLiteral("version")).toDouble() != 1) {
    throw TransferError(QCoreApplication::translate(
        "App", "Diese Datei verwendet eine nicht unterstützte Exportversion. "
               "Die Sammlung bleibt unverändert."));
  }
  const auto entries = root.value(QStringLiteral("projects")).toArray();
  if (entries.size() > maxTransferEntries) {
    limitExceeded();
  }
  TransferDraftList games;
  games.reserve(entries.size());
  for (const auto& entry : entries) {
    const auto object = entry.toObject();
    if (!entry.isObject())
      invalidFormat();
    try {
      games.append(decodeEntry(object));
    } catch (const ValidationError&) {
      invalidFormat();
    } catch (const PhotoError&) {
      invalidFormat();
    }
  }
  return games;
}

void JsonCollectionFile::write(const TransferDraftList& games, const CollectionExportTarget& target,
                               bool replacementApproved) {
  const auto bytes = encode(games);
  if (target.exists && !replacementApproved) {
    throw TransferError(QCoreApplication::translate(
        "App", "Bitte bestätige das Ersetzen der vorhandenen Exportdatei."));
  }
  if (inspect(target.path) != target) {
    changedTarget();
  }
  QTemporaryFile temporary(
      QFileInfo(target.path).dir().filePath(QStringLiteral(".woodworking-projects-XXXXXX.tmp")));
  if (!temporary.open() || temporary.write(bytes) != bytes.size() || !temporary.flush()) {
    writeFailure();
  }
  temporary.close();
  if (temporary.error() != QFileDevice::NoError) {
    writeFailure();
  }
  if (inspect(target.path) != target) {
    changedTarget();
  }
  std::error_code error;
  if (!target.exists) {
    error = link_(temporary.fileName(), target.path);
    if (error == std::errc::file_exists) {
      changedTarget();
    }
    if (error) {
      // exFAT and some file providers cannot create hard links. NewOnly reserves
      // the destination without replacing another file, even after a late race.
      QFile output(target.path);
      if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        writeFailure();
      }
      QByteArray createdIdentity;
      try {
        createdIdentity = identity(target.path);
        if (output.write(bytes) != bytes.size() || !output.flush()) {
          writeFailure();
        }
        output.close();
        if (output.error() != QFileDevice::NoError) {
          writeFailure();
        }
      } catch (...) {
        const auto failure = std::current_exception();
        output.close();
        try {
          if (!createdIdentity.isEmpty() && identity(target.path) == createdIdentity) {
            QFile::remove(target.path);
          }
        } catch (const std::exception&) {
          // A removed/replaced destination must not cause deletion of another file.
          std::rethrow_exception(failure);
        }
        std::rethrow_exception(failure);
      }
    }
  } else {
#ifdef Q_OS_WIN
    if (!MoveFileExW(reinterpret_cast<LPCWSTR>(temporary.fileName().utf16()),
                     reinterpret_cast<LPCWSTR>(target.path.utf16()), MOVEFILE_REPLACE_EXISTING)) {
      writeFailure();
    }
#else
    std::filesystem::rename(nativePath(temporary.fileName()), nativePath(target.path), error);
    if (error) {
      writeFailure();
    }
#endif
  }
}

} // namespace woodwork
