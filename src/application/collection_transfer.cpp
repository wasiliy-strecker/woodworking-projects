#include "application/collection_transfer.h"
namespace woodwork {
qsizetype CollectionTransfer::exportEntries(const CollectionExportTarget& target, bool approved) {
  const auto drafts = repository_.transferSnapshot();
  files_.write(drafts, target, approved);
  return drafts.size();
}
} // namespace woodwork
