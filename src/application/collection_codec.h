#pragma once
#include "application/collection_file.h"
#include <QJsonObject>
namespace woodwork {
QJsonObject encodeEntry(const ProjectDraft& draft);
ProjectDraft decodeEntry(const QJsonObject& object);
QByteArray entryFingerprint(const ProjectDraft& draft);
} // namespace woodwork
