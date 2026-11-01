#include "ui/information_dialog.h"
#include "i18n/privacy_notice.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFile>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

static void initializeNotices() { Q_INIT_RESOURCE(app_notices); }

namespace woodwork {
namespace {
QString licenseText() {
  initializeNotices();
  QString result = QCoreApplication::translate(
      "App", "Die folgenden Hinweise und Lizenztexte beschreiben die verwendeten Fremdkomponenten. "
             "Rechtliche Originaltexte werden in ihrer Originalsprache angezeigt. Weitere "
             "Laufzeithinweise befinden sich in den Lizenzordnern der installierten App.");
  QStringList paths;
  QDirIterator files(QStringLiteral(":/notices"), QDir::Files, QDirIterator::Subdirectories);
  while (files.hasNext()) {
    paths.append(files.next());
  }
  paths.sort();
  for (const auto& path : paths) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      return QCoreApplication::translate(
          "App",
          "Die Informationen konnten nicht geladen werden. Bitte installiere die App erneut.");
    }
    result += QStringLiteral("\n\n────────────────────\n") + path.sliced(10) +
              QStringLiteral("\n\n") + QString::fromUtf8(file.readAll());
  }
  return result;
}
} // namespace

InformationDialog::InformationDialog(InformationPage page, const QString& dataDirectory,
                                     QWidget* parent)
    : QDialog(parent) {
  QString content;
  QString suffix;
  switch (page) {
  case InformationPage::About:
    suffix = QStringLiteral("About");
    setWindowTitle(QCoreApplication::translate("App", "Über Woodworking Projects"));
    content =
        QStringLiteral("Woodworking Projects\n") +
        QCoreApplication::translate("App", "Version %1")
            .arg(QCoreApplication::applicationVersion()) +
        QStringLiteral("\n\n") +
        QCoreApplication::translate("App", "Deine Holzideen. Schritt für Schritt verwirklicht.") +
        QStringLiteral("\n") +
        QCoreApplication::translate("App", "Lokal auf diesem Computer gespeichert") +
        QStringLiteral("\n\n") +
        QCoreApplication::translate("App", "Datenverzeichnis: %1").arg(dataDirectory) +
        QStringLiteral("\n\n") +
        QCoreApplication::translate("App", "Entwicklung: Wasiliy Strecker · AppFabrik AI") +
        QStringLiteral("\nsupport@appfabrik-ai.de");
    break;
  case InformationPage::Privacy:
    suffix = QStringLiteral("Privacy");
    setWindowTitle(
        QCoreApplication::translate("App", "Datenschutzerklärung für Woodworking Projects"));
    content = privacyNotice();
    break;
  case InformationPage::Licenses:
    suffix = QStringLiteral("Licenses");
    setWindowTitle(QCoreApplication::translate("App", "Lizenzen"));
    content = licenseText();
    break;
  }
  setObjectName(suffix.toLower() + QStringLiteral("Dialog"));
  auto* layout = new QVBoxLayout(this);
  auto* text = new QPlainTextEdit(this);
  text->setObjectName(suffix.toLower() + QStringLiteral("Text"));
  text->setAccessibleName(windowTitle());
  text->setReadOnly(true);
  text->setTabChangesFocus(true);
  text->setPlainText(content);
  layout->addWidget(text);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  auto* close = buttons->button(QDialogButtonBox::Close);
  close->setObjectName(QStringLiteral("close") + suffix);
  close->setText(QCoreApplication::translate("App", "Schließen"));
  close->setIcon(QIcon());
  close->setStyleSheet(QStringLiteral(
      "QPushButton { background-color: #b3261e; color: #ffffff; border: 2px solid #b3261e; "
      "border-radius: 4px; padding: 7px 18px; font-weight: bold; } "
      "QPushButton:hover { background-color: #9b211a; border-color: #9b211a; } "
      "QPushButton:pressed { background-color: #7d1b15; border-color: #7d1b15; } "
      "QPushButton:focus { border-color: #f4b1ac; }"));
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);
  const auto available = screen()->availableGeometry();
  resize(qMin(780, available.width() - 40),
         qMin(page == InformationPage::About ? 380 : 620, available.height() - 40));
}
} // namespace woodwork
