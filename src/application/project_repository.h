#pragma once

#include "domain/project.h"

namespace woodwork {

class StorageError final : public std::runtime_error {
public:
  explicit StorageError(const QString& message) : std::runtime_error(message.toStdString()) {}
};

// Implementations belong to one thread. Every successful mutation is committed.
class ProjectRepository {
public:
  virtual ~ProjectRepository() = default;
  virtual void initialize() = 0;
  virtual ProjectList findAll() = 0;
  virtual ProjectDraft find(qint64 id) = 0;
  virtual WoodProject create(const ProjectDraft& draft) = 0;
  virtual WoodProject update(qint64 id, const ProjectDraft& draft) = 0;
  virtual void remove(qint64 id) = 0;
};

} // namespace woodwork
