#include "ui/storage_controller.h"
#include "application/collection_transfer.h"
#include "application/photo_importer.h"
#include "persistence/json_collection_file.h"
#include "persistence/language_preferences.h"
#include <QCoreApplication>

#include <utility>

namespace woodwork {

StorageWorker::StorageWorker(RepositoryFactory factory) : factory_(std::move(factory)) {}

void StorageWorker::load() {
  try {
    if (!repository_) {
      repository_ = factory_();
    }
    ProjectService service(*repository_);
    emit loaded(service.load());
  } catch (const std::exception& error) {
    emit failed(Operation::Load, QString::fromUtf8(error.what()));
  }
}

void StorageWorker::save(qint64 id, const ProjectDraft& draft) {
  try {
    if (!repository_) {
      throw StorageError(
          QCoreApplication::translate("App", "Die Sammlung ist noch nicht geladen."));
    }
    ProjectService service(*repository_);
    emit saved(service.save(id, draft));
  } catch (const std::exception& error) {
    emit failed(Operation::Save, QString::fromUtf8(error.what()));
  }
}

void StorageWorker::remove(qint64 id) {
  try {
    if (!repository_) {
      throw StorageError(
          QCoreApplication::translate("App", "Die Sammlung ist noch nicht geladen."));
    }
    ProjectService service(*repository_);
    service.remove(id);
    emit removed(id);
  } catch (const std::exception& error) {
    emit failed(Operation::Remove, QString::fromUtf8(error.what()));
  }
}

void StorageWorker::read(qint64 id, quint64 token) {
  try {
    if (!repository_)
      throw StorageError(
          QCoreApplication::translate("App", "Die Projekte sind noch nicht geladen."));
    const auto draft = repository_->find(id);
    const auto preview = draft.photo.isEmpty() ? QImage() : decodePhoto(draft.photo).preview;
    emit readReady(token, id, draft, preview);
  } catch (const std::exception& error) {
    emit readFailed(
        token, QCoreApplication::translate(
                   "App", "Das Projekt konnte nicht vollständig geladen werden. "
                          "Zum Schutz der gespeicherten Daten bleibt die Bearbeitung gesperrt.\n%1")
                   .arg(QString::fromUtf8(error.what())));
  }
}
void StorageWorker::importPhotoFile(const QString& path) {
  try {
    const auto photo = importPhoto(path);
    emit imported(photo.bytes, photo.preview);
  } catch (const std::exception& error) {
    emit failed(Operation::Import, QString::fromUtf8(error.what()));
  }
}

void StorageWorker::saveLanguage(const QString& directory, const QString& preference) {
  try {
    saveLanguagePreference(directory, preference);
    emit languageSaved(preference);
  } catch (const std::exception& error) {
    emit failed(Operation::Language, QString::fromUtf8(error.what()));
  }
}

// File path and app directory are named explicitly at the queued UI/worker boundary.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void StorageWorker::inspectCollectionExport(const QString& path, const QString& directory) {
  try {
    JsonCollectionFile files(directory);
    emit collectionExportInspected(files.inspect(path));
  } catch (const std::exception& error) {
    emit failed(Operation::InspectCollectionExport, QString::fromUtf8(error.what()));
  }
}

void StorageWorker::exportCollection(const CollectionExportTarget& target, bool replacementApproved,
                                     const QString& directory) {
  try {
    if (!repository_) {
      throw StorageError(
          QCoreApplication::translate("App", "Die Sammlung ist noch nicht geladen."));
    }
    JsonCollectionFile files(directory);
    CollectionTransfer transfer(*repository_, files);
    emit collectionExported(transfer.exportEntries(target, replacementApproved));
  } catch (const std::exception& error) {
    emit failed(Operation::ExportCollection, QString::fromUtf8(error.what()));
  }
}

void StorageWorker::readCollectionImport(const QString& path) {
  pendingImport_.reset();
  try {
    JsonCollectionFile files;
    pendingImport_ = files.read(path);
    emit collectionImportReady(pendingImport_->size());
  } catch (const std::exception& error) {
    emit failed(Operation::ReadCollectionImport, QString::fromUtf8(error.what()));
  }
}

void StorageWorker::importCollection() {
  try {
    if (!repository_ || !pendingImport_) {
      throw TransferError(
          QCoreApplication::translate("App", "Wähle zuerst eine gültige Importdatei."));
    }
    // Import the exact validated snapshot the user confirmed, never a later file revision.
    const auto result = repository_->importEntries(*pendingImport_);
    pendingImport_.reset();
    emit collectionImported(result);
  } catch (const std::exception& error) {
    pendingImport_.reset();
    emit failed(Operation::ImportCollection, QString::fromUtf8(error.what()));
  }
}

void StorageWorker::discardCollectionImport() { pendingImport_.reset(); }

