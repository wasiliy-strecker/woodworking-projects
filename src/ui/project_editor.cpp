#include "ui/project_editor.h"
#include "ui/checklist_editor.h"
#include "ui/photo_view.h"
#include "ui/ui_support.h"
#include <QComboBox>
#include <QFileDialog>

#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>

namespace woodwork {

ProjectEditor::ProjectEditor(StorageController& controller, std::optional<WoodProject> existing,
                             QWidget* parent, const QImage& preview)
    : QDialog(parent), controller_(controller), name_(new QLineEdit(this)),
      material_(new QLineEdit(this)), location_(new QLineEdit(this)),
      note_(new QPlainTextEdit(this)), status_(new QComboBox(this)), photo_(new PhotoView(this)),
      checklist_(new ChecklistEditor(existing ? existing->fields.steps : Checklist{}, this)),
      choosePhoto_(new QPushButton(QStringLiteral("&Foto hinzufügen …"), this)),
      removePhoto_(new QPushButton(QStringLiteral("Foto &entfernen"), this)),
      scroll_(new QScrollArea(this)), error_(new QLabel(this)),
      save_(new QPushButton(QStringLiteral("Speichern"), this)),
      cancel_(new QPushButton(QStringLiteral("Abbrechen"), this)) {
  setObjectName(QStringLiteral("projectEditor"));
  setWindowTitle(existing ? QStringLiteral("Projekt bearbeiten")
                          : QStringLiteral("Projekt hinzufügen"));
  setWindowModality(Qt::WindowModal);
  resize(600, 780);
  setMinimumSize(320, 280);
  status_->setObjectName(QStringLiteral("projectStatus"));
  status_->setEditable(false);
  for (const auto value :
       {ProjectStatus::Planned, ProjectStatus::Started, ProjectStatus::Finished}) {
    status_->addItem(displayStatus(value), QVariant::fromValue(value));
  }
  photo_->setObjectName(QStringLiteral("editorPhoto"));
  photo_->setMinimumHeight(150);
  choosePhoto_->setObjectName(QStringLiteral("choosePhoto"));
  removePhoto_->setObjectName(QStringLiteral("removePhoto"));
  choosePhoto_->setAutoDefault(false);
  removePhoto_->setAutoDefault(false);
  connect(choosePhoto_, &QPushButton::clicked, this, &ProjectEditor::selectPhoto);
  connect(removePhoto_, &QPushButton::clicked, this, [this] {
    if (saving_ || importing_ || choosing_)
      return;
    photoBytes_.clear();
    photo_->showMessage(QStringLiteral("Kein Foto"));
    updatePhotoActions();
  });
  name_->setObjectName(QStringLiteral("projectName"));
  material_->setObjectName(QStringLiteral("projectMaterial"));
  location_->setObjectName(QStringLiteral("projectLocation"));
  note_->setObjectName(QStringLiteral("projectNote"));
  save_->setObjectName(QStringLiteral("saveProject"));
  cancel_->setObjectName(QStringLiteral("cancelEdit"));
  error_->setObjectName(QStringLiteral("editorError"));
  for (auto* field : {name_, material_, location_}) {
    field->setMaxLength(1000000);
    field->setClearButtonEnabled(true);
  }
  name_->setPlaceholderText(QStringLiteral("Wie heißt das Holzwerkprojekt?"));
  material_->setPlaceholderText(QStringLiteral("Optional"));
  location_->setPlaceholderText(QStringLiteral("Zum Beispiel Wohnzimmer, Regal 2"));
  note_->setPlaceholderText(QStringLiteral("Was möchtest du dir zu diesem Projekt merken?"));
  note_->setMinimumHeight(120);
  note_->setTabChangesFocus(true);
  error_->setTextFormat(Qt::PlainText);
  error_->setWordWrap(true);
  error_->setAccessibleName(QStringLiteral("Hinweis zum Speichern"));
  error_->hide();
  connect(name_, &QLineEdit::textChanged, this, [this] {
    if (validationError_ && !name_->text().trimmed().isEmpty()) {
      validationError_ = false;
      error_->hide();
    }
  });
  if (existing) {
    (void)storedStatus(existing->fields.status);
    id_ = existing->id;
    name_->setText(existing->fields.name);
    material_->setText(existing->fields.material);
    location_->setText(existing->fields.location);
    note_->setPlainText(existing->fields.note);
    status_->setCurrentIndex(status_->findData(QVariant::fromValue(existing->fields.status)));
    photoBytes_ = existing->fields.photo;
    if (existing->hasPhoto && (photoBytes_.isEmpty() || preview.isNull())) {
      throw StorageError(QStringLiteral(
          "Das gespeicherte Foto muss vor der Bearbeitung vollständig geladen werden."));
    }
    if (!photoBytes_.isEmpty())
      photo_->showPhoto(preview);
  }
  original_ = draft();

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(20, 20, 20, 20);
  root->setSpacing(14);
  auto* scroll = scroll_;
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  auto* formWidget = new QWidget(scroll);
  auto* form = new QFormLayout(formWidget);
  form->setRowWrapPolicy(QFormLayout::WrapAllRows);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  form->setSpacing(10);
  auto* hint = new QLabel(QStringLiteral("Nur der Projektname ist erforderlich."), formWidget);
  hint->setWordWrap(true);
  form->addRow(hint);
  form->addRow(error_);
  form->addRow(QStringLiteral("&Projektname *"), name_);
  form->addRow(QStringLiteral("Holz oder &Material"), material_);
  form->addRow(QStringLiteral("&Aufbewahrungsort"), location_);
  form->addRow(QStringLiteral("&Status"), status_);
  form->addRow(QStringLiteral("&Notiz"), note_);
  form->addRow(QStringLiteral("&Checkliste"), checklist_);
  form->addRow(QStringLiteral("Foto"), photo_);
  form->addRow(choosePhoto_);
  form->addRow(removePhoto_);
  auto* photoHint = new QLabel(
      QStringLiteral(
          "JPEG oder PNG. Bis 10 MiB, 6000 Pixel je Seite und 16 Millionen Pixel insgesamt."),
      formWidget);
  photoHint->setWordWrap(true);
  form->addRow(photoHint);

  scroll->setWidget(formWidget);
  connect(checklist_, &ChecklistEditor::rowAdded, this, [scroll](QWidget* field) {
    QTimer::singleShot(0, field, [scroll, field] { scroll->ensureWidgetVisible(field); });
  });
  root->addWidget(scroll, 1);
  auto* buttons = new QDialogButtonBox(this);
  buttons->addButton(cancel_, QDialogButtonBox::RejectRole);
  buttons->addButton(save_, QDialogButtonBox::AcceptRole);
  root->addWidget(buttons);
  save_->setDefault(true);
  connect(save_, &QPushButton::clicked, this, &ProjectEditor::save);
  connect(cancel_, &QPushButton::clicked, this, &ProjectEditor::reject);
  auto* saveShortcut = new QShortcut(QKeySequence::Save, this);
  connect(saveShortcut, &QShortcut::activated, this, &ProjectEditor::save);
  connect(&controller_, &StorageController::saved, this, [this](const WoodProject&) {
    if (saving_) {
      saving_ = false;
      accept();
    }
  });
  connect(&controller_, &StorageController::failed, this,
          [this, scroll](Operation operation, const QString& message) {
            if ((operation == Operation::Save && saving_) ||
                (operation == Operation::Import && importing_)) {
              importing_ = false;
              setSaving(false);
              error_->setText(message);
              error_->show();
              scroll->ensureWidgetVisible(error_);
            }
          });
  connect(&controller_, &StorageController::imported, this,
          [this](const QByteArray& bytes, const QImage& image) {
            if (!importing_)
              return;
            photoBytes_ = bytes;
            photo_->showPhoto(image);
            importing_ = false;
            updateBusy();
          });
  updatePhotoActions();
  name_->setFocus();
}

ProjectDraft ProjectEditor::draft() const {
  if (status_->currentIndex() < 0)
    throw ValidationError(QStringLiteral("Bitte wähle einen gültigen Projektstatus."));
  return {name_->text(),
          material_->text(),
          location_->text(),
          note_->toPlainText(),
          status_->currentData().value<ProjectStatus>(),
          photoBytes_,
          checklist_->steps()};
}

void ProjectEditor::setSaving(bool saving) {
  saving_ = saving;
  updateBusy();
}

void ProjectEditor::updateBusy() {
  const bool saving = saving_ || importing_ || choosing_;
  name_->setEnabled(!saving);
  material_->setEnabled(!saving);
  location_->setEnabled(!saving);
  note_->setEnabled(!saving);
  status_->setEnabled(!saving);
  checklist_->setEnabled(!saving);
  choosePhoto_->setEnabled(!saving);
  removePhoto_->setEnabled(!saving && !photoBytes_.isEmpty());
  save_->setEnabled(!saving);
  cancel_->setEnabled(!saving);
  updatePhotoActions();
  save_->setText(saving_ ? QStringLiteral("Wird gespeichert …") : QStringLiteral("Speichern"));
}

void ProjectEditor::save() {
  if (saving_ || importing_ || choosing_ || controller_.isBusy()) {
    return;
  }
  try {
    const auto validated = validateDraft(draft());
    validationError_ = false;
    error_->hide();
    setSaving(true);
    if (!controller_.save(id_, validated)) {
      setSaving(false);
    }
  } catch (const ValidationError& error) {
    validationError_ = true;
    error_->setText(QString::fromUtf8(error.what()));
    error_->show();
    scroll_->ensureWidgetVisible(error_);
  }
}

void ProjectEditor::updatePhotoActions() {
  choosePhoto_->setText(photoBytes_.isEmpty() ? QStringLiteral("&Foto hinzufügen …")
                                              : QStringLiteral("&Foto ersetzen …"));
  removePhoto_->setEnabled(!photoBytes_.isEmpty() && !importing_ && !saving_ && !choosing_);
}
void ProjectEditor::selectPhoto() {
  if (saving_ || importing_ || choosing_ || controller_.isBusy())
    return;
  choosing_ = true;
  updateBusy();
  // No DontUseNativeDialog option. The platform supplies its native picker.
  const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("Projektfoto auswählen"), {},
                                                 QStringLiteral("Fotos (*.jpg *.jpeg *.png)"));
  choosing_ = false;
  updateBusy();
  importFromPath(path);
}
void ProjectEditor::importFromPath(const QString& path) {
  if (path.isEmpty() || saving_ || importing_ || choosing_ || controller_.isBusy())
    return;
  importing_ = true;
  error_->hide();
  updateBusy();
  if (!controller_.importPhotoFile(path)) {
    importing_ = false;
    updateBusy();
  }
}

bool ProjectEditor::requestClose() {
  if (saving_ || importing_ || choosing_) {
    return false;
  }
  if (draft() != original_ && !confirmDiscard(this)) {
    return false;
  }
  QDialog::reject();
  return true;
}

void ProjectEditor::reject() { requestClose(); }

void ProjectEditor::closeEvent(QCloseEvent* event) {
  if (requestClose()) {
    event->accept();
  } else {
    event->ignore();
  }
}

} // namespace woodwork
