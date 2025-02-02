/*
Mod Organizer shared UI functionality

Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "log.h"
#include "report.h"
#include "utility.h"

#include <QApplication>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDir>
#include <cerrno>
#include <spawn.h>

using namespace std;
namespace fs = std::filesystem;

namespace MOBase
{

std::string formatSystemMessage(int id)
{
  return strerror(id);
}

namespace shell
{

  Result ExploreFileInDirectory(const QFileInfo& info)
  {
    /*
     interface specification:
    <interface name='org.freedesktop.FileManager1'>
    <method name='ShowFolders'>
      <arg type='as' name='URIs' direction='in'/>
      <arg type='s' name='StartupId' direction='in'/>
    </method>
    <method name='ShowItems'>
      <arg type='as' name='URIs' direction='in'/>
      <arg type='s' name='StartupId' direction='in'/>
    </method>
    <method name='ShowItemProperties'>
      <arg type='as' name='URIs' direction='in'/>
      <arg type='s' name='StartupId' direction='in'/>
    </method>
  </interface>
  */
    QDBusInterface interface("org.freedesktop.FileManager1",
                             "/org/freedesktop/FileManager1",
                             "org.freedesktop.FileManager1");
    interface.call("org.freedesktop.FileManager1.ShowItems",
                   QStringList("file://" + info.absoluteFilePath()), "");
    auto errorType = interface.lastError().type();
    if (errorType != QDBusError::NoError) {
      return Result::makeFailure((int)errorType, QDBusError::errorString(errorType));
    }
    return Result::makeSuccess();
  }

}  // namespace shell

QString ToString(const SYSTEMTIME& time)
{
  QDateTime t = QDateTime::fromSecsSinceEpoch(time.tv_sec);

  return t.toString(QLocale::system().dateFormat());
}

}  // namespace MOBase