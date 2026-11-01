#pragma once

#include "application/collection_file.h"
#include "application/project_repository.h"

namespace woodwork {

class CollectionTransfer final {
public:
  CollectionTransfer(ProjectRepository& repository, CollectionFile& files)
      : repository_(repository), files_(files) {}
  qsizetype exportEntries(const CollectionExportTarget& target, bool replacementApproved);

private:
  ProjectRepository& repository_;
  CollectionFile& files_;
};

} // namespace woodwork
