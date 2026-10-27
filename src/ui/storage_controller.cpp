#include "ui/storage_controller.h"
#include "application/photo_importer.h"

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
      throw StorageError(QStringLiteral("Die Sammlung ist noch nicht geladen."));
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
      throw StorageError(QStringLiteral("Die Sammlung ist noch nicht geladen."));
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
      throw StorageError(QStringLiteral("Die Projekte sind noch nicht geladen."));
    const auto draft = repository_->find(id);
    const auto preview = draft.photo.isEmpty() ? QImage() : decodePhoto(draft.photo).preview;
    emit readReady(token, id, draft, preview);
  } catch (const std::exception& error) {
    emit readFailed(
        token,
        QStringLiteral("Das Projekt konnte nicht vollständig geladen werden. "
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

StorageController::StorageController(RepositoryFactory factory, QObject* parent) : QObject(parent) {
  qRegisterMetaType<ProjectDraft>();
  qRegisterMetaType<WoodProject>();
  qRegisterMetaType<ProjectList>();
  qRegisterMetaType<Operation>();
  auto* worker = new StorageWorker(std::move(factory));
  worker->moveToThread(&thread_);
  connect(&thread_, &QThread::finished, worker, &QObject::deleteLater);
  connect(this, &StorageController::loadRequested, worker, &StorageWorker::load);
  connect(this, &StorageController::saveRequested, worker, &StorageWorker::save);
  connect(this, &StorageController::removeRequested, worker, &StorageWorker::remove);
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

} // namespace woodwork
