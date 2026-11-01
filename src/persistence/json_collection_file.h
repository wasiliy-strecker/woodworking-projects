#pragma once

#include "application/collection_file.h"
#include <functional>
#include <system_error>

namespace woodwork {

class JsonCollectionFile final : public CollectionFile {
public:
  using LinkAttempt = std::function<std::error_code(const QString&, const QString&)>;
  explicit JsonCollectionFile(QString dataDirectory = {}, LinkAttempt link = {});
  CollectionExportTarget inspect(const QString& path) override;
  TransferDraftList read(const QString& path) override;
  void write(const TransferDraftList& games, const CollectionExportTarget& target,
             bool replacementApproved) override;

private:
  QString dataDirectory_;
  LinkAttempt link_;
};

} // namespace woodwork
