#include "application/colors.h"
#include <QPainter>
#include <QRegularExpression>
namespace woodwork {
QColor parseColor(const QString& value) {
  static const QRegularExpression pattern(QStringLiteral("\\A#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?\\z"));
  if (!pattern.match(value).hasMatch())
    return {};
  QColor color(value.first(7));
  if (value.size() == 9)
    color.setAlpha(value.sliced(7, 2).toInt(nullptr, 16));
  return color;
}
QString encodeColor(const QColor& color) {
  if (!color.isValid())
    return {};
  auto value = color.name(QColor::HexRgb).toUpper();
  if (color.alpha() != 255)
    value += QStringLiteral("%1").arg(color.alpha(), 2, 16, QChar('0')).toUpper();
  return value;
}
QImage colorSwatch(const QString& value, int size) {
  const auto color = parseColor(value);
  if (!color.isValid() || size <= 0)
    return {};
  QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::white);
  QPainter painter(&image);
  constexpr int tile = 4;
  for (int y = 0; y < size; y += tile)
    for (int x = 0; x < size; x += tile)
      if ((x / tile + y / tile) % 2 == 0)
        painter.fillRect(x, y, tile, tile, QColor(205, 205, 205));
  painter.fillRect(image.rect(), color);
  painter.setPen(QColor(95, 95, 95));
  painter.drawRect(0, 0, size - 1, size - 1);
  return image;
}
} // namespace woodwork
