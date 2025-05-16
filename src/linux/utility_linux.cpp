#include "log.h"
#include "report.h"
#include "utility.h"

#include <KIO/CopyJob>
#include <KIO/DeleteJob>
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

inline int jobErrorToErrno(int error)
{
  using namespace KIO;
  switch (error) {
  case ERR_CANNOT_OPEN_FOR_READING:
  case ERR_CANNOT_OPEN_FOR_WRITING:
    return EIO;
  case ERR_MALFORMED_URL:
  case ERR_NO_SOURCE_PROTOCOL:
    return EINVAL;
  case ERR_UNSUPPORTED_PROTOCOL:
    return EPROTONOSUPPORT;
  case ERR_UNSUPPORTED_ACTION:
    return ENOTSUP;
  case ERR_IS_DIRECTORY:
    return EISDIR;
  case ERR_DOES_NOT_EXIST:
    return ENOENT;
  case ERR_FILE_ALREADY_EXIST:
  case ERR_DIR_ALREADY_EXIST:
    return EEXIST;
  case ERR_ACCESS_DENIED:
  case ERR_WRITE_ACCESS_DENIED:
  case ERR_CANNOT_ENTER_DIRECTORY:
    return EACCES;
  case ERR_PROTOCOL_IS_NOT_A_FILESYSTEM:
    return EPROTO;
  case ERR_USER_CANCELED:
  case ERR_ABORTED:
    return ECANCELED;
  case ERR_DISK_FULL:
    return ENOSPC;
  case ERR_OUT_OF_MEMORY:
    return ENOMEM;

  default:
    return EIO;
  }
}

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
  // set some defaults for copy jobs
  if (dynamic_cast<KIO::CopyJob*>(job)) {
    dynamic_cast<KIO::CopyJob*>(job)->setWriteIntoExistingDirectories(true);
  }
  KJobWidgets::setWindow(job, dialog);

  // event loop is required to process input in confirmation dialogs
  QEventLoop eventLoop;
  QObject::connect(job, &KIO::Job::result, &eventLoop, &QEventLoop::quit);

  job->start();
  eventLoop.exec();

  int error = job->error();
  if (error != 0) {
    errno = jobErrorToErrno(error);
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
    KIO::CopyJob* job =
        KIO::copy(stringListToUrlList(sourceNames),
                  QUrl::fromLocalFile(destinationNames.first()), KIO::HideProgressInfo);
    return runJob(job, dialog);
  }

  for (qsizetype i = 0; i < sourceNames.size(); i++) {
    auto* job =
        KIO::copy(QUrl::fromLocalFile(sourceNames.at(i)),
                  QUrl::fromLocalFile(destinationNames.at(i)), KIO::HideProgressInfo);
    if (!runJob(job, dialog)) {
      return false;
    }
  }
  return true;
}

bool shellCopy(const QString& sourceNames, const QString& destinationNames,
               bool yesToAll, QWidget* dialog)
{
  KIO::CopyJob* job = KIO::copy(
      QUrl::fromLocalFile(sourceNames), QUrl::fromLocalFile(destinationNames),
      KIO::HideProgressInfo | (yesToAll ? KIO::Overwrite : KIO::DefaultFlags));
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
    auto* job =
        KIO::move(stringListToUrlList(sourceNames),
                  QUrl::fromLocalFile(destinationNames.first()), KIO::HideProgressInfo);
    return runJob(job);
  }
  for (qsizetype i = 0; i < sourceNames.size(); i++) {
    auto* job =
        KIO::move(QUrl::fromLocalFile(sourceNames.at(i)),
                  QUrl::fromLocalFile(destinationNames.at(i)), KIO::HideProgressInfo);
    if (!runJob(job, dialog)) {
      return false;
    }
  }

  return true;
}

