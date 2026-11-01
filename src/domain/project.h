#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QUuid>
#include <QVector>
#include <stdexcept>

namespace woodwork {

enum class ProjectStatus : quint8 { Planned, Started, Finished };
QString storedStatus(ProjectStatus status);
QString displayStatus(ProjectStatus status);
ProjectStatus statusFromStorage(const QString& value);

struct ChecklistStep {
  QUuid id;
  QString text;
  bool completed = false;
  qint64 position = 0;
  bool operator==(const ChecklistStep&) const = default;
};
using Checklist = QVector<ChecklistStep>;
Checklist validateChecklist(Checklist steps);

struct ProjectDraft {
  QString name;
  QString material;
  QString location;
  QString note;
  ProjectStatus status = ProjectStatus::Planned;
  QByteArray photo = {};
  Checklist steps = {};
  bool operator==(const ProjectDraft&) const = default;
};

struct WoodProject {
  qint64 id = 0;
  ProjectDraft fields;
  bool hasPhoto = false;
  bool operator==(const WoodProject&) const = default;
};

// List summaries never contain photo bytes. Full drafts are loaded by ID.
WoodProject summarize(qint64 id, ProjectDraft draft);

using ProjectList = QVector<WoodProject>;

class ValidationError final : public std::runtime_error {
public:
  explicit ValidationError(const QString& message) : std::runtime_error(message.toStdString()) {}
};

ProjectDraft validateDraft(ProjectDraft draft);
QString searchKey(const QString& text);
enum class SortField : quint8 { Name, Details, Location, Status, Photo };
using TransferDraftList = QVector<ProjectDraft>;
ProjectList searchProjects(const ProjectList& projects, const QString& query,
                           SortField field = SortField::Name,
                           Qt::SortOrder order = Qt::AscendingOrder);

} // namespace woodwork

Q_DECLARE_METATYPE(woodwork::ProjectDraft)
Q_DECLARE_METATYPE(woodwork::WoodProject)
Q_DECLARE_METATYPE(woodwork::ProjectList)

Q_DECLARE_METATYPE(woodwork::ProjectStatus)
