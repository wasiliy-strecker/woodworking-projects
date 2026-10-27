#pragma once

#include "application/project_service.h"

#include <QImage>
#include <QObject>
#include <QThread>
#include <functional>
#include <memory>

namespace woodwork {

enum class Operation : quint8 { Load, Save, Remove, Import };
using RepositoryFactory = std::function<std::unique_ptr<ProjectRepository>()>;

class StorageWorker final : public QObject {
  Q_OBJECT
public:
  explicit StorageWorker(RepositoryFactory factory);

public slots:
  void load();
  void save(qint64 id, const woodwork::ProjectDraft& draft);
  void remove(qint64 id);
  void read(qint64 id, quint64 token);
  void importPhotoFile(const QString& path);

signals:
  void readReady(quint64 token, qint64 id, const woodwork::ProjectDraft& draft,
                 const QImage& preview);
  void readFailed(quint64 token, const QString& message);
  void imported(const QByteArray& bytes, const QImage& preview);
  void loaded(const woodwork::ProjectList& projects);
  void saved(const woodwork::WoodProject& project);
  void removed(qint64 id);
  void failed(woodwork::Operation operation, const QString& message);

private:
  RepositoryFactory factory_;
  std::unique_ptr<ProjectRepository> repository_;
};

class StorageController final : public QObject {
  Q_OBJECT
public:
  explicit StorageController(RepositoryFactory factory, QObject* parent = nullptr);
  ~StorageController() override;
  bool isBusy() const { return busy_; }
  bool load();
  bool save(qint64 id, const ProjectDraft& draft);
  bool remove(qint64 id);
  void read(qint64 id, quint64 token);
  bool importPhotoFile(const QString& path);

signals:
  void readReady(quint64 token, qint64 id, const woodwork::ProjectDraft& draft,
                 const QImage& preview);
  void readFailed(quint64 token, const QString& message);
  void imported(const QByteArray& bytes, const QImage& preview);
  void loaded(const woodwork::ProjectList& projects);
  void saved(const woodwork::WoodProject& project);
  void removed(qint64 id);
  void failed(woodwork::Operation operation, const QString& message);
  void busyChanged(bool busy);
  void readRequested(qint64 id, quint64 token);
  void importRequested(const QString& path);
  void loadRequested();
  void saveRequested(qint64 id, const woodwork::ProjectDraft& draft);
  void removeRequested(qint64 id);

private:
  bool begin();
  void finish();
  QThread thread_;
  bool busy_ = false;
};

} // namespace woodwork
Q_DECLARE_METATYPE(woodwork::Operation)
