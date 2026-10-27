#pragma once

#include "domain/project.h"
#include <QWidget>

class QCheckBox;
class QLineEdit;
class QVBoxLayout;
class QPushButton;

namespace woodwork {
class ChecklistEditor final : public QWidget {
  Q_OBJECT
public:
  explicit ChecklistEditor(const Checklist& steps, QWidget* parent = nullptr);
  Checklist steps() const;

signals:
  void rowAdded(QWidget* field);

private:
  struct Row {
    QUuid id;
    qint64 position;
    QWidget* widget;
    QCheckBox* completed;
    QLineEdit* text;
  };
  void append(const ChecklistStep& step, bool focus);
  void remove(const QUuid& id);
  void updateLabels();
  QVector<Row> rows_;
  QVBoxLayout* rowsLayout_;
  QPushButton* add_;
};
} // namespace woodwork
