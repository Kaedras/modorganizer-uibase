#include "log.h"
#include "report.h"
#include <QApplication>
#include <QMessageBox>
#include <QString>
#include <QWidget>
#include <Windows.h>

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
  } else {
    ::MessageBoxW(0, message.toStdWString().c_str(),
                  QObject::tr("Error").toStdWString().c_str(), MB_ICONERROR | MB_OK);
  }
}

}  // namespace MOBase