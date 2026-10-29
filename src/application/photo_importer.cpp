#include "application/photo_importer.h"
#include <QCoreApplication>

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QtEndian>
#include <atomic>
#include <cstdio>
#include <mutex>

namespace woodwork {
namespace {
// Qt's JPEG decoder can recover damaged entropy data and return an image while
// emitting a warning. Treat that as invalid input as well. Serialize the scoped
// process-wide handler, capture only this thread, forward all other diagnostics.
std::mutex diagnosticsMutex;
std::atomic<QtMessageHandler> previousHandler{nullptr};
thread_local bool decoding = false;
thread_local bool decoderWarning = false;
void decoderMessage(QtMsgType type, const QMessageLogContext& context, const QString& message) {
  if (decoding && (type == QtWarningMsg || type == QtCriticalMsg)) {
    decoderWarning = true;
  }
  const auto handler = previousHandler.load();
  if (handler) {
    handler(type, context, message);
  } else {
    const auto encoded = message.toLocal8Bit();
    fprintf(stderr, "%s\n", encoded.constData());
  }
}
class DecoderDiagnostics final {
public:
  DecoderDiagnostics() : lock_(diagnosticsMutex) {
    decoderWarning = false;
    decoding = true;
    previous_ = qInstallMessageHandler(decoderMessage);
    previousHandler = previous_;
  }
  ~DecoderDiagnostics() {
    qInstallMessageHandler(previous_);
    decoding = false;
  }
  DecoderDiagnostics(const DecoderDiagnostics&) = delete;
  DecoderDiagnostics& operator=(const DecoderDiagnostics&) = delete;
  bool hasWarning() const { return decoderWarning; }

private:
  std::unique_lock<std::mutex> lock_;
  QtMessageHandler previous_ = nullptr;
};

[[noreturn]] void invalidPhoto() {
  throw PhotoError(QCoreApplication::translate(
      "App", "Das Foto ist beschädigt oder unvollständig. Bitte wähle eine andere Datei."));
}

// PNG readers may tolerate missing end chunks. Check framing and all chunk CRCs
// before decoding. No decompression or allocation proportional to declared sizes.
void checkPng(const QByteArray& bytes) {
  qsizetype offset = 8;
  bool ended = false;
  while (offset <= bytes.size() - 12) {
    const auto length = qFromBigEndian<quint32>(bytes.constData() + offset);
    if (length > static_cast<quint64>(bytes.size() - offset - 12)) {
      invalidPhoto();
    }
    quint32 crc = 0xffffffffU;
    for (qsizetype i = offset + 4; i < offset + 8 + length; ++i) {
      crc ^= static_cast<quint8>(bytes.at(i));
      for (int bit = 0; bit < 8; ++bit) {
        crc = (crc >> 1U) ^ ((crc & 1U) != 0U ? 0xedb88320U : 0U);
      }
    }
    if ((crc ^ 0xffffffffU) != qFromBigEndian<quint32>(bytes.constData() + offset + 8 + length)) {
      invalidPhoto();
    }
    const auto type = bytes.mid(offset + 4, 4);
    if (type == "acTL") {
      throw PhotoError(QCoreApplication::translate(
          "App",
          "Animierte PNG-Dateien werden nicht unterstützt. Bitte wähle ein einzelnes Foto."));
    }
    offset += 12 + length;
    if (type == "IEND") {
      ended = length == 0 && offset == bytes.size();
      break;
    }
  }
  if (!ended) {
    invalidPhoto();
  }
}
} // namespace

QByteArray readPhotoBytes(QIODevice& device) {
  // The extra byte detects growth after stat and also bounds sequential devices.
  QByteArray bytes;
  while (bytes.size() <= maxPhotoBytes) {
    const auto chunk = device.read(qMin<qint64>(64LL * 1024, maxPhotoBytes + 1 - bytes.size()));
    if (chunk.isEmpty()) {
      if (!device.atEnd()) {
        throw PhotoError(QCoreApplication::translate(
            "App",
            "Das Foto konnte nicht vollständig gelesen werden. Prüfe Datei und Zugriffsrechte."));
      }
      break;
    }
    bytes.append(chunk);
  }
  if (bytes.size() > maxPhotoBytes) {
    throw PhotoError(
        QCoreApplication::translate("App", "Das Foto ist zu groß. Erlaubt sind höchstens 10 MiB."));
  }
  return bytes;
}

ImportedPhoto decodePhoto(const QByteArray& bytes) {
  if (bytes.isEmpty() || bytes.size() > maxPhotoBytes) {
    throw PhotoError(
        QCoreApplication::translate("App", "Das Foto ist leer oder größer als 10 MiB."));
  }
  const DecoderDiagnostics diagnostics;
  QBuffer buffer;
  buffer.setData(bytes);
  buffer.open(QIODevice::ReadOnly);
  QImageReader reader(&buffer);
  reader.setDecideFormatFromContent(true);
  const auto format = reader.format().toLower();
  if (format != "jpeg" && format != "jpg" && format != "png") {
    throw PhotoError(QCoreApplication::translate(
        "App", "Bitte wähle ein gültiges JPEG- oder PNG-Foto. Der Dateiinhalt wird geprüft."));
  }
  const QSize size = reader.size();
  if (!size.isValid() || size.isEmpty()) {
    invalidPhoto();
  }
  if (size.width() > maxPhotoSide || size.height() > maxPhotoSide ||
      static_cast<qint64>(size.width()) * size.height() > maxPhotoPixels) {
    throw PhotoError(QCoreApplication::translate(
        "App", "Das Foto hat zu viele Pixel. Erlaubt sind 6000 Pixel je Seite "
               "und insgesamt 16 Millionen Pixel."));
  }
  if (format == "png") {
    checkPng(bytes);
  } else if (!bytes.startsWith(QByteArray::fromHex("ffd8")) ||
             !bytes.endsWith(QByteArray::fromHex("ffd9"))) {
    invalidPhoto();
  }
  reader.setAutoTransform(true);
  // Always decode the complete image, never validate only a scaled thumbnail.
  const auto image = reader.read();
  if (image.isNull() || reader.error() != QImageReader::UnknownError || diagnostics.hasWarning()) {
    invalidPhoto();
  }
  return {bytes, image.scaled(1000, 1000, Qt::KeepAspectRatio, Qt::SmoothTransformation)};
}

ImportedPhoto importPhoto(const QString& path) {
  if (!QFileInfo(path).isFile()) {
    throw PhotoError(QCoreApplication::translate("App", "Bitte wähle eine lesbare Bilddatei."));
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    throw PhotoError(QCoreApplication::translate(
        "App", "Das Foto kann nicht geöffnet werden. Prüfe Datei und Zugriffsrechte."));
  }
  if (file.size() > maxPhotoBytes) {
    throw PhotoError(
        QCoreApplication::translate("App", "Das Foto ist zu groß. Erlaubt sind höchstens 10 MiB."));
  }
  const auto bytes = readPhotoBytes(file);
  if (file.error() != QFileDevice::NoError) {
    throw PhotoError(
        QCoreApplication::translate("App", "Beim Lesen des Fotos ist ein Fehler aufgetreten."));
  }
  return decodePhoto(bytes);
}
} // namespace woodwork
