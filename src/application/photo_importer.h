#pragma once

#include <QByteArray>
#include <QIODevice>
#include <QImage>
#include <QString>
#include <stdexcept>

namespace woodwork {
inline constexpr qint64 maxPhotoBytes = 10LL * 1024 * 1024;
inline constexpr int maxPhotoSide = 6000;
inline constexpr qint64 maxPhotoPixels = 16000000;

class PhotoError final : public std::runtime_error {
public:
  explicit PhotoError(const QString& message) : std::runtime_error(message.toStdString()) {}
};
struct ImportedPhoto {
  QByteArray bytes;
  QImage preview;
};
// Reentrant. Call outside the GUI thread. No QPixmap or widget ownership.
QByteArray readPhotoBytes(QIODevice& device);
ImportedPhoto decodePhoto(const QByteArray& bytes);
ImportedPhoto importPhoto(const QString& path);
} // namespace woodwork