bool shellMove(const QString& sourceNames, const QString& destinationNames,
               bool yesToAll, QWidget* dialog)
{
  auto* job = KIO::move(
      QUrl::fromLocalFile(sourceNames), QUrl::fromLocalFile(destinationNames),
      KIO::HideProgressInfo | (yesToAll ? KIO::Overwrite : KIO::DefaultFlags));
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
    job = KIO::trash(files, KIO::HideProgressInfo);
  } else {
    job = KIO::del(files, KIO::HideProgressInfo);
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

  void writeErrorToPipe(int pipeFd, int error)
  {
    ssize_t bytesWritten = write(pipeFd, &error, sizeof(int));
    if (bytesWritten == -1) {
      const int writeError = errno;
      log::warn("Error writing error to pipe, {}.\nError was {} ({})",
                strerror(writeError), strerror(error), strerrorname_np(error));
    }
  }

  Result Execute(const QString& program, const QString& params, const QString& workdir)
  {
    // clang-format off
    /*
    source: https://stackoverflow.com/a/3703179
    1. Before forking, open a pipe in the parent process.
    2. After forking, the parent closes the writing end of the pipe
       and reads from the reading end.
    3. The child closes the reading end and sets the close-on-exec flag
       for the writing end.
    4. The child calls exec.
    5. If exec fails, the child writes the error code back to the parent using the pipe,
       then exits.
    6. The parent reads eof (a zero-length read) if the child successfully performed exec,
       since close-on-exec made successful exec close the writing end of the pipe.
       Or, if exec failed, the parent reads the error code and can proceed accordingly.
       Either way, the parent blocks until the child calls exec.
    7. The parent closes the reading end of the pipe.
    */
    // clang-format on

    // pipefd[0] refers to the read end of the pipe.
    // pipefd[1] refers to the write end of the pipe.
    int pipefd[2];

    int result = pipe(pipefd);
    if (result == -1) {
      return Result::makeFailure(EPIPE, u"Could not open pipe"_s);
    }

    pid_t pid = fork();

    if (pid == -1) {  // error
      close(pipefd[0]);
      close(pipefd[1]);

      const int error = errno;
      return Result::makeFailure(
          error, QStringLiteral("Could not fork, %1").arg(strerror(error)));
    }

    if (pid == 0) {  // child
      // close read end
      close(pipefd[0]);
      // set CLOEXEC on write end
      fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);

      if (!workdir.isEmpty()) {
        int r = chdir(workdir.toLocal8Bit());
        if (r == -1) {
          const int error = errno;
          log::error(
              "Error changing directory to '{}', {}.\nWorking directory was '{}'",
              workdir, strerror(error), QDir::currentPath());

          writeErrorToPipe(pipefd[1], error);
          exit(error);
        }
      }

      QString command = program % u" "_s % params;

      execl("/bin/sh", "sh", "-c", command.toLocal8Bit(), nullptr);

      // The exec() functions return only if an error has occurred. The return value is
      // -1, and errno is set to indicate the error.
      const int error = errno;

      writeErrorToPipe(pipefd[1], error);

      exit(error);
    }

    // parent

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

  QString toUNC(const QFileInfo& path)
  {
    return path.absoluteFilePath();
  }

  QString formatError(int i)
  {
    return strerror(i);
  }

  Result Rename(const QFileInfo& src, const QFileInfo& dest, bool copyAllowed)
  {
    (void)copyAllowed;

    std::error_code ec;
    std::filesystem::rename(src.filesystemAbsoluteFilePath(),
                            dest.filesystemAbsoluteFilePath(), ec);

    if (ec) {
      return Result::makeFailure(ec.value());
    }
    return Result::makeSuccess();
  }

  Result Delete(const QFileInfo& path)
  {
    std::error_code ec;
    std::filesystem::remove(path.filesystemAbsoluteFilePath(), ec);

    if (ec) {
      return Result::makeFailure(ec.value());
    }

    return Result::makeSuccess();
  }

  Result CreateDirectories(const QDir& dir)
  {
    std::error_code ec;
    std::filesystem::create_directories(dir.filesystemAbsolutePath(), ec);

    if (ec) {
      return Result::makeFailure(ec.value());
    }

    return Result::makeSuccess();
  }

  Result DeleteDirectoryRecursive(const QDir& dir)
  {
    std::error_code ec;
    std::filesystem::remove_all(dir.filesystemPath(), ec);

    if (ec) {
      return Result::makeFailure(ec.value(), ToQString(ec.message()));
    }

    return Result::makeSuccess();
  }

}  // namespace shell

}  // namespace MOBase