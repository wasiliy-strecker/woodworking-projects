#pragma once

#include "application/project_repository.h"

namespace woodwork {

class ProjectService final {
public:
  explicit ProjectService(ProjectRepository& repository) : repository_(repository) {}
  ProjectList load();
  WoodProject save(qint64 id, const ProjectDraft& draft);
  void remove(qint64 id);

private:
  ProjectRepository& repository_;
};

} // namespace woodwork
