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
#include <QBuffer>
#include <QDir>
#include <QErrorMessage>
#include <bit7z/bitarchivereader.hpp>
#include <cerrno>
#include <format>
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

enum spawnAction
{
  spawn,
  spawnp
};

bool shellDelete(const QStringList& fileNames, bool recycle, QWidget* dialog)
{
  (void)dialog;  // suppress unused parameter warning

  bool result = true;
  for (const auto& fileName : fileNames) {
    bool r;
    QFile file = fileName;
    if (recycle) {
      r = file.moveToTrash();
    } else {
      r = file.remove();
    }
    if (!r) {
      result = r;
      int e  = errno;
      log::error("error deleting file '{}': ", formatSystemMessage(e));
    }
  }
  return result;
}

bool shellCopy(const QStringList& sourceNames, const QStringList& destinationNames,
               QWidget* dialog)
{
  if (sourceNames.length() != destinationNames.length() &&
      destinationNames.length() != 1) {
    return false;
  }

  QStringList destinations;
  for (qsizetype i = 0; i < sourceNames.length(); i++) {
    if (destinationNames.length() == 1) {
      destinations.append(QFileInfo(destinationNames[0]).absolutePath() + "/" +
                          QFileInfo(sourceNames[i]).fileName());
    } else {
      destinations.append(QFileInfo(destinationNames[i]).absolutePath());
    }
  }

  bool yesToAll = false;

  for (qsizetype i = 0; i < sourceNames.length(); i++) {
    QFile src = sourceNames[i];
    QFile dst = destinations[i];
    // prompt user if file already exists
    if (dst.exists()) {
      if (yesToAll) {
        dst.remove();
      }

      QMessageBox msgBox;
      msgBox.setText("Target file already exists");
      msgBox.setDetailedText(
          QString("\"%1\" already exists. Would you like to overwrite it?")
              .arg(destinations[i]));
      msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::YesToAll |
                                QMessageBox::No | QMessageBox::Cancel);
      msgBox.setDefaultButton(QMessageBox::Cancel);
      msgBox.setParent(dialog);

      int result = msgBox.exec();
      switch (result) {
      case QMessageBox::YesToAll:
        yesToAll = true;
        [[fallthrough]];
      case QMessageBox::Yes:
        // delete destination file, QFile::copy cannot directly overwrite files
        if (!dst.remove()) {
          if (dst.error() == QFileDevice::RemoveError) {
            QErrorMessage().showMessage("The file could not be overwritten.");
          } else {
            QErrorMessage().showMessage(dst.errorString());
          }
          return false;
        }
        break;
      case QMessageBox::No:
        continue;
      case QMessageBox::Cancel:
      default:
        return false;
      }
    }

    if (!src.copy(destinations[i])) {
      QErrorMessage().showMessage(src.errorString());
      return false;
    }
  }

  return true;
}

namespace shell
{

  static QString g_urlHandler;

  void LogShellFailure(const char* file, const std::vector<const char*>& params,
                       int error)
  {
    QStringList s;

    if (file) {
      s << ToQString(file);
    }

    if (!params.empty()) {
      for (const auto param : params) {
        s << ToQString(param);
      }
    }

    log::error("failed to invoke '{}': {}", s.join(" "), formatSystemMessage(error));
  }

  Result ShellExecuteWrapper(spawnAction operation, const char* file,
                             vector<const char*> params)
  {
    char** environ = nullptr;

    pid_t pid  = 0;
    int status = -1;

    // The only difference between posix_spawn() and posix_spawnp() is the manner in
    // which they specify the file to be executed by the child process. With
    // posix_spawn(), the executable file is specified as a pathname (which can be
    // absolute or relative). With posix_spawnp(), the executable file is specified as a
    // simple filename; the system searches for this file in the list of directories
    // specified by PATH (in the same way as for execvp(3)).
    switch (operation) {
    case spawn:
      status = posix_spawn(&pid, file, nullptr, nullptr,
                           const_cast<char**>(params.data()), environ);
      break;
    case spawnp:
      status = posix_spawnp(&pid, file, nullptr, nullptr,
                            const_cast<char**>(params.data()), environ);
      break;
    }

    if (status != 0) {
      const auto e = errno;
      LogShellFailure(file, params, e);

      return Result::makeFailure(e, ToQString(formatSystemMessage(e)));
    }

    return Result::makeSuccess(pid);
  }

  Result ShellExecuteWrapper(spawnAction operation, const char* file, const char* param)
  {
    return ShellExecuteWrapper(operation, file, vector<const char*>{param});
  }

  pid_t Result::processHandle() const
  {
    return m_process;
  }

  Result ExploreDirectory(const QFileInfo& info)
  {
    const auto path = QDir::toNativeSeparators(info.absoluteFilePath());

    return ShellExecuteWrapper(spawnp, "xdg-open", path.toStdString().c_str());
  }

  Result ExploreFileInDirectory(const QFileInfo& info)
  {
    const auto path = QDir::toNativeSeparators(info.absoluteFilePath());

    return ShellExecuteWrapper(spawnp, "xdg-open", path.toStdString().c_str());
  }

  void SetUrlHandler(const QString& cmd)
  {
    g_urlHandler = cmd;
  }

  Result Open(const QString& path)
  {
    const auto s_path = path.toStdString();
    return ShellExecuteWrapper(spawnp, "xdg-open", s_path.c_str());
  }

  Result OpenCustomURL(const std::string& format, const std::string& url)
  {
    log::debug("custom url handler: '{}'", format);

    auto cmd = std::format("'{}' '{}'", format, url);
    log::debug("running {}", cmd);

    Result r = ShellExecuteWrapper(spawn, format.c_str(), url.c_str());

    if (r.processHandle() == 0) {
      log::error("failed to run '{}'", cmd);
      log::error("{}", formatSystemMessage(r.error()));
      log::error(
          "{}",
          QObject::tr("You have an invalid custom browser command in the settings."));
      return r;
    }

    return Result::makeSuccess();
  }

  Result Open(const QUrl& url)
  {
    log::debug("opening url '{}'", url.toString());

    const auto s_url = url.toString().toStdString();

    if (g_urlHandler.isEmpty()) {
      return ShellExecuteWrapper(spawnp, "xdg-open", s_url.c_str());
    }

    return OpenCustomURL(g_urlHandler.toStdString(), s_url);
  }

  Result Execute(const QString& program, const QString& params)
  {
    const auto program_s = program.toStdString();
    const auto params_s  = params.toStdString();

    return ShellExecuteWrapper(spawn, program_s.c_str(), params_s.c_str());
  }

}  // namespace shell

QString ToString(const SYSTEMTIME& time)
{
  QDateTime t = QDateTime::fromSecsSinceEpoch(time.tv_sec);

  return t.toString(QLocale::system().dateFormat());
}

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