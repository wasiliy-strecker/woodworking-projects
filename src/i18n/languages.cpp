#include "i18n/languages.h"

#include <QCoreApplication>
#include <stdexcept>

static void initializeTranslations() {
  Q_INIT_RESOURCE(app_translations);
  Q_INIT_RESOURCE(qt_translations);
}

namespace woodwork {
QStringList supportedLanguages() {
  return {QStringLiteral("de-DE"), QStringLiteral("en-US"), QStringLiteral("es-ES"),
          QStringLiteral("fr-FR"), QStringLiteral("it-IT"), QStringLiteral("pt-BR")};
}

QString languageFor(const QString& preference, const QLocale& system) {
  if (preference == QStringLiteral("auto")) {
    const auto language = system.language();
    for (const auto& tag : supportedLanguages()) {
      if (QLocale(tag).language() == language) {
        return tag;
      }
    }
  } else if (supportedLanguages().contains(preference)) {
    return preference;
  }
  return QStringLiteral("en-US");
}

QString languageName(const QString& tag) {
  const QLocale locale(tag);
  return locale.nativeLanguageName() + QStringLiteral(" (") + locale.nativeTerritoryName() +
         QStringLiteral(")");
}

Translations::Translations() { initializeTranslations(); }

void Translations::install(const QString& preference, const QLocale& system) {
  QCoreApplication::removeTranslator(&app_);
  QCoreApplication::removeTranslator(&qt_);
  const auto tag = languageFor(preference, system);
  const QLocale locale(tag);
  QLocale::setDefault(locale);
  if (!app_.load(QStringLiteral(":/i18n/app_") + tag + QStringLiteral(".qm"))) {
    throw std::runtime_error("Missing embedded application translations");
  }
  if (tag != QStringLiteral("en-US")) {
    const auto qtLanguage = tag == QStringLiteral("pt-BR") ? QStringLiteral("pt_BR") : tag.left(2);
    if (!qt_.load(QStringLiteral(":/i18n/qtbase_") + qtLanguage + QStringLiteral(".qm"))) {
      throw std::runtime_error("Missing embedded Qt translations");
    }
    QCoreApplication::installTranslator(&qt_);
  }
  QCoreApplication::installTranslator(&app_);
}
} // namespace woodwork
