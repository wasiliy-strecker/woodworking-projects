#include "ui/photo_view.h"
#include <QPixmap>
#include <QResizeEvent>
namespace woodwork {
PhotoView::PhotoView(QWidget* parent) : QLabel(parent) {
  setAlignment(Qt::AlignCenter);
  setWordWrap(true);
  setTextFormat(Qt::PlainText);
  setMinimumSize(120, 100);
  setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
  setAccessibleName(QStringLiteral("Projektfoto"));
  showMessage(QStringLiteral("Kein Foto"));
}
void PhotoView::showPhoto(const QImage& image) {
  image_ = image;
  render();
}
void PhotoView::showMessage(const QString& message) {
  image_ = {};
  clear();
  setText(message);
  setAccessibleDescription(message);
}
void PhotoView::render() {
  if (!image_.isNull()) {
    setPixmap(
        QPixmap::fromImage(image_).scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    setAccessibleDescription(QStringLiteral("Gespeichertes oder ausgewähltes Projektfoto"));
  }
}
void PhotoView::resizeEvent(QResizeEvent* event) {
  QLabel::resizeEvent(event);
  render();
}
} // namespace woodwork
