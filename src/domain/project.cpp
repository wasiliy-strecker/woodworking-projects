#include "domain/project.h"
#include <QCoreApplication>

#include <QCollator>
#include <QLocale>
#include <QSet>
#include <algorithm>

namespace woodwork {

QString storedStatus(ProjectStatus status) {
  switch (status) {
  case ProjectStatus::Planned:
    return QStringLiteral("planned");
  case ProjectStatus::Started:
    return QStringLiteral("started");
  case ProjectStatus::Finished:
    return QStringLiteral("finished");
  }
  throw ValidationError(QCoreApplication::translate("App", "Der Projektstatus ist ungültig."));
}
QString displayStatus(ProjectStatus status) {
  switch (status) {
  case ProjectStatus::Planned:
    return QCoreApplication::translate("App", "Geplant");
  case ProjectStatus::Started:
    return QCoreApplication::translate("App", "Angefangen");
  case ProjectStatus::Finished:
    return QCoreApplication::translate("App", "Fertig");
  }
  throw ValidationError(QCoreApplication::translate("App", "Der Projektstatus ist ungültig."));
}
ProjectStatus statusFromStorage(const QString& value) {
  if (value == QStringLiteral("planned"))
    return ProjectStatus::Planned;
  if (value == QStringLiteral("started"))
    return ProjectStatus::Started;
  if (value == QStringLiteral("finished"))
    return ProjectStatus::Finished;
  throw ValidationError(QCoreApplication::translate("App",
                                                    "Unbekannter gespeicherter Projektstatus „%1“. "
                                                    "Die Daten werden nicht verändert.")
                            .arg(value));
}
WoodProject summarize(qint64 id, ProjectDraft draft) {
  const bool hasPhoto = !draft.photo.isEmpty();
  draft.photo.clear();
  return {id, std::move(draft), hasPhoto};
}

ProjectDraft validateDraft(ProjectDraft draft) {
  (void)storedStatus(draft.status);
  draft.name = draft.name.trimmed();
  draft.material = draft.material.trimmed();
  draft.location = draft.location.trimmed();
  if (draft.name.isEmpty()) {
    throw ValidationError(QCoreApplication::translate("App", "Bitte gib einen Projektnamen ein."));
  }
  draft.steps = validateChecklist(std::move(draft.steps));
  return draft;
}

Checklist validateChecklist(Checklist steps) {
  QSet<QUuid> ids;
  qint64 previousPosition = -1;
  for (qsizetype index = 0; index < steps.size(); ++index) {
    auto& step = steps[index];
    step.text = step.text.trimmed();
    if (step.text.isEmpty()) {
      throw ValidationError(
          QCoreApplication::translate("App", "Arbeitsschritt %1 ist leer. Gib einen Text ein oder "
                                             "entferne diese Zeile.")
              .arg(index + 1));
    }
    for (const auto character : step.text) {
      if (character == QChar('\n') || character == QChar('\r') || character == QChar(0x2028) ||
          character == QChar(0x2029)) {
        throw ValidationError(
            QCoreApplication::translate("App", "Arbeitsschritt %1 muss einzeilig sein.")
                .arg(index + 1));
      }
    }
    if (step.id.isNull() || ids.contains(step.id) || step.position <= previousPosition) {
      throw ValidationError(
          QCoreApplication::translate("App", "Die IDs oder die Reihenfolge der Arbeitsschritte "
                                             "sind ungültig. Die Daten werden nicht verändert."));
    }
    ids.insert(step.id);
    previousPosition = step.position;
  }
  return steps;
}

QString searchKey(const QString& text) {
  return text.normalized(QString::NormalizationForm_C)
      .toCaseFolded()
      .normalized(QString::NormalizationForm_C);
}

ProjectList searchProjects(const ProjectList& projects, const QString& query, SortField field,
                           Qt::SortOrder order) {
  const auto needle = searchKey(query.trimmed());
  ProjectList matches;
  for (const auto& project : projects) {
    const auto& f = project.fields;
    if (searchKey(f.name).contains(needle) || searchKey(f.material).contains(needle) ||
        searchKey(f.location).contains(needle) || searchKey(f.note).contains(needle) ||
        std::any_of(f.steps.cbegin(), f.steps.cend(), [&needle](const ChecklistStep& step) {
          return searchKey(step.text).contains(needle);
        })) {
      matches.append(project);
    }
  }
  QCollator collator{QLocale()};
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  collator.setNumericMode(true);
  std::sort(
      matches.begin(), matches.end(),
      [&collator, field, order](const WoodProject& left, const WoodProject& right) {
        int comparison = 0;
        switch (field) {
        case SortField::Name:
          comparison = collator.compare(left.fields.name.normalized(QString::NormalizationForm_C),
                                        right.fields.name.normalized(QString::NormalizationForm_C));
          break;
        case SortField::Details:
          comparison =
              collator.compare(left.fields.material.normalized(QString::NormalizationForm_C),
                               right.fields.material.normalized(QString::NormalizationForm_C));
          break;
        case SortField::Location:
          comparison =
              collator.compare(left.fields.location.normalized(QString::NormalizationForm_C),
                               right.fields.location.normalized(QString::NormalizationForm_C));
          break;
        case SortField::Status:
          comparison = static_cast<int>(left.fields.status) - static_cast<int>(right.fields.status);
          break;
        case SortField::Photo:
          comparison = static_cast<int>(left.hasPhoto) - static_cast<int>(right.hasPhoto);
          break;
        }
        return comparison == 0 ? left.id < right.id
                               : (order == Qt::AscendingOrder ? comparison < 0 : comparison > 0);
      });
  return matches;
}

} // namespace woodwork
