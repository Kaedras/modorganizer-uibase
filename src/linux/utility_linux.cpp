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

#include <KIO/CopyJob>
#include <KIO/DeleteJob>
#include <KIO/DeleteOrTrashJob>
#include <KIO/JobUiDelegateFactory>
#include <KJobWidgets>
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

QList<QUrl> stringListToUrlList(const QStringList& list)
{
  QList<QUrl> urls;
  urls.reserve(list.size());
  for (const auto& string : list) {
    urls.push_back(QUrl::fromLocalFile(string));
  }
  return urls;
}

bool runJob(KIO::Job* job, QWidget* dialog = nullptr)
{
  KJobWidgets::setWindow(job, dialog);

  // event loop is required to process input in confirmation dialogs
  QEventLoop eventLoop;
  QObject::connect(job, &KIO::Job::result, &eventLoop, &QEventLoop::quit);

  job->start();
  eventLoop.exec();

  if (job->error() != 0) {
    errno = job->error();
    return false;
  }
  return true;
}

bool shellCopy(const QStringList& sourceNames, const QStringList& destinationNames,
               QWidget* dialog)
{
  if (sourceNames.size() != destinationNames.size() && destinationNames.size() != 1) {
    errno = EINVAL;
    return false;
  }

  if (destinationNames.size() == 1) {
    KIO::CopyJob* job = KIO::copy(stringListToUrlList(sourceNames),
                                  QUrl::fromLocalFile(destinationNames[0]));
    return runJob(job, dialog);
  }

  for (qsizetype i = 0; i < sourceNames.size(); i++) {
    auto* job = KIO::copy(QUrl::fromLocalFile(sourceNames[i]),
                          QUrl::fromLocalFile(destinationNames[i]));
    if (!runJob(job, dialog)) {
      return false;
    }
  }
  return true;
}

bool shellCopy(const QString& sourceNames, const QString& destinationNames,
               bool yesToAll, QWidget* dialog)
{
  KIO::CopyJob* job =
      KIO::copy(QUrl::fromLocalFile(sourceNames), QUrl::fromLocalFile(destinationNames),
                yesToAll ? KIO::Overwrite : KIO::DefaultFlags);
  return runJob(job, dialog);
}

bool shellMove(const QStringList& sourceNames, const QStringList& destinationNames,
               QWidget* dialog)
{
  if (sourceNames.size() != destinationNames.size() && destinationNames.size() != 1) {
    errno = EINVAL;
    return false;
  }
  if (destinationNames.size() == 1) {
    auto* job = KIO::move(stringListToUrlList(sourceNames),
                          QUrl::fromLocalFile(destinationNames[0]));
    return runJob(job);
  }
  for (qsizetype i = 0; i < sourceNames.size(); i++) {
    auto* job = KIO::move(QUrl::fromLocalFile(sourceNames[i]),
                          QUrl::fromLocalFile(destinationNames[i]));
    if (!runJob(job, dialog)) {
      return false;
    }
  }

  return true;
}

bool shellMove(const QString& sourceNames, const QString& destinationNames,
               bool yesToAll, QWidget* dialog)
{
  auto* job =
      KIO::move(QUrl::fromLocalFile(sourceNames), QUrl::fromLocalFile(destinationNames),
                yesToAll ? KIO::Overwrite : KIO::DefaultFlags);
  return runJob(job, dialog);
}

bool shellRename(const QString& oldName, const QString& newName, bool yesToAll,
                 QWidget* dialog)
{
  return shellMove(oldName, newName, yesToAll, dialog);
}

bool shellDelete(const QStringList& fileNames, bool recycle, QWidget* dialog)
{
  KIO::Job* job     = nullptr;
  QList<QUrl> files = stringListToUrlList(fileNames);

  if (recycle) {
    job = KIO::trash(files);
  } else {
    job = KIO::del(files);
  }

  return runJob(job, dialog);
}

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
    // clang-format off
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
    // clang-format on
    QDBusInterface interface(u"org.freedesktop.FileManager1"_s,
                             u"/org/freedesktop/FileManager1"_s,
                             u"org.freedesktop.FileManager1"_s);
    QDBusMessage response = interface.call(
        u"ShowItems"_s, QStringList(u"file://"_s % info.absoluteFilePath()), "");
    if (response.type() == QDBusMessage::ErrorMessage) {
      return Result::makeFailure(static_cast<uint32_t>(response.type()),
                                 response.errorMessage());
    }
    return Result::makeSuccess();
  }

  Result Execute(const QString& program, const QString& params)
  {
    // create argument array for execvp
    vector<const char*> args;
    // split arguments
    QStringList argList = QProcess::splitCommand(params);
    // first argument is executable
    args.reserve(argList.size() + 1);
    args.push_back(program.toLocal8Bit());
    for (const auto& arg : argList) {
      args.push_back(arg.toLocal8Bit());
    }
    // array must be terminated by a null pointer
    args.push_back(nullptr);

    /*
    source: https://stackoverflow.com/a/3703179
    1. Before forking, open a pipe in the parent process.
    2. After forking, the parent closes the writing end of the pipe and reads from the
    reading end.
    3. The child closes the reading end and sets the close-on-exec flag for the writing
    end.
    4. The child calls exec.
    5. If exec fails, the child writes the error code back to the parent using the pipe,
    then exits.
    6. The parent reads eof (a zero-length read) if the child successfully performed
    exec, since close-on-exec made successful exec close the writing end of the pipe.
    Or, if exec failed, the parent reads the error code and can proceed accordingly.
    Either way, the parent blocks until the child calls exec.
    7. The parent closes the reading end of the pipe.
    */

    // pipefd[0] refers to the read end of the pipe. pipefd[1] refers to the write end
    // of the pipe.
    int pipefd[2];

    int result = pipe(pipefd);
    if (result == -1) {
      return Result::makeFailure(EPIPE, u"Could not open pipe"_s);
    }

    pid_t pid = fork();

    switch (pid) {
    case 0:  // child
    {
      // close read end
      close(pipefd[0]);
      // set CLOEXEC on write end
      fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);

      // from man 3 exec: If the specified filename includes a slash character, then
      // PATH is ignored, and the file at the specified pathname is executed.
      execvp(args[0], const_cast<char* const*>(args.data()));

      // The exec() functions return only if an error has occurred. The return value is
      // -1, and errno is set to indicate the error.
      const int error = errno;

      ssize_t bytesWritten = write(pipefd[1], &error, sizeof(int));
      if (bytesWritten == -1) {
        const int writeError = errno;
        log::warn("Error writing exec error to pipe, {}.\nExec error was {}",
                  strerror(writeError), strerror(error));
      }

      exit(error);
    }
    case -1:  // error
    {
      const int error = errno;
      return Result::makeFailure(
          error, QStringLiteral("Could not fork, %1").arg(strerror(error)));
    }
    default:  // parent
    {
      // close write end
      close(pipefd[1]);

      int buf;

      size_t count = read(pipefd[0], &buf, sizeof(int));

      // close read end
      close(pipefd[0]);
      if (count == 0) {
        // success
        return Result::makeSuccess(pidfd_open(pid, 0));
      }

      return Result::makeFailure(buf, QString::fromStdString(strerror(buf)));
    }
    }
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

    // split cmd into program and arguments
    QString program, args;
    if (cmd.contains(' ')) {
      auto pos = cmd.indexOf(' ');

      program = cmd.mid(0, pos);
      args    = cmd.sliced(pos);
    } else {
      program = cmd;
    }

    return Execute(program, args);
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