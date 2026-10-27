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
  void initialize() override;
  ProjectList findAll() override;
  ProjectDraft find(qint64 id) override;
  WoodProject create(const ProjectDraft& draft) override;
  WoodProject update(qint64 id, const ProjectDraft& draft) override;
  void remove(qint64 id) override;

private:
  void close();
  QString directory_;
  QString connectionName_;
  QSqlDatabase database_;
  std::unique_ptr<QLockFile> lock_;
  bool initialized_ = false;
};

} // namespace woodwork
