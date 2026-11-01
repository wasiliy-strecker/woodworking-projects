#pragma once
#include "application/colors.h"
#include <QIcon>
#include <QWidget>
class QLabel;
class QPushButton;
namespace woodwork {
QIcon colorIcon(const QString& value);
class ColorField final : public QWidget {
  Q_OBJECT
public:
  explicit ColorField(const QString& initialValue = {}, QWidget* parent = nullptr);
  void setTabOrderBetween(QWidget* previous, QWidget* next);
  QString value() const { return value_; }
  bool isChoosing() const { return choosing_; }

private:
  void refresh();
  QString value_;
  QPushButton* choose_;
  QLabel* description_;
  QPushButton* clear_;
  bool choosing_ = false;
};
} // namespace woodwork
