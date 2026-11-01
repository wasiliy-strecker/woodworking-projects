#include "ui/sort_header.h"

#include <QAbstractItemModel>
#include <QPainter>
#include <QStyleOptionHeader>
#include <algorithm>

namespace woodwork {
namespace {
constexpr int margin = 10;
constexpr int arrowWidth = 9;
constexpr int arrowHeight = 5;
constexpr int arrowGap = 3;
constexpr int textGap = 8;
} // namespace

SortHeader::SortHeader(QWidget* parent) : QHeaderView(Qt::Horizontal, parent) {
  setSectionsClickable(true);
  setSectionsMovable(false);
  setSortIndicatorShown(true);
  setSortIndicator(0, Qt::AscendingOrder);
  setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  auto headerFont = font();
  headerFont.setBold(true);
  setFont(headerFont);
}

QSize SortHeader::sectionSizeFromContents(int logicalIndex) const {
  auto size = QHeaderView::sectionSizeFromContents(logicalIndex);
  const auto label =
      model() ? model()->headerData(logicalIndex, orientation()).toString() : QString();
  size.setWidth(std::max(size.width(), fontMetrics().horizontalAdvance(label) + 3 * margin +
                                           textGap + arrowWidth));
  size.setHeight(std::max(size.height(), fontMetrics().height() + 12));
  return size;
}

void SortHeader::paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const {
  if (!rect.isValid()) {
    return;
  }
  QStyleOptionHeader option;
  initStyleOption(&option);
  initStyleOptionForIndex(&option, logicalIndex);
  option.rect = rect;
  const auto label =
      model() ? model()->headerData(logicalIndex, orientation()).toString() : option.text;
  option.text.clear();
  option.sortIndicator = QStyleOptionHeader::None;
  painter->save();
  painter->setClipRect(rect);
  style()->drawControl(QStyle::CE_Header, &option, painter, this);
  painter->setFont(font());
  const auto labelRect = rect.adjusted(margin, 0, -margin - textGap - arrowWidth, 0);
  const auto elided = fontMetrics().elidedText(label, Qt::ElideRight, labelRect.width());
  const auto color = option.palette.color(QPalette::ButtonText);
  painter->setPen(color);
  painter->drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, elided);

  const int x = labelRect.left() + fontMetrics().horizontalAdvance(elided) + textGap;
  const int y = rect.center().y() - (2 * arrowHeight + arrowGap) / 2;
  painter->setPen(Qt::NoPen);
  for (const auto order : {Qt::AscendingOrder, Qt::DescendingOrder}) {
    auto arrowColor = color;
    if (sortIndicatorSection() != logicalIndex || sortIndicatorOrder() != order) {
      arrowColor.setAlphaF(0.35F);
    }
    painter->setBrush(arrowColor);
    const QPolygon triangle = order == Qt::AscendingOrder
                                  ? QPolygon{{x, y + arrowHeight},
                                             {x + arrowWidth / 2, y},
                                             {x + arrowWidth, y + arrowHeight}}
                                  : QPolygon{{x, y + arrowHeight + arrowGap},
                                             {x + arrowWidth, y + arrowHeight + arrowGap},
                                             {x + arrowWidth / 2, y + 2 * arrowHeight + arrowGap}};
    painter->drawPolygon(triangle);
  }
  painter->restore();
}

} // namespace woodwork
