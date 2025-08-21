#include "utility.h"

#include "../pch.h"
#include "linux/stub.h"
#include <QApplication>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QDir>
#include <QStringList>
#include <cerrno>
#include <fcntl.h>
#include <format>
#include <log.h>
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

int fileErrorToErrno(QFile::FileError error)
{
  switch (error) {
  case QFile::NoError:
    return 0;
  case QFile::ReadError:
  case QFile::WriteError:
  case QFile::FatalError:
    return EIO;
  case QFile::ResourceError:
    return ENOENT;
  case QFile::OpenError:
    return EACCES;
  case QFile::AbortError:
    return ECANCELED;
  case QFile::TimeOutError:
    return ETIMEDOUT;
  case QFile::UnspecifiedError:
  case QFile::RemoveError:
  case QFile::RenameError:
  case QFile::PositionError:
  case QFile::ResizeError:
  case QFile::PermissionsError:
  case QFile::CopyError:
    return EIO;
  default:
    return EINVAL;
  }
}

enum op : unsigned int
{
  FO_COPY,
  FO_MOVE
};

bool doOperation(const fs::path& src, const fs::path& dst, QWidget* dialog,
                 op operation, bool yesToAll, bool silent = false)
{
  try {
    if (exists(dst) && !yesToAll) {
      if (silent) {
        errno = EEXIST;
        return false;
      }

      QMessageBox msg;
      msg.setText(QStringLiteral("File '%1' already exists")
                      .arg(QString::fromStdString(dst.string())));
      msg.setInformativeText(
          QStringLiteral(
              "Would you like to overwrite it?\nSource size: %1, destination size: %2")
              .arg(file_size(src))
              .arg(file_size(dst)));
      msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
      msg.setDefaultButton(QMessageBox::Yes);
      msg.setParent(dialog);

      int result = msg.exec();
      if (result == QMessageBox::No) {
        errno = EEXIST;
        return false;
      }
    }
    if (operation == FO_COPY) {
      fs::copy(src, dst, fs::copy_options::recursive);
    } else {
      fs::rename(src, dst);
    }
    return true;
  } catch (const fs::filesystem_error& ex) {
    errno = ex.code().value();
    return false;
  }
}

static bool shellOp(const QStringList& sourceNames, const QStringList& destinationNames,
                    QWidget* dialog, op operation, bool yesToAll)
{
  if ((sourceNames.size() != destinationNames.size() && destinationNames.size() != 1) ||
      (destinationNames.size() == 1 && !QFileInfo(destinationNames[0]).isDir())) {
    errno = EINVAL;
    return false;
  }

  std::vector<fs::path> sources;
  std::vector<fs::path> destinations;

  // allocate memory
  sources.reserve(sourceNames.size());
  destinations.reserve(sourceNames.size());

  // create sources
  for (const auto& sourceName : sourceNames) {
    sources.emplace_back(QFileInfo(sourceName).filesystemAbsoluteFilePath());
  }

  // create destinations
  if (destinationNames.size() > 1) {
    for (const auto& destinationName : destinationNames) {
      destinations.emplace_back(
          QFileInfo(destinationName).filesystemAbsoluteFilePath());
    }
  } else {
    fs::path dstDir = QFileInfo(destinationNames[0]).filesystemAbsoluteFilePath();

    for (const auto& sourceName : sourceNames) {
      destinations.emplace_back(dstDir / sourceName.toStdString());
    }
  }

  for (int i = 0; i < sourceNames.size(); ++i) {
    if (!doOperation(sources[i], destinations[i], dialog, operation, yesToAll)) {
      return false;
    }
  }

  return true;
}

bool shellCopy(const QStringList& sourceNames, const QStringList& destinationNames,
               QWidget* dialog)
{
  return shellOp(sourceNames, destinationNames, dialog, FO_COPY, false);
}

bool shellCopy(const QString& sourceNames, const QString& destinationNames,
               bool yesToAll, QWidget* dialog)
{
  return shellOp({sourceNames}, {destinationNames}, dialog, FO_COPY, yesToAll);
}

bool shellMove(const QStringList& sourceNames, const QStringList& destinationNames,
               QWidget* dialog)
{
  return shellOp(sourceNames, destinationNames, dialog, FO_MOVE, false);
}

