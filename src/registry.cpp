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
#include "qinipp.h"
#include "report.h"
#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include <QString>
#include <fstream>
#ifdef __unix__
#include "linux/compatibility.h"
#endif

namespace
{

// helper function that mirrors the behaviour of WritePrivateProfileString as described
// in
// https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-writeprivateprofilestringw
bool SetValue(const QString& appName, const QString& keyName, const QString& value,
              const QString& fileName)
{
  qinipp::Ini ini;

  // read ini file if it exists
  if (QFile::exists(fileName)) {
    QFile in(fileName);
    if (!in.open(QIODeviceBase::ReadOnly | QIODeviceBase::Text)) {
      return false;
    }
    QTextStream inStream(&in);
    ini.parse(inStream);
    in.close();
  }

  if (keyName.isNull()) {
    // remove section if key is null
    ini.sections.erase(appName);
  } else if (value.isNull()) {
    // remove key if value is null
    ini.sections[appName].erase(keyName);
  } else {
    ini.sections[appName][keyName] = value;
  }

  // write the modified ini file
  QFile out(fileName);
  if (!out.open(QIODeviceBase::WriteOnly | QIODeviceBase::Text)) {
    return false;
  }
  QTextStream outStream(&out);
  ini.generate(outStream);
  return ini.errors.empty();
}

}  // namespace

namespace MOBase
{

bool WriteRegistryValue(const QString& appName, const QString& keyName,
                        const QString& value, const QString& fileName)
{
  bool success = true;

  if (!SetValue(appName, keyName, value, fileName)) {
    const int error = GetLastError();
    success         = false;
    if (error == ERROR_ACCESS_DENIED) {
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
                             .arg(file.fileName()))
                .icon(QMessageBox::Warning)
                .button({QObject::tr("Clear the read-only flag"), QMessageBox::Yes})
                .button({QObject::tr("Allow the write once"),
                         QObject::tr("The file will be set to read-only again."),
                         QMessageBox::Ignore})
                .button({QObject::tr("Skip this file"), QMessageBox::No})
                .remember("clearReadOnly", file.fileName())
                .exec();

        // clear the read-only flag if requested
        if (result & (QMessageBox::Yes | QMessageBox::Ignore)) {
          attrs |= QFile::Permission::WriteUser;
          if (file.setPermissions(attrs)) {
            if (SetValue(appName, keyName, value, fileName)) {
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

QString ReadRegistryValue(const QString& appName, const QString& keyName,
                          const QString& defaultValue, const QString& fileName)
{
  // read ini file
  qinipp::Ini ini;
  {
    // read ini file
    if (QFile::exists(fileName)) {
      QFile in(fileName);
      if (!in.open(QIODeviceBase::ReadOnly | QIODeviceBase::Text)) {
        return defaultValue;
      }
      QTextStream inStream(&in);
      ini.parse(inStream);
      in.close();
    }
  }

  if (appName.isNull()) {
    // return all section names in the file
    QString result;
    for (const auto& sectionName : ini.sections | std::views::keys) {
      result.append(sectionName);
      result += '\n';
    }
    result.chop(1);
    return result;
  }
  if (keyName.isNull()) {
    // return all key names in the section specified by the appName parameter
    QString result;
    if (!ini.sections.contains(appName)) {
      return result;
    }
    for (const auto& key : ini.sections[appName] | std::views::keys) {
      result.append(key);
      result += '\n';
    }
    result.chop(1);
    return result;
  }

  try {
    return ini.sections.at(appName).at(keyName);
  } catch (...) {
    return defaultValue;
  }
}

}  // namespace MOBase
