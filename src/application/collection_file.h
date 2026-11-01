#pragma once

#include "domain/project.h"
#include <QByteArray>

namespace woodwork {

inline constexpr qsizetype maxTransferEntries = 10000;
inline constexpr qint64 maxTransferBytes = qint64{256} * 1024 * 1024;

class TransferError final : public std::runtime_error {
public:
  explicit TransferError(const QString& message) : std::runtime_error(message.toStdString()) {}
};

struct CollectionExportTarget {
  QString path;
  QByteArray directoryIdentity;
  bool exists = false;
  QByteArray identity;
  QByteArray revision;
  QByteArray digest;
  bool operator==(const CollectionExportTarget&) const = default;
};

class CollectionFile {
public:
  virtual ~CollectionFile() = default;
  virtual CollectionExportTarget inspect(const QString& path) = 0;
  virtual TransferDraftList read(const QString& path) = 0;
  virtual void write(const TransferDraftList& games, const CollectionExportTarget& target,
                     bool replacementApproved) = 0;
};

} // namespace woodwork

Q_DECLARE_METATYPE(woodwork::CollectionExportTarget)
