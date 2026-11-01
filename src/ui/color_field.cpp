#include "ui/color_field.h"
#include <QColorDialog>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScopedValueRollback>
namespace woodwork {
QIcon colorIcon(const QString& value) {
  const auto image = colorSwatch(value);
  return image.isNull() ? QIcon{} : QIcon(QPixmap::fromImage(image));
}
ColorField::ColorField(const QString& initialValue, QWidget* parent)
    : QWidget(parent), value_(initialValue),
      choose_(new QPushButton(QCoreApplication::translate("App", "Farbe auswählen"), this)),
      description_(new QLabel(this)), clear_(new QPushButton(QStringLiteral("×"), this)) {
  setObjectName(QStringLiteral("colorField"));
  choose_->setObjectName(QStringLiteral("chooseColor"));
  clear_->setObjectName(QStringLiteral("clearColor"));
  description_->setObjectName(QStringLiteral("colorValue"));
  description_->setTextFormat(Qt::PlainText);
  description_->setWordWrap(true);
  description_->setMinimumWidth(90);
  choose_->setAutoDefault(false);
  clear_->setAutoDefault(false);
  clear_->setFixedWidth(34);
  clear_->setToolTip(QCoreApplication::translate("App", "Farbe entfernen"));
  clear_->setAccessibleName(QCoreApplication::translate("App", "Farbe entfernen"));
  choose_->setFocusPolicy(Qt::StrongFocus);
  clear_->setFocusPolicy(Qt::StrongFocus);
  setFocusProxy(choose_);
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(choose_);
  layout->addWidget(description_, 1);
  layout->addWidget(clear_);
  connect(choose_, &QPushButton::clicked, this, [this] {
    if (choosing_)
      return;
    const QScopedValueRollback<bool> choosing(choosing_, true);
    QColorDialog dialog(this);
    dialog.setObjectName(QStringLiteral("colorDialog"));
    dialog.setWindowTitle(QCoreApplication::translate("App", "Farbe auswählen"));
    dialog.setOption(QColorDialog::ShowAlphaChannel);
    const auto initial = parseColor(value_);
    dialog.setCurrentColor(initial.isValid() ? initial : QColor(Qt::white));
    if (dialog.exec() == QDialog::Accepted) {
      value_ = encodeColor(dialog.selectedColor());
      refresh();
    }
  });
  connect(clear_, &QPushButton::clicked, this, [this] {
    value_.clear();
    refresh();
  });
  refresh();
}
// Use the same preceding/following order as QWidget::setTabOrder.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void ColorField::setTabOrderBetween(QWidget* previous, QWidget* next) {
  QWidget::setTabOrder(previous, choose_);
  QWidget::setTabOrder(choose_, clear_);
  QWidget::setTabOrder(clear_, next);
}
void ColorField::refresh() {
  const auto label = value_.isEmpty() ? QCoreApplication::translate("App", "Keine Farbe") : value_;
  description_->setText(label);
  choose_->setIcon(colorIcon(value_));
  choose_->setIconSize(QSize(24, 24));
  choose_->setAccessibleName(QCoreApplication::translate("App", "Farbe auswählen") +
                             QStringLiteral(": ") + label);
  clear_->setEnabled(!value_.isEmpty());
}
} // namespace woodwork
