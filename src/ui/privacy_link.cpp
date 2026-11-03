#include "ui/privacy_link.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QMessageBox>

namespace woodwork {
QUrl privacyPolicyUrl() {
  return QUrl(QStringLiteral(
      "https://github.com/wasiliy-strecker/woodworking-projects/blob/main/PRIVACY.md"));
}

bool openInDefaultBrowser(const QUrl& url) { return QDesktopServices::openUrl(url); }

void openPrivacyPolicy(QWidget* parent, const BrowserOpener& browser) {
  const auto url = privacyPolicyUrl();
  if (browser && browser(url)) {
    return;
  }
  QMessageBox message(
      QMessageBox::Warning, QCoreApplication::translate("App", "Datenschutz"),
      QCoreApplication::translate(
          "App", "Die Datenschutzerklärung konnte nicht im Browser geöffnet werden. Kopiere diese "
                 "Adresse und öffne sie in deinem Browser. Die Erklärung ist auf Englisch.") +
          QStringLiteral("\n\n") + url.toString(),
      QMessageBox::Close, parent);
  message.setObjectName(QStringLiteral("privacyLinkError"));
  message.setTextFormat(Qt::PlainText);
  message.setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
  message.button(QMessageBox::Close)->setText(QCoreApplication::translate("App", "Schließen"));
  message.exec();
}
} // namespace woodwork
