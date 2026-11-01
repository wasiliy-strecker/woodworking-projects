#pragma once
#include <QDialog>

namespace woodwork {
enum class InformationPage : quint8 { About, Privacy, Licenses };
class InformationDialog final : public QDialog {
public:
  InformationDialog(InformationPage page, const QString& dataDirectory, QWidget* parent);
};
} // namespace woodwork
