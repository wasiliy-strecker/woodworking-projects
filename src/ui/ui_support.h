#pragma once

#include <QWidget>

namespace woodwork {
bool confirmDiscard(QWidget* parent);
bool confirmDelete(QWidget* parent, const QString& name);
} // namespace woodwork
