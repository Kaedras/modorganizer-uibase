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
#include <QProcess>
#include <cerrno>
#include <fcntl.h>
#include <spawn.h>

extern "C"
{
#include <sys/pidfd.h>
}

using namespace std;
using namespace Qt::Literals::StringLiterals;
namespace fs = std::filesystem;

namespace MOBase
{

std::string formatSystemMessage(int id)
{
  return strerror(id);
}

namespace shell
{
  static QString g_urlHandler;

  void SetUrlHandler(const QString& cmd)
  {
    g_urlHandler = cmd;
  }

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
    QDBusInterface interface(u"org.freedesktop.FileManager1"_s,
                             u"/org/freedesktop/FileManager1"_s,
                             u"org.freedesktop.FileManager1"_s);
    interface.call(u"org.freedesktop.FileManager1.ShowItems"_s,
                   QStringList(u"file://"_s % info.absoluteFilePath()), "");
    auto errorType = interface.lastError().type();
    if (errorType != QDBusError::NoError) {
      return Result::makeFailure((int)errorType, QDBusError::errorString(errorType));
    }
    return Result::makeSuccess();
  }

  extern Result OpenURL(const QUrl& url);

  Result OpenCustomURL(const QString& format, const QString& url)
  {
    log::debug("custom url handler: '{}'", format);

    QString formatStr = format;

    // remove %2 %3 ... %98 %99
    static auto regex = QRegularExpression(u"%([2-9]|[1-9][0-9](?![0-9]))"_s);
    formatStr.replace(regex, "");

    QString cmd = QString(formatStr).arg(url);

    log::debug("running '{}'", cmd);

    auto cmdList = QProcess::splitCommand(cmd);

    QProcess p;
    qint64 pid;
    p.setProgram(cmdList.takeFirst());
    p.setArguments(cmdList);
    if (!p.startDetached(&pid)) {
      log::error("failed to run '{}'", cmd);
      log::error("{}", p.errorString());
      log::error(
          "{}",
          QObject::tr("You have an invalid custom browser command in the settings."));
      return Result::makeFailure(p.error(), p.errorString());
    }
    return Result::makeSuccess(pidfd_open(static_cast<pid_t>(pid), 0));
  }

  Result Open(const QUrl& url)
  {
    if (g_urlHandler.isEmpty()) {
      return OpenURL(url);
    }
    return OpenCustomURL(g_urlHandler, url.toString(QUrl::FullyEncoded));
  }

  HANDLE GetHandleFromPid(qint64 pid)
  {
    return pidfd_open(static_cast<pid_t>(pid), 0);
  }

  QString toUNC(const QFileInfo& path)
  {
    return path.absoluteFilePath();
  }

  QString formatError(int i)
  {
    return strerror(i);
  }

}  // namespace shell

QString ToString(const SYSTEMTIME& time)
{
  QDateTime t = QDateTime::fromSecsSinceEpoch(time.tv_sec);

  return t.toString(QLocale::system().dateFormat());
}

}  // namespace MOBase