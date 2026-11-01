#pragma once

#include <QHeaderView>

namespace woodwork {

// Keep both directions visible, including on columns that are not currently sorted.
class SortHeader final : public QHeaderView {
public:
  explicit SortHeader(QWidget* parent = nullptr);

protected:
  void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override;
  QSize sectionSizeFromContents(int logicalIndex) const override;
};

} // namespace woodwork
