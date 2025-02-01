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

#include "linux/PeExtractor.h"
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

QIcon iconForExecutable(const QString& filepath)
{
  QFile exeFile(filepath);
  QBuffer buffer;

  if (!exeFile.open(QIODeviceBase::ReadOnly) ||
      !buffer.open(QIODeviceBase::ReadWrite)) {
    return QIcon(QStringLiteral(":/MO/gui/executable"));
  }

  PeExtractor extractor(&exeFile, &buffer);

  if (!extractor.loadIconData()) {
    return QIcon(QStringLiteral(":/MO/gui/executable"));
  }

  QPixmap pixmap;
  if (!pixmap.loadFromData(buffer.buffer())) {
    return QIcon(QStringLiteral(":/MO/gui/executable"));
  }

  return QIcon(pixmap);
}

enum version_t
{
  fileversion,
  productversion
};

QString getFileVersionInfo(QString const& filepath, version_t type)
{
  QFile exeFile(filepath);
  if (!exeFile.open(QIODeviceBase::ReadOnly)) {
    return {};
  }

  QBuffer buffer;
  if (!buffer.open(QIODeviceBase::ReadWrite)) {
    return {};
  }

  PeExtractor extractor(&exeFile, &buffer);

  if (!extractor.loadVersionData()) {
    return {};
  }

  QString fileVersion, productVersion;

  buffer.seek(0);
  QDataStream stream(&buffer);
  stream >> fileVersion >> productVersion;

  switch (type) {

  case fileversion:
    return fileVersion;
  case productversion:
    return productVersion;
  }
  return {};
}

QString getFileVersion(QString const& filepath)
{
  return getFileVersionInfo(filepath, fileversion);
}

QString getProductVersion(QString const& filepath)
{
  return getFileVersionInfo(filepath, productversion);
}

}  // namespace MOBase