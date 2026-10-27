#include "application/photo_importer.h"
#include "persistence/sqlite_project_repository.h"

#include <QBuffer>
#include <QFile>
#include <QImageWriter>
#include <QTemporaryDir>
#include <QtTest>

using namespace woodwork;
namespace {
QByteArray picture(const char* format = "PNG", QSize size = {32, 24}, QColor color = Qt::red) {
  QImage image(size, QImage::Format_RGB32);
  image.fill(color);
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  if (!image.save(&buffer, format))
    throw std::runtime_error("Synthetic image encoding failed");
  return bytes;
}
} // namespace
class PhotoTest final : public QObject {
  Q_OBJECT
private slots:
  void validFormats_data() {
    QTest::addColumn<QByteArray>("format");
    QTest::newRow("jpeg") << QByteArray("JPEG");
    QTest::newRow("png") << QByteArray("PNG");
  }
  void validFormats() {
    QFETCH(QByteArray, format);
    QTemporaryDir directory;
    const auto bytes = picture(format.constData());
    QFile file(directory.filePath(QStringLiteral("synthetic.wrong-extension")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(bytes), bytes.size());
    file.close();
    const auto imported = importPhoto(file.fileName());
    QCOMPARE(imported.bytes, bytes);
    QCOMPARE(imported.preview.size(), QSize(1000, 750));
    QCOMPARE(imported.preview.pixelColor(200, 200).red(), 254 + (format == "PNG" ? 1 : 0));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), bytes);
  }
  void invalidContent_data() {
    QTest::addColumn<QByteArray>("bytes");
    QTest::newRow("empty") << QByteArray();
    QTest::newRow("text") << QByteArray("This is not a photo");
    QTest::newRow("bmp") << picture("BMP");
    QTest::newRow("png-truncated") << picture().chopped(12);
    auto png = picture();
    png[png.size() - 15] = static_cast<char>(png.at(png.size() - 15) ^ 1);
    QTest::newRow("png-crc") << png;
    QTest::newRow("jpeg-truncated") << picture("JPEG").chopped(30);
    auto jpeg = picture("JPEG", {400, 300});
    jpeg.remove(jpeg.size() / 2, 100);
    QTest::newRow("jpeg-entropy-with-eoi") << jpeg;
    QTest::newRow("oversized-bytes") << QByteArray(maxPhotoBytes + 1, 'x');
  }
  void invalidContent() {
    QFETCH(QByteArray, bytes);
    QVERIFY_EXCEPTION_THROWN(decodePhoto(bytes), PhotoError);
  }
  void dimensionLimits_data() {
    QTest::addColumn<QSize>("size");
    QTest::addColumn<bool>("accepted");
    QTest::newRow("side-boundary") << QSize(6000, 1) << true;
    QTest::newRow("width-too-large") << QSize(6001, 1) << false;
    QTest::newRow("height-too-large") << QSize(1, 6001) << false;
    QTest::newRow("pixel-boundary") << QSize(4000, 4000) << true;
    QTest::newRow("pixels-too-large") << QSize(4001, 4000) << false;
  }
  void dimensionLimits() {
    QFETCH(QSize, size);
    QFETCH(bool, accepted);
    const auto bytes = picture("PNG", size);
    if (accepted)
      QVERIFY(!decodePhoto(bytes).preview.isNull());
    else
      QVERIFY_EXCEPTION_THROWN(decodePhoto(bytes), PhotoError);
  }
  void boundedReadIncludingGrowingDevice() {
    QByteArray exact(maxPhotoBytes, 'x');
    QBuffer buffer(&exact);
    QVERIFY(buffer.open(QIODevice::ReadOnly));
    QCOMPARE(readPhotoBytes(buffer).size(), maxPhotoBytes);
    class GrowingDevice final : public QIODevice {
    public:
      GrowingDevice() { open(QIODevice::ReadOnly); }
      qint64 size() const override { return 1; }
      bool atEnd() const override { return count >= maxPhotoBytes + 1000; }
      qint64 count = 0;

    protected:
      qint64 readData(char* data, qint64 maxSize) override {
        const auto amount = qMin(maxSize, maxPhotoBytes + 1000 - count);
        std::fill_n(data, amount, 'x');
        count += amount;
        return amount;
      }
      qint64 writeData(const char*, qint64) override { return -1; }
    } growing;
    QVERIFY_EXCEPTION_THROWN(readPhotoBytes(growing), PhotoError);
    // QIODevice may prefetch one internal 16 KiB block, never the whole file.
    QVERIFY(growing.count <= maxPhotoBytes + 16384);
  }
  void missingUnreadableAndOversizedFiles() {
    QTemporaryDir directory;
    QVERIFY_EXCEPTION_THROWN(importPhoto(directory.filePath(QStringLiteral("missing.png"))),
                             PhotoError);
    QVERIFY_EXCEPTION_THROWN(importPhoto(directory.path()), PhotoError);
    QFile file(directory.filePath(QStringLiteral("too-big.png")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.resize(maxPhotoBytes + 1));
    file.close();
    QVERIFY_EXCEPTION_THROWN(importPhoto(file.fileName()), PhotoError);
  }
  void orientationIsApplied() {
    auto jpeg = picture("JPEG", {40, 20});
    // Synthetic EXIF little-endian TIFF with orientation 6, 90 degrees clockwise.
    const auto exif = QByteArray::fromHex(
        "ffe1002245786966000049492a0008000000010012010300010000000600000000000000");
    jpeg.insert(2, exif);
    const auto result = decodePhoto(jpeg);
    QCOMPARE(result.preview.size(), QSize(500, 1000));
    QCOMPARE(result.bytes, jpeg);
  }
  void persistenceDuplicatesReplacementRemovalAndNoSource() {
    QTemporaryDir directory;
    const auto firstPhoto = picture();
    const auto secondPhoto = picture("JPEG", {30, 40}, Qt::blue);
    qint64 firstId = 0;
    qint64 secondId = 0;
    {
      QFile source(directory.filePath(QStringLiteral("source.png")));
      QVERIFY(source.open(QIODevice::WriteOnly));
      source.write(firstPhoto);
      source.close();
      SqliteProjectRepository repository(directory.path());
      repository.initialize();
      ProjectDraft draft{QStringLiteral("Doppelt"),           {}, {}, {}, ProjectStatus::Started,
                         importPhoto(source.fileName()).bytes};
      const auto first = repository.create(draft);
      firstId = first.id;
      QVERIFY(first.hasPhoto);
      QVERIFY(first.fields.photo.isEmpty());
      draft.photo = secondPhoto;
      secondId = repository.create(draft).id;
      QVERIFY(source.remove());
    }
    {
      SqliteProjectRepository repository(directory.path());
      repository.initialize();
      QCOMPARE(repository.find(firstId).photo, firstPhoto);
      QCOMPARE(repository.find(secondId).photo, secondPhoto);
      const auto rows = repository.findAll();
      for (const auto& row : rows)
        QVERIFY(row.fields.photo.isEmpty());
      auto draft = repository.find(firstId);
      draft.photo = secondPhoto;
      repository.update(firstId, draft);
      QCOMPARE(repository.find(firstId).photo, secondPhoto);
      draft.photo.clear();
      QVERIFY(!repository.update(firstId, draft).hasPhoto);
      QVERIFY(repository.find(firstId).photo.isEmpty());
      QCOMPARE(repository.find(secondId).photo, secondPhoto);
      repository.remove(secondId);
      QVERIFY_EXCEPTION_THROWN(repository.find(secondId), StorageError);
      QVERIFY_EXCEPTION_THROWN(repository.create({QStringLiteral("Ungültig"),
                                                  {},
                                                  {},
                                                  {},
                                                  ProjectStatus::Planned,
                                                  QByteArray("broken")}),
                               PhotoError);
      QCOMPARE(repository.findAll().size(), 1);
    }
  }
};
QTEST_GUILESS_MAIN(PhotoTest)
#include "photo_test.moc"
