#include "ui/main_window.h"
#include "ui/language_dialog.h"
#include "ui/photo_view.h"
#include "ui/ui_support.h"
#include <QCoreApplication>
#include <QLocale>
#include <QScrollArea>

#include <QApplication>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QFile>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

namespace woodwork {

MainWindow::MainWindow(StorageController& controller, QString dataDirectory,
                       QString languagePreference, QWidget* parent)
    : QMainWindow(parent), controller_(controller), dataDirectory_(std::move(dataDirectory)),
      languagePreference_(std::move(languagePreference)), search_(new QLineEdit(this)),
      table_(new QTableWidget(this)), note_(new QPlainTextEdit(this)),
      checklist_(new QPlainTextEdit(this)), photo_(new PhotoView(this)),
      detailTitle_(new QLabel(this)), state_(new QLabel(this)), feedback_(new QLabel(this)),
      count_(new QLabel(this)),
      add_(new QPushButton(QCoreApplication::translate("App", "Projekt hinzufügen"), this)),
      edit_(new QPushButton(QCoreApplication::translate("App", "&Bearbeiten"), this)),
      remove_(new QPushButton(QCoreApplication::translate("App", "&Löschen"), this)),
      reload_(new QPushButton(QCoreApplication::translate("App", "Neu laden"), this)),
      language_(new QPushButton(QCoreApplication::translate("App", "Sprache"), this)) {
  setWindowTitle(QStringLiteral("HolzwerkProjekt"));
  setObjectName(QStringLiteral("mainWindow"));
  resize(1100, 800);
  setMinimumSize(440, 420);
  auto* central = new QWidget(this);
  auto* root = new QVBoxLayout(central);
  root->setContentsMargins(24, 22, 24, 22);
  root->setSpacing(14);
  auto* header = new QHBoxLayout;
  auto* title = new QLabel(QStringLiteral("HolzwerkProjekt"), central);
  QFont titleFont = title->font();
  titleFont.setPointSize(22);
  titleFont.setBold(true);
  title->setFont(titleFont);
  header->addWidget(title, 1);
  auto* about = new QPushButton(QCoreApplication::translate("App", "Über die App"), central);
  root->addLayout(header);
  auto* settings = new QHBoxLayout;
  settings->addStretch();
  language_->setObjectName(QStringLiteral("changeLanguage"));
  settings->addWidget(language_);
  settings->addWidget(about);
  root->addLayout(settings);
  auto* subtitle = new QLabel(
      QCoreApplication::translate("App", "Deine Holzideen. Schritt für Schritt verwirklicht."),
      central);
  subtitle->setWordWrap(true);
  root->addWidget(subtitle);
  search_->setObjectName(QStringLiteral("searchProjects"));
  search_->setAccessibleName(QCoreApplication::translate("App", "Sammlung durchsuchen"));
  search_->setPlaceholderText(
      QCoreApplication::translate("App", "Projekte und Arbeitsschritte durchsuchen …"));
  search_->setClearButtonEnabled(true);
  root->addWidget(search_);
  auto* clear = new QPushButton(QCoreApplication::translate("App", "Suche leeren"), central);
  clear->setObjectName(QStringLiteral("clearSearch"));
  auto* searchInfo = new QHBoxLayout;
  searchInfo->addWidget(count_, 1);
  searchInfo->addWidget(clear);
  root->addLayout(searchInfo);

  auto* splitter = new QSplitter(Qt::Vertical, central);
  auto* collection = new QWidget(splitter);
  auto* collectionLayout = new QVBoxLayout(collection);
  collectionLayout->setContentsMargins(0, 0, 0, 0);
  state_->setObjectName(QStringLiteral("collectionState"));
  state_->setWordWrap(true);
  state_->setTextFormat(Qt::PlainText);
  collectionLayout->addWidget(state_);
  table_->setObjectName(QStringLiteral("projectsTable"));
  table_->setAccessibleName(QCoreApplication::translate("App", "Holzwerkprojekte"));
  table_->setColumnCount(5);
  table_->setHorizontalHeaderLabels({QCoreApplication::translate("App", "Projektname"),
                                     QCoreApplication::translate("App", "Holz / Material"),
                                     QCoreApplication::translate("App", "Aufbewahrungsort"),
                                     QCoreApplication::translate("App", "Status"),
                                     QCoreApplication::translate("App", "Foto")});
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->setAlternatingRowColors(true);
  table_->setShowGrid(false);
  table_->verticalHeader()->hide();
  table_->verticalHeader()->setDefaultSectionSize(table_->fontMetrics().height() + 20);
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  table_->horizontalHeader()->setMinimumSectionSize(65);
  table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
  table_->installEventFilter(this);
  collectionLayout->addWidget(table_, 1);
  auto* detail = new QWidget(splitter);
  auto* detailLayout = new QVBoxLayout(detail);
  detailLayout->setContentsMargins(0, 0, 0, 0);
  detailTitle_->setTextFormat(Qt::PlainText);
  detailTitle_->setWordWrap(true);
  QFont detailFont = detailTitle_->font();
  detailFont.setBold(true);
  detailTitle_->setFont(detailFont);
  detailLayout->addWidget(detailTitle_);
  note_->setObjectName(QStringLiteral("selectedNote"));
  note_->setAccessibleName(QCoreApplication::translate("App", "Vollständige Notiz"));
  note_->setReadOnly(true);
  note_->setTabChangesFocus(true);
  photo_->setObjectName(QStringLiteral("selectedPhoto"));
  auto* detailContent = new QSplitter(Qt::Horizontal, detail);
  detailContent->addWidget(photo_);
  auto* textTabs = new QTabWidget(detail);
  checklist_->setObjectName(QStringLiteral("selectedChecklist"));
  checklist_->setAccessibleName(QCoreApplication::translate("App", "Gespeicherte Arbeitsschritte"));
  checklist_->setReadOnly(true);
  checklist_->setTabChangesFocus(true);
  textTabs->addTab(checklist_, QCoreApplication::translate("App", "Arbeitsschritte"));
  textTabs->addTab(note_, QCoreApplication::translate("App", "Notiz"));
  detailContent->addWidget(textTabs);
  detailContent->setStretchFactor(0, 1);
  detailContent->setStretchFactor(1, 2);
  detailContent->setChildrenCollapsible(false);
  detailLayout->addWidget(detailContent);
  splitter->addWidget(collection);
  splitter->addWidget(detail);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);
  splitter->setChildrenCollapsible(false);
  root->addWidget(splitter, 1);
  feedback_->setObjectName(QStringLiteral("mainFeedback"));
  feedback_->setTextFormat(Qt::PlainText);
  feedback_->setWordWrap(true);
  feedback_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  root->addWidget(feedback_);

