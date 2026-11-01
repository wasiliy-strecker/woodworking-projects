#include "application/collection_codec.h"
#include "application/photo_importer.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <cmath>
namespace woodwork {
namespace {
[[noreturn]] void invalid() {
  throw TransferError(
      QCoreApplication::translate("App", "Die Sammlungsdatei enthält ungültige oder zu große "
                                         "Daten. Die Sammlung bleibt unverändert."));
}
void check(const ProjectDraft& draft) {
  if (validateDraft(draft) != draft)
    invalid();
  for (const auto& text : {draft.name, draft.material, draft.location, draft.note}) {
    if (!text.isValidUtf16() || text.size() > 1000000)
      invalid();
  }
  if (!draft.photo.isEmpty())
    (void)decodePhoto(draft.photo);
  if (draft.steps.size() > 10000)
    invalid();
  for (const auto& step : draft.steps) {
    if (!step.text.isValidUtf16() || step.text.size() > 1000000 ||
        step.position > 9007199254740991LL)
      invalid();
  }
}
} // namespace
QJsonObject encodeEntry(const ProjectDraft& draft) {
  check(draft);
  QJsonObject object{{QStringLiteral("name"), draft.name},
                     {QStringLiteral("material"), draft.material},
                     {QStringLiteral("location"), draft.location},
                     {QStringLiteral("note"), draft.note},
                     {QStringLiteral("photoBase64"),
                      draft.photo.isEmpty()
                          ? QJsonValue(QJsonValue::Null)
                          : QJsonValue(QString::fromLatin1(draft.photo.toBase64()))}};
  object.insert(QStringLiteral("status"), storedStatus(draft.status));
  QJsonArray list;
  for (const auto& step : draft.steps)
    list.append(QJsonObject{{QStringLiteral("id"), step.id.toString(QUuid::WithoutBraces)},
                            {QStringLiteral("text"), step.text},
                            {QStringLiteral("completed"), step.completed},
                            {QStringLiteral("position"), step.position}});
  object.insert(QStringLiteral("steps"), list);
  return object;
}
ProjectDraft decodeEntry(const QJsonObject& object) {
  if (object.size() != 7)
    invalid();
  for (const auto& field : {QStringLiteral("name"), QStringLiteral("material"),
                            QStringLiteral("location"), QStringLiteral("note")})
    if (!object.value(field).isString())
      invalid();
  ProjectDraft draft;
  draft.name = object.value(QStringLiteral("name")).toString();
  draft.material = object.value(QStringLiteral("material")).toString();
  draft.location = object.value(QStringLiteral("location")).toString();
  draft.note = object.value(QStringLiteral("note")).toString();
  const auto photo = object.value(QStringLiteral("photoBase64"));
  if (!photo.isNull()) {
    if (!photo.isString())
      invalid();
    const auto encoded = photo.toString().toLatin1();
    if (encoded.isEmpty() || encoded.size() > ((maxPhotoBytes + 2) / 3) * 4)
      invalid();
    const auto decoded =
        QByteArray::fromBase64Encoding(encoded, QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.toBase64() != encoded)
      invalid();
    draft.photo = decoded.decoded;
  }
  if (!object.value(QStringLiteral("status")).isString())
    invalid();
  draft.status = statusFromStorage(object.value(QStringLiteral("status")).toString());
  if (!object.value(QStringLiteral("steps")).isArray())
    invalid();
  const auto list = object.value(QStringLiteral("steps")).toArray();
  if (list.size() > 10000)
    invalid();
  for (const auto& value : list) {
    const auto step = value.toObject();
    if (!value.isObject() || step.size() != 4 || !step.value(QStringLiteral("id")).isString() ||
        !step.value(QStringLiteral("text")).isString() ||
        !step.value(QStringLiteral("completed")).isBool() ||
        !step.value(QStringLiteral("position")).isDouble())
      invalid();
    const auto idText = step.value(QStringLiteral("id")).toString();
    const QUuid id(idText);
    const double position = step.value(QStringLiteral("position")).toDouble();
    if (id.isNull() || id.toString(QUuid::WithoutBraces) != idText || position < 0 ||
        position > 9007199254740991.0 || std::floor(position) != position)
      invalid();
    draft.steps.append({id, step.value(QStringLiteral("text")).toString(),
                        step.value(QStringLiteral("completed")).toBool(),
                        static_cast<qint64>(position)});
  }
  check(draft);
  return draft;
}
QByteArray entryFingerprint(const ProjectDraft& draft) {
  auto object = encodeEntry(draft);
  QJsonArray steps;
  for (const auto& value : object.value(QStringLiteral("steps")).toArray()) {
    auto step = value.toObject();
    step.remove(QStringLiteral("id"));
    steps.append(step);
  }
  object.insert(QStringLiteral("steps"), steps);
  return QCryptographicHash::hash(QJsonDocument(object).toJson(QJsonDocument::Compact),
                                  QCryptographicHash::Sha256);
}
} // namespace woodwork
