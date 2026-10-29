#pragma once

#include <QLocale>
#include <QStringList>
#include <QTranslator>

namespace woodwork {
QStringList supportedLanguages();
QString languageFor(const QString& preference, const QLocale& system = QLocale::system());
QString languageName(const QString& tag);

// Install before creating windows or starting workers. A change takes effect on restart.
class Translations final {
public:
  Translations();
  void install(const QString& preference, const QLocale& system = QLocale::system());

private:
  QTranslator app_;
  QTranslator qt_;
};
} // namespace woodwork
