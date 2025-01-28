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
#include <bit7z/bitarchivereader.hpp>
#include <cerrno>
#include <spawn.h>

using namespace std;
namespace fs = std::filesystem;

static inline constexpr string SEVEN_ZIP_LIB = "lib/lib7zip.so";

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

// TODO: parse pe file directly
QIcon iconForExecutable(const QString& filepath)
{
  try {
    using namespace bit7z;

    Bit7zLibrary lib{SEVEN_ZIP_LIB};
    BitArchiveReader reader{lib, filepath.toStdString(), BitFormat::Pe};
    auto items = reader.items();

    vector<BitArchiveItemInfo> icons;
    for (const auto& item : items) {
      if (item.extension() == ".ico") {
        icons.emplace_back(item);
      }
    }

    // determine largest icon
    uint32_t largestIconIndex = 0;
    uint64_t largestIconSize  = 0;

    for (const auto& icon : icons) {
      if (icon.size() > largestIconSize) {
        largestIconSize  = icon.size();
        largestIconIndex = icon.index();
      }
    }

    std::vector<byte_t> buffer;
    reader.extractTo(buffer, largestIconIndex);

    auto byteArray = QByteArray((const char*)buffer.data(), buffer.size());

    QPixmap pixmap;
    if (!pixmap.loadFromData(byteArray)) {
      return QIcon(":/MO/gui/executable");
    }
    return QIcon(pixmap);
  } catch (const bit7z::BitException& ex) {
    cerr << ex.what() << endl;
    return QIcon(":/MO/gui/executable");
  }
}

enum version_t
{
  fileversion,
  productversion
};

// TODO: parse pe file directly
QString getFileVersionInfo(QString const& filepath, version_t type)
{
  try {
    using namespace bit7z;

    Bit7zLibrary lib{SEVEN_ZIP_LIB};

    std::vector<byte_t> buffer;

    BitArchiveReader reader{lib, filepath.toStdString(), BitFormat::Pe};
    auto items = reader.items();

    vector<BitArchiveItemInfo> versions;
    for (const auto& item : items) {
      if (item.name() == "version.txt") {
        versions.emplace_back(item);
      }
    }

    reader.extractTo(buffer, versions.at(0).index());

    auto stream = QTextStream(QByteArray(buffer), QIODeviceBase::ReadOnly);
    stream.setEncoding(QStringConverter::Utf16);

    QString version;

    QString keyword;
    switch (type) {
    case fileversion:
      keyword = "FILEVERSION";
      break;
    case productversion:
      keyword = "PRODUCTVERSION";
      break;
    }

    // convert
    // FILEVERSION     1,3,22,0
    // to
    // 1.3.22.0

    while (!stream.atEnd()) {
      auto line = stream.readLine();
      if (line.startsWith(keyword)) {
        line.remove(0, keyword.length());
        // remove whitespaces
        version = line.trimmed();
        // replace ',' with '.'
        version.replace(',', '.');
        break;
      }
    }

    return version;
  } catch (const bit7z::BitException& ex) {
    cerr << ex.what() << endl;
    return {};
  }
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