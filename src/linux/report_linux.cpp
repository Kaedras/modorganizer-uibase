#include "report.h"
#include "log.h"
#include <QApplication>
#include <QMessageBox>
#include <QString>
#include <QWidget>

namespace MOBase
{

QWidget* topLevelWindow();

void reportError(const QString& message)
{
  log::error("{}", message);

  if (QApplication::topLevelWidgets().count() != 0) {
    if (auto* mw = topLevelWindow()) {
      QMessageBox::warning(mw, QObject::tr("Error"), message, QMessageBox::Ok);
    } else {
      criticalOnTop(message);
    }
  }
}

}  // namespace MOBase