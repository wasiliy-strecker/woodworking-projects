#pragma once

#include "ui/project_editor.h"

#include <QMainWindow>
#include <QPointer>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

namespace woodwork {

class MainWindow final : public QMainWindow {
  Q_OBJECT
public:
  MainWindow(StorageController& controller, QString dataDirectory,
             QString languagePreference = QStringLiteral("auto"), QWidget* parent = nullptr);

protected:
  void closeEvent(QCloseEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  void refresh(std::optional<qint64> preferredId = std::nullopt, int fallbackRow = 0);
  void updateSelection();
  void updateActions();
  std::optional<WoodProject> selectedProject() const;
  void openEditor(bool editing);
  void deleteSelected();
  bool beginTransfer();
  void finishTransfer(const QString& message);
  void exportCollection();
  void importCollection();
  void showAbout();
  void showLanguage();
  StorageController& controller_;
  QString dataDirectory_;
  QString languagePreference_;
  ProjectList projects_;
  ProjectList visibleProjects_;
  QPointer<ProjectEditor> editor_;
  QLineEdit* search_;
  QTableWidget* table_;
  QPlainTextEdit* note_;
  QPlainTextEdit* checklist_;
  PhotoView* photo_;
  std::optional<ProjectDraft> selectedDetails_;
  QImage selectedPreview_;
  quint64 previewGeneration_ = 0;
  QLabel* detailTitle_;
  QLabel* state_;
  QLabel* feedback_;
  QLabel* count_;
  QPushButton* add_;
  QPushButton* edit_;
  QPushButton* remove_;
  QPushButton* reload_;
  QPushButton* language_;
  QPushButton* exportCollection_;
  QPushButton* importCollection_;
  QString transferDirectory_;
  bool transferRunning_ = false;
  bool ready_ = false;
};

} // namespace woodwork
