#include "ui/checklist_editor.h"
#include <QCoreApplication>

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>

namespace woodwork {
ChecklistEditor::ChecklistEditor(const Checklist& steps, QWidget* parent)
    : QWidget(parent), rowsLayout_(new QVBoxLayout),
      add_(
          new QPushButton(QCoreApplication::translate("App", "Arbeitsschritt hin&zufügen"), this)) {
  setObjectName(QStringLiteral("checklistEditor"));
  setFocusPolicy(Qt::StrongFocus);
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  auto* hint = new QLabel(
      QCoreApplication::translate("App", "Arbeitsschritte. Häkchen und Texte werden erst mit dem "
                                         "Projekt gespeichert. Der Status bleibt unabhängig."),
      this);
  hint->setWordWrap(true);
  layout->addWidget(hint);
  layout->addLayout(rowsLayout_);
  add_->setObjectName(QStringLiteral("addStep"));
  add_->setAutoDefault(false);
  layout->addWidget(add_, 0, Qt::AlignLeft);
  for (const auto& step : steps)
    append(step, false);
  connect(add_, &QPushButton::clicked, this, [this] {
    const auto position = rows_.isEmpty() ? 0 : rows_.last().position + 1;
    append({QUuid::createUuid(), {}, false, position}, true);
  });
  updateLabels();
}

Checklist ChecklistEditor::steps() const {
  Checklist result;
  for (const auto& row : rows_)
    result.append({row.id, row.text->text(), row.completed->isChecked(), row.position});
  return result;
}

void ChecklistEditor::append(const ChecklistStep& step, bool focus) {
  auto* widget = new QWidget(this);
  widget->setProperty("stepId", step.id.toString(QUuid::WithoutBraces));
  auto* layout = new QVBoxLayout(widget);
  layout->setContentsMargins(0, 4, 0, 8);
  layout->setSpacing(4);
  auto* line = new QHBoxLayout;
  auto* completed = new QCheckBox(widget);
  completed->setObjectName(QStringLiteral("stepCompleted"));
  completed->setChecked(step.completed);
  auto* text = new QLineEdit(step.text, widget);
  text->setObjectName(QStringLiteral("stepText"));
  text->setMaxLength(1000000);
  text->setMinimumWidth(40);
  line->addWidget(completed);
  line->addWidget(text, 1);
  layout->addLayout(line);
  auto* removeButton =
      new QPushButton(QCoreApplication::translate("App", "Schritt entfernen"), widget);
  removeButton->setObjectName(QStringLiteral("removeStep"));
  removeButton->setAutoDefault(false);
  layout->addWidget(removeButton, 0, Qt::AlignRight);
  rows_.append({step.id, step.position, widget, completed, text});
  rowsLayout_->addWidget(widget);
  connect(removeButton, &QPushButton::clicked, this, [this, id = step.id] { remove(id); });
  updateLabels();
  // Rows added later still precede the add action in keyboard traversal.
  QWidget* previous =
      rows_.size() > 1 ? rows_[rows_.size() - 2].widget->findChild<QPushButton*>() : nullptr;
  if (previous)
    setTabOrder(previous, completed);
  setTabOrder(completed, text);
  setTabOrder(text, removeButton);
  setTabOrder(removeButton, add_);
  if (focus) {
    text->setFocus();
    emit rowAdded(text);
  }
}

void ChecklistEditor::remove(const QUuid& id) {
  const auto found =
      std::find_if(rows_.begin(), rows_.end(), [&id](const Row& row) { return row.id == id; });
  if (found == rows_.end())
    return;
  auto* widget = found->widget;
  rows_.erase(found);
  rowsLayout_->removeWidget(widget);
  widget->hide();
  widget->deleteLater();
  updateLabels();
  add_->setFocus();
}

void ChecklistEditor::updateLabels() {
  setFocusProxy(rows_.isEmpty() ? static_cast<QWidget*>(add_) : rows_.first().completed);
  for (qsizetype i = 0; i < rows_.size(); ++i) {
    rows_[i].text->setAccessibleName(
        QCoreApplication::translate("App", "Arbeitsschritt %1").arg(QLocale().toString(i + 1)));
    rows_[i].text->setPlaceholderText(
        QCoreApplication::translate("App", "Arbeitsschritt %1 *").arg(QLocale().toString(i + 1)));
    rows_[i].completed->setAccessibleName(
        QCoreApplication::translate("App", "Arbeitsschritt %1 erledigt")
            .arg(QLocale().toString(i + 1)));
    rows_[i].widget->findChild<QPushButton*>()->setAccessibleName(
        QCoreApplication::translate("App", "Arbeitsschritt %1 entfernen")
            .arg(QLocale().toString(i + 1)));
  }
  add_->setEnabled(rows_.isEmpty() || rows_.last().position < std::numeric_limits<qint64>::max());
}
} // namespace woodwork