bool shellMove(const QString& sourceNames, const QString& destinationNames,
               bool yesToAll, QWidget* dialog)
{
  return shellOp({sourceNames}, {destinationNames}, dialog, FO_MOVE, yesToAll);
}

bool shellRename(const QString& oldName, const QString& newName, bool yesToAll,
                 QWidget* dialog)
{
  return shellMove(oldName, newName, yesToAll, dialog);
}

bool shellDelete(const QStringList& fileNames, bool recycle, QWidget* dialog)
{
  (void)dialog;

  return std::ranges::all_of(fileNames, [recycle](const QString& fileName) {
    QFile file(fileName);
    if (recycle) {
      if (!file.moveToTrash()) {
        errno = fileErrorToErrno(file.error());
        return false;
      }
    } else {
      if (!file.remove()) {
        errno = fileErrorToErrno(file.error());
        return false;
      }
    }
    return true;
  });
}

namespace shell
{

  static QString g_urlHandler;

  Result OpenUrl(const QUrl& url)
  {
    bool result = QDesktopServices::openUrl(url);
    if (!result) {
      const int e = errno;
      return Result::makeFailure(e, strerror(e));
    }
    return Result::makeSuccess();
  }

  Result ExploreDirectory(const QFileInfo& info)
  {
    return Open(info.absolutePath());
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

  Result Open(const QString& path)
  {
    return OpenUrl(QUrl::fromLocalFile(path));
  }

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
      return OpenUrl(url);
    }
    return OpenCustomURL(g_urlHandler, url.toString(QUrl::FullyEncoded));
  }

  Result Execute(const QString& program, const QString& params)
  {
    return ExecuteIn(program, QDir::currentPath(), params);
  }

  Result ExecuteIn(const QString& program, const QString& workdir,
                   const QString& params)
  {
    if (!QFile::exists(workdir)) {
      return Result::makeFailure(ENOENT, u"Workdir does not exist"_s);
    }

    /*
    source: https://stackoverflow.com/a/3703179
    1. Before forking, open a
     * pipe in the parent process.
    2. After forking, the parent closes the writing
     * end of the pipe and reads from the
    reading end.
    3. The child closes the
     * reading end and sets the close-on-exec flag for the writing
    end.
    4. The
     * child calls exec.
    5. If exec fails, the child writes the error code back to
     * the parent using the pipe,
    then exits.
    6. The parent reads eof (a
     * zero-length read) if the child successfully performed
    exec, since
     * close-on-exec made successful exec close the writing end of the pipe.
    Or, if
     * exec failed, the parent reads the error code and can proceed accordingly.
 Either
     * way, the parent blocks until the child calls exec.
    7. The parent closes the
     * reading end of the pipe.
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

      QString command = '\"' % program % u"\" "_s % params;
      // store the temporary QByteArray object
      QByteArray utf8Data = command.toUtf8();

      if (chdir(workdir.toUtf8().constData()) != 0) {
        const int e = errno;
        log::warn("Could not change directory to '{}': ", workdir, strerror(e));
        exit(e);
      }
      execl("/bin/sh", "sh", "-c", utf8Data.constData(), nullptr);

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
      close(pipefd[0]);
      close(pipefd[1]);

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

  void SetUrlHandler(const QString& cmd)
  {
    g_urlHandler = cmd;
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

  Result Rename(const QFileInfo& src, const QFileInfo& dest, bool copyAllowed)
  {
    Q_UNUSED(copyAllowed);

    std::error_code ec;
    std::filesystem::rename(src.filesystemAbsoluteFilePath(),
                            dest.filesystemAbsoluteFilePath(), ec);
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

QIcon iconForExecutable(const QString& filePath)
{
#warning "STUB"
  (void)filePath;
  STUB();
  return QIcon(u":/MO/gui/executable"_s);
}
QString getFileVersion(QString const& filepath)
{
#warning "STUB"
  (void)filepath;
  STUB();
  return "";
}

QString getProductVersion(QString const& filepath)
{
#warning "STUB"
  (void)filepath;
  STUB();
  return "";
}

std::string formatSystemMessage(int id)
{
  return strerror(id);
}

}  // namespace MOBase
