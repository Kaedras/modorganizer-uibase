/*
This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "registry.h"
#include "log.h"
#include "report.h"
#include <QApplication>
#include <QList>
#include <QMessageBox>
#include <QString>

namespace MOBase
{

// helper function that mirrors the behaviour of WritePrivateProfileString
bool SetValue(const QString& appName, const QString& keyName, const QString& value,
              QSettings& ini)
{
  if (keyName.isEmpty()) {
    // remove section if key is empty
    ini.remove(appName);
  } else if (value.isEmpty()) {
    // remove key if value is empty
    ini.remove(keyName);
  } else {
    ini.setValue(appName % '/' % keyName, value);
  }

  ini.sync();
  return ini.status() == QSettings::NoError;
}

bool WriteRegistryValue(const wchar_t* appName, const wchar_t* keyName,
                        const wchar_t* value, const wchar_t* fileName)
{
  return WriteRegistryValue(
      QString::fromWCharArray(appName), QString::fromWCharArray(keyName),
      QString::fromWCharArray(value), QString::fromWCharArray(fileName));
}

bool WriteRegistryValue(const char* appName, const char* keyName, const char* value,
                        const char* fileName)
{
  return WriteRegistryValue(
      QString::fromLocal8Bit(appName), QString::fromLocal8Bit(keyName),
      QString::fromLocal8Bit(value), QString::fromLocal8Bit(fileName));
}

bool WriteRegistryValue(const QString& appName, const QString& keyName,
                        const QString& value, const QString& fileName)
{
  QSettings settings(fileName, QSettings::Format::IniFormat);
  bool success = true;

  if (!SetValue(appName, keyName, value, settings)) {
    success = false;
    if (settings.status() == QSettings::AccessError) {
#ifdef _WIN32
      // On NTFS file systems, ownership and permissions checking is disabled by default
      // for performance reasons. source:
      // https://doc.qt.io/qt-6/qfileinfo.html#ntfs-permissions
      QNtfsPermissionCheckGuard permissionGuard;
#endif
      QFile file(fileName);
      auto attrs = file.permissions();
      if (file.exists() && !file.isWritable() && file.isReadable()) {
        QMessageBox::StandardButton result =
            MOBase::TaskDialog(qApp->activeModalWidget(),
                               QObject::tr("INI file is read-only"))
                .main(QObject::tr("INI file is read-only"))
                .content(QObject::tr("Mod Organizer is attempting to write to \"%1\" "
                                     "which is currently set to read-only.")
                             .arg(fileName))
                .icon(QMessageBox::Warning)
                .button({QObject::tr("Clear the read-only flag"), QMessageBox::Yes})
                .button({QObject::tr("Allow the write once"),
                         QObject::tr("The file will be set to read-only again."),
                         QMessageBox::Ignore})
                .button({QObject::tr("Skip this file"), QMessageBox::No})
                .remember("clearReadOnly", fileName)
                .exec();

        // clear the read-only flag if requested
        if (result & (QMessageBox::Yes | QMessageBox::Ignore)) {
          attrs |= QFile::Permission::WriteUser;
          if (file.setPermissions(attrs)) {
            if (SetValue(appName, keyName, value, settings)) {
              success = true;
            }
          }
        }

        // set the read-only flag if requested
        if (result == QMessageBox::Ignore) {
          attrs &= ~QFile::Permission::WriteUser;
          file.setPermissions(attrs);
        }
      }
    }
  }

  return success;
}

}  // namespace MOBase
