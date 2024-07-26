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

#include "utility.h"
#include "log.h"
#include "report.h"
#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QStringEncoder>
#include <QUuid>
#include <format>
#include <iostream>

#include <QStandardPaths>
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
      for (auto param : params) {
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

  HANDLE Result::processHandle() const
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
    } else {
      return OpenCustomURL(g_urlHandler.toStdString(), s_url);
    }
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
  auto result = shell::ShellExecuteWrapper(
      spawnp, "7z",
      {"x", filepath.toStdString().c_str(), ".rsrc/ICON/1", "-o /tmp/mo2/.rsrc/"});
  if (result.success()) {
    return QIcon("/tmp/mo-icon.ico");
  }

  return QIcon(":/MO/gui/executable");
}

enum version_t
{
  fileversion,
  productversion
};

QString getFileVersionInfo(QString const& filepath, version_t type)
{
  auto result = shell::ShellExecuteWrapper(
      spawnp, "7z",
      {"x", filepath.toStdString().c_str(), ".rsrc/version.txt", "-o /tmp/mo2/.rsrc/"});

  QFile versionFile("/tmp/mo2/.rsrc/version.txt");
  QString version = "1.0.0";

  string keyword;
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
  if (versionFile.isOpen()) {
    return version;
  }
  while (!versionFile.atEnd()) {
    auto line = versionFile.readLine();
    if (line.startsWith(keyword)) {
      line.remove(0, keyword.length());
      // remove whitespaces
      version = line.trimmed();
      // replace '' with ''
      version.replace(',', '.');
      break;
    }
  }

  return version;
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