  auto* actions = new QHBoxLayout;
  add_->setObjectName(QStringLiteral("addProject"));
  edit_->setObjectName(QStringLiteral("editProject"));
  remove_->setObjectName(QStringLiteral("removeProject"));
  reload_->setObjectName(QStringLiteral("reloadProjects"));
  actions->addWidget(add_);
  actions->addWidget(edit_);
  actions->addWidget(remove_);
  actions->addStretch();
  root->addLayout(actions);
  root->addWidget(reload_, 0, Qt::AlignRight);
  reload_->hide();
  auto* centralScroll = new QScrollArea(this);
  centralScroll->setWidgetResizable(true);
  centralScroll->setFrameShape(QFrame::NoFrame);
  centralScroll->setWidget(central);
  setCentralWidget(centralScroll);

  connect(about, &QPushButton::clicked, this, &MainWindow::showAbout);
  connect(language_, &QPushButton::clicked, this, &MainWindow::showLanguage);
  auto* languageShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+,")), this);
  connect(languageShortcut, &QShortcut::activated, this, &MainWindow::showLanguage);
  connect(&controller_, &StorageController::languageSaved, this, [this](const QString& preference) {
    languagePreference_ = preference;
    feedback_->setText(
        QCoreApplication::translate("App", "Sprache gespeichert. Sie gilt beim nächsten Start."));
  });
  connect(clear, &QPushButton::clicked, search_, &QLineEdit::clear);
  connect(search_, &QLineEdit::textChanged, this, [this, clear] {
    clear->setEnabled(!search_->text().isEmpty());
    const auto selected = selectedProject();
    refresh(selected ? selected->id : 0);
  });
  clear->setEnabled(false);
  connect(table_, &QTableWidget::itemSelectionChanged, this, &MainWindow::updateSelection);
  connect(table_, &QTableWidget::cellDoubleClicked, this, [this] { openEditor(true); });
  connect(add_, &QPushButton::clicked, this, [this] { openEditor(false); });
  connect(edit_, &QPushButton::clicked, this, [this] { openEditor(true); });
  connect(remove_, &QPushButton::clicked, this, &MainWindow::deleteSelected);
  connect(reload_, &QPushButton::clicked, &controller_, &StorageController::load);
  auto* newShortcut = new QShortcut(QKeySequence::New, this);
  connect(newShortcut, &QShortcut::activated, this, [this] { openEditor(false); });
  auto* findShortcut = new QShortcut(QKeySequence::Find, this);
  connect(findShortcut, &QShortcut::activated, this, [this] {
    search_->setFocus();
    search_->selectAll();
  });
  auto* quitShortcut = new QShortcut(QKeySequence::Quit, this);
  connect(quitShortcut, &QShortcut::activated, this, &QWidget::close);
  connect(&controller_, &StorageController::busyChanged, this, [this](bool busy) {
    updateActions();
    if (busy) {
      feedback_->setText(QCoreApplication::translate("App", "Daten werden verarbeitet …"));
    }
  });
  connect(&controller_, &StorageController::loaded, this, [this](const ProjectList& projects) {
    const auto selected = selectedProject();
    projects_ = projects;
    ready_ = true;
    feedback_->clear();
    reload_->hide();
    refresh(selected ? selected->id : 0);
  });
  connect(&controller_, &StorageController::saved, this, [this](const WoodProject& project) {
    auto found = std::find_if(projects_.begin(), projects_.end(),
                              [&project](const WoodProject& row) { return row.id == project.id; });
    if (found == projects_.end()) {
      projects_.append(project);
    } else {
      *found = project;
    }
    refresh(project.id);
    const bool visible =
        std::any_of(visibleProjects_.cbegin(), visibleProjects_.cend(),
                    [&project](const WoodProject& row) { return row.id == project.id; });
    feedback_->setText(
        visible
            ? QCoreApplication::translate("App", "„%1“ wurde gespeichert.").arg(project.fields.name)
            : QCoreApplication::translate(
                  "App", "„%1“ wurde gespeichert, passt aber nicht zur aktuellen Suche. "
                         "Mit „Suche leeren“ findest du es wieder.")
                  .arg(project.fields.name));
    reload_->hide();
  });
  connect(&controller_, &StorageController::removed, this, [this](qint64 id) {
    const int row = table_->currentRow();
    projects_.erase(std::remove_if(projects_.begin(), projects_.end(),
                                   [id](const WoodProject& project) { return project.id == id; }),
                    projects_.end());
    refresh(0, row);
    feedback_->setText(QCoreApplication::translate("App", "Das Projekt wurde gelöscht."));
    reload_->hide();
  });
  connect(&controller_, &StorageController::failed, this,
          [this](Operation operation, const QString& message) {
            feedback_->setText(message);
            if (operation != Operation::Save && operation != Operation::Language) {
              reload_->show();
            }
            if (!ready_) {
              state_->setText(
                  QCoreApplication::translate("App", "Die Sammlung konnte nicht geöffnet werden."));
              count_->clear();
            }
            updateActions();
          });
  connect(&controller_, &StorageController::readReady, this,
          [this](quint64 token, qint64 id, const ProjectDraft& draft, const QImage& preview) {
            const auto selected = selectedProject();
            if (token != previewGeneration_ || !selected || selected->id != id)
              return;
            selectedDetails_ = draft;
            selectedPreview_ = preview;
            if (draft.photo.isEmpty())
              photo_->showMessage(QCoreApplication::translate("App", "Kein Foto"));
            else
              photo_->showPhoto(preview);
            updateActions();
          });
  connect(&controller_, &StorageController::readFailed, this,
          [this](quint64 token, const QString& message) {
            if (token != previewGeneration_)
              return;
            selectedDetails_.reset();
            photo_->showMessage(
                QCoreApplication::translate("App", "Foto konnte nicht geladen werden"));
            feedback_->setText(message);
            reload_->show();
            updateActions();
          });
  refresh();
  QTimer::singleShot(0, &controller_, &StorageController::load);
}

std::optional<WoodProject> MainWindow::selectedProject() const {
  const int row = table_->currentRow();
  if (row < 0 || row >= visibleProjects_.size()) {
    return std::nullopt;
  }
  return visibleProjects_.at(row);
}

void MainWindow::refresh(std::optional<qint64> preferredId, int fallbackRow) {
  const QSignalBlocker block(table_);
  visibleProjects_ = searchProjects(projects_, search_->text());
  table_->setRowCount(static_cast<int>(visibleProjects_.size()));
  int selection = visibleProjects_.isEmpty()
                      ? -1
                      : std::clamp(fallbackRow, 0, static_cast<int>(visibleProjects_.size()) - 1);
  for (int row = 0; row < visibleProjects_.size(); ++row) {
    const auto& project = visibleProjects_.at(row);
    const QStringList values{project.fields.name, project.fields.material, project.fields.location,
                             displayStatus(project.fields.status),
                             project.hasPhoto ? QCoreApplication::translate("App", "Ja")
                                              : QStringLiteral("–")};
    for (int column = 0; column < values.size(); ++column) {
      auto* item = new QTableWidgetItem(values.at(column));
      item->setData(Qt::UserRole, project.id);
      item->setToolTip(values.at(column));
      table_->setItem(row, column, item);
    }
    if (preferredId && project.id == *preferredId) {
      selection = row;
    }
  }
  table_->clearSelection();
  if (selection >= 0) {
    table_->setCurrentCell(selection, 0);
    table_->selectRow(selection);
  } else {
    table_->setCurrentCell(-1, -1);
  }
  state_->setText(
      !ready_ ? QCoreApplication::translate("App", "Sammlung wird geladen …")
      : projects_.isEmpty()
          ? QCoreApplication::translate(
                "App", "Deine Projektliste ist noch leer. Füge dein erstes Holzwerkprojekt hinzu.")
      : visibleProjects_.isEmpty()
          ? QCoreApplication::translate("App",
                                        "Keine Projekte gefunden. Ändere oder leere die Suche.")
          : QString());
  count_->setText(ready_ ? QCoreApplication::translate("App", "Projekte: %1 / %2")
                               .arg(QLocale().toString(visibleProjects_.size()),
                                    QLocale().toString(projects_.size()))
                         : QString());
  updateSelection();
}

void MainWindow::updateSelection() {
  const auto selected = selectedProject();
  detailTitle_->setText(selected
                            ? QCoreApplication::translate("App", "„%1“").arg(selected->fields.name)
                            : QCoreApplication::translate("App", "Projekt auswählen"));
  note_->setPlainText(selected ? selected->fields.note : QString());
  QStringList lines;
  if (selected) {
    for (const auto& step : selected->fields.steps) {
      lines.append(QStringLiteral("%1  %2").arg(
          step.completed ? QStringLiteral("☑") : QStringLiteral("☐"), step.text));
    }
  }
  checklist_->setPlainText(lines.join(QChar('\n')));
  checklist_->setPlaceholderText(
      selected ? QCoreApplication::translate(
                     "App", "Noch keine Arbeitsschritte. Ergänze sie unter Bearbeiten.")
               : QCoreApplication::translate(
                     "App", "Wähle ein Projekt, um seine Arbeitsschritte zu sehen."));
  ++previewGeneration_;
  selectedDetails_.reset();
  selectedPreview_ = {};
  if (selected) {
    photo_->showMessage(QCoreApplication::translate("App", "Projekt wird geladen …"));
    controller_.read(selected->id, previewGeneration_);
  } else {
    photo_->showMessage(QCoreApplication::translate("App", "Kein Foto"));
  }
  note_->setPlaceholderText(
      selected
          ? QCoreApplication::translate("App", "Zu diesem Projekt gibt es noch keine Notiz.")
          : QCoreApplication::translate("App", "Wähle ein Projekt aus, um seine Notiz zu lesen."));
  updateActions();
}

void MainWindow::updateActions() {
  const bool available = ready_ && !controller_.isBusy() && (!editor_ || !editor_->isVisible());
  add_->setEnabled(available);
  language_->setEnabled(available);
  edit_->setEnabled(available && selectedProject().has_value() && selectedDetails_.has_value());
  remove_->setEnabled(available && selectedProject().has_value());
  reload_->setEnabled(!controller_.isBusy() && (!editor_ || !editor_->isVisible()));
}

void MainWindow::openEditor(bool editing) {
  if (!ready_ || controller_.isBusy() || (editor_ && editor_->isVisible())) {
    return;
  }
  auto selected = editing ? selectedProject() : std::nullopt;
  if (editing && (!selected || !selectedDetails_)) {
    return;
  }
  if (selected)
    selected->fields = *selectedDetails_;
  editor_ = new ProjectEditor(controller_, selected, this, editing ? selectedPreview_ : QImage());
  editor_->setAttribute(Qt::WA_DeleteOnClose);
  connect(editor_, &QDialog::finished, this, [this] {
    editor_ = nullptr;
    updateActions();
  });
  editor_->open();
  updateActions();
}

void MainWindow::deleteSelected() {
  const auto selected = selectedProject();
  if (!selected || !remove_->isEnabled()) {
    return;
  }
  if (confirmDelete(this, selected->fields.name)) {
    controller_.remove(selected->id);
  }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if (watched == table_ && event->type() == QEvent::KeyPress) {
    const auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
      openEditor(true);
      return true;
    }
    if (key->key() == Qt::Key_Delete ||
        (key->key() == Qt::Key_Backspace && key->modifiers().testFlag(Qt::ControlModifier))) {
      deleteSelected();
      return true;
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (controller_.isBusy()) {
    feedback_->setText(QCoreApplication::translate(
        "App",
        "Bitte warte, bis der laufende Datenbankzugriff beendet ist, und schließe dann erneut."));
    event->ignore();
    return;
  }
  if (editor_ && editor_->isVisible() && !editor_->requestClose()) {
    event->ignore();
    return;
  }
  event->accept();
}

void MainWindow::showAbout() {
  QDialog dialog(this);
  dialog.setWindowTitle(QCoreApplication::translate("App", "Über HolzwerkProjekt"));
  dialog.resize(640, 500);
  auto* layout = new QVBoxLayout(&dialog);
  auto* text = new QTextBrowser(&dialog);
  QFile notices(QStringLiteral(":/licenses/THIRD_PARTY_NOTICES.md"));
  QString licenseText;
  if (notices.open(QIODevice::ReadOnly)) {
    licenseText = QString::fromUtf8(notices.readAll());
  }
  text->setPlainText(
      QCoreApplication::translate(
          "App", "HolzwerkProjekt %1\n\nDeine Sammlung bleibt lokal.\nDatenverzeichnis: %2\n\n%3")
          .arg(QCoreApplication::applicationVersion(), dataDirectory_, licenseText));
  layout->addWidget(text);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
  buttons->button(QDialogButtonBox::Close)
      ->setText(QCoreApplication::translate("App", "Schließen"));
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);
  dialog.exec();
}

void MainWindow::showLanguage() {
  if (!language_->isEnabled())
    return;
  LanguageDialog dialog(controller_, dataDirectory_, languagePreference_, this);
  dialog.exec();
}

} // namespace woodwork
