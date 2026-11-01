#pragma once

#include "application/project_repository.h"

#include <QLockFile>
#include <QSqlDatabase>
#include <memory>

namespace woodwork {

class SqliteProjectRepository final : public ProjectRepository {
public:
  explicit SqliteProjectRepository(QString directory);
  ~SqliteProjectRepository() override;
  SqliteProjectRepository(const SqliteProjectRepository&) = delete;
  SqliteProjectRepository& operator=(const SqliteProjectRepository&) = delete;
  TransferDraftList transferSnapshot() override;
  CollectionImportResult importEntries(const TransferDraftList& drafts) override;
  void initialize() override;
  ProjectList findAll() override;
  ProjectDraft find(qint64 id) override;
  WoodProject create(const ProjectDraft& draft) override;
  WoodProject update(qint64 id, const ProjectDraft& draft) override;
  void remove(qint64 id) override;

private:
  ProjectList findAllInTransaction();
  ProjectDraft findInTransaction(qint64 id);
  WoodProject createInTransaction(const ProjectDraft& draft);
  void close();
  QString directory_;
  QString connectionName_;
  QSqlDatabase database_;
  std::unique_ptr<QLockFile> lock_;
  bool initialized_ = false;
};

} // namespace woodwork