StorageController::StorageController(RepositoryFactory factory, QObject* parent) : QObject(parent) {
  qRegisterMetaType<ProjectDraft>();
  qRegisterMetaType<WoodProject>();
  qRegisterMetaType<ProjectList>();
  qRegisterMetaType<Operation>();
  qRegisterMetaType<CollectionExportTarget>();
  qRegisterMetaType<CollectionImportResult>();
  auto* worker = new StorageWorker(std::move(factory));
  worker->moveToThread(&thread_);
  connect(&thread_, &QThread::finished, worker, &QObject::deleteLater);
  connect(this, &StorageController::loadRequested, worker, &StorageWorker::load);
  connect(this, &StorageController::saveRequested, worker, &StorageWorker::save);
  connect(this, &StorageController::removeRequested, worker, &StorageWorker::remove);
  connect(this, &StorageController::languageRequested, worker, &StorageWorker::saveLanguage);
  connect(worker, &StorageWorker::languageSaved, this, [this](const QString& preference) {
    finish();
    emit languageSaved(preference);
  });
  connect(worker, &StorageWorker::loaded, this, [this](const ProjectList& projects) {
    finish();
    emit loaded(projects);
  });
  connect(worker, &StorageWorker::saved, this, [this](const WoodProject& project) {
    finish();
    emit saved(project);
  });
  connect(worker, &StorageWorker::removed, this, [this](qint64 id) {
    finish();
    emit removed(id);
  });
  connect(worker, &StorageWorker::failed, this,
          [this](Operation operation, const QString& message) {
            finish();
            emit failed(operation, message);
          });
  connect(this, &StorageController::readRequested, worker, &StorageWorker::read);
  connect(worker, &StorageWorker::readReady, this, &StorageController::readReady);
  connect(worker, &StorageWorker::readFailed, this, &StorageController::readFailed);
  connect(this, &StorageController::importRequested, worker, &StorageWorker::importPhotoFile);
  connect(worker, &StorageWorker::imported, this,
          [this](const QByteArray& bytes, const QImage& preview) {
            finish();
            emit imported(bytes, preview);
          });
  connect(this, &StorageController::inspectCollectionExportRequested, worker,
          &StorageWorker::inspectCollectionExport);
  connect(this, &StorageController::collectionExportRequested, worker,
          &StorageWorker::exportCollection);
  connect(this, &StorageController::readCollectionImportRequested, worker,
          &StorageWorker::readCollectionImport);
  connect(this, &StorageController::collectionImportRequested, worker,
          &StorageWorker::importCollection);
  connect(this, &StorageController::discardCollectionImportRequested, worker,
          &StorageWorker::discardCollectionImport);
  connect(worker, &StorageWorker::collectionExportInspected, this,
          [this](const CollectionExportTarget& target) {
            finish();
            emit collectionExportInspected(target);
          });
  connect(worker, &StorageWorker::collectionExported, this, [this](qsizetype count) {
    finish();
    emit collectionExported(count);
  });
  connect(worker, &StorageWorker::collectionImportReady, this, [this](qsizetype count) {
    finish();
    emit collectionImportReady(count);
  });
  connect(worker, &StorageWorker::collectionImported, this,
          [this](const CollectionImportResult& result) {
            finish();
            emit collectionImported(result);
          });
  thread_.start();
}

StorageController::~StorageController() {
  // MainWindow prevents normal closure during outstanding work. This also makes
  // exceptional teardown safe. All connection ownership stays in the worker.
  thread_.quit();
  thread_.wait();
}

void StorageController::read(qint64 id, quint64 token) { emit readRequested(id, token); }
bool StorageController::importPhotoFile(const QString& path) {
  if (!begin())
    return false;
  emit importRequested(path);
  return true;
}

bool StorageController::begin() {
  if (busy_) {
    return false;
  }
  busy_ = true;
  emit busyChanged(true);
  return true;
}

void StorageController::finish() {
  busy_ = false;
  emit busyChanged(false);
}

bool StorageController::load() {
  if (!begin()) {
    return false;
  }
  emit loadRequested();
  return true;
}

bool StorageController::save(qint64 id, const ProjectDraft& draft) {
  if (!begin()) {
    return false;
  }
  emit saveRequested(id, draft);
  return true;
}

bool StorageController::remove(qint64 id) {
  if (!begin()) {
    return false;
  }
  emit removeRequested(id);
  return true;
}

bool StorageController::saveLanguage(const QString& directory, const QString& preference) {
  if (!begin()) {
    return false;
  }
  emit languageRequested(directory, preference);
  return true;
}

bool StorageController::inspectCollectionExport(const QString& path, const QString& directory) {
  if (!begin()) {
    return false;
  }
  emit inspectCollectionExportRequested(path, directory);
  return true;
}

bool StorageController::exportCollection(const CollectionExportTarget& target,
                                         bool replacementApproved, const QString& directory) {
  if (!begin()) {
    return false;
  }
  emit collectionExportRequested(target, replacementApproved, directory);
  return true;
}

bool StorageController::readCollectionImport(const QString& path) {
  if (!begin()) {
    return false;
  }
  emit readCollectionImportRequested(path);
  return true;
}

bool StorageController::importCollection() {
  if (!begin()) {
    return false;
  }
  emit collectionImportRequested();
  return true;
}

void StorageController::discardCollectionImport() { emit discardCollectionImportRequested(); }

} // namespace woodwork
