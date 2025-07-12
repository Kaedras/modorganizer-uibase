#include "log.h"
#include "report.h"
#include <QApplication>
#include <QMessageBox>
#include <QString>
#include <QWidget>

namespace MOBase
{

extern QWidget* topLevelWindow();

void reportError(const QString& message)
{
  log::error("{}", message);

  if (QApplication::topLevelWidgets().count() != 0) {
    if (auto* mw = topLevelWindow()) {
      QMessageBox::warning(mw, QObject::tr("Error"), message, QMessageBox::Ok);
    } else {
      criticalOnTop(message);
    }
  } else {
    QMessageBox::warning(nullptr, QObject::tr("Error"), message, QMessageBox::Ok);
  }
}

}  // namespace MOBase