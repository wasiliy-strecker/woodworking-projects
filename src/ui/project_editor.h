#pragma once

#include "ui/color_field.h"
#include "ui/storage_controller.h"

#include <QDialog>
#include <optional>

class QCloseEvent;
class QComboBox;
class QScrollArea;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace woodwork {

class PhotoView;
class ChecklistEditor;
class ProjectEditor final : public QDialog {
  Q_OBJECT
public:
  ProjectEditor(StorageController& controller, std::optional<WoodProject> existing,
                QWidget* parent = nullptr, const QImage& preview = {});
  ProjectDraft draft() const;
  bool requestClose();
  void importFromPath(const QString& path);

public slots:
  void reject() override;
  void save();

protected:
  void closeEvent(QCloseEvent* event) override;

private:
  void setSaving(bool saving);
  void updateBusy();
  void selectPhoto();
  void updatePhotoActions();
  StorageController& controller_;
  qint64 id_ = 0;
  ProjectDraft original_;
  ColorField* color_;
  QLineEdit* name_;
  QLineEdit* material_;
  QLineEdit* location_;
  QPlainTextEdit* note_;
  QComboBox* status_;
  PhotoView* photo_;
  ChecklistEditor* checklist_;
  QPushButton* choosePhoto_;
  QPushButton* removePhoto_;
  QByteArray photoBytes_;
  QScrollArea* scroll_;
  bool importing_ = false;
  bool choosing_ = false;
  QLabel* error_;
  QPushButton* save_;
  QPushButton* cancel_;
  bool saving_ = false;
  bool validationError_ = false;
};

} // namespace woodwork
