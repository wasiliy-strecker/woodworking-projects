#pragma once

#include <QString>

namespace woodwork {
QString defaultDataDirectory();
QString resolveDataDirectory(const QString& overridePath, bool explicitlyProvided);
} // namespace woodwork
