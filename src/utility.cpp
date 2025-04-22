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
#include "moassert.h"
#include "peextractor.h"
#include "report.h"
#include <QApplication>
#include <QCollator>
#include <QDesktopServices>
#include <QDir>
#include <QErrorMessage>
#include <QImage>
#include <QNtfsPermissionCheckGuard>
#include <QProcess>
#include <QStandardPaths>
#include <QStringEncoder>

#ifdef __unix__
#define QNtfsPermissionCheckGuard [[maybe_unused]] void*
#endif

using namespace Qt::StringLiterals;

namespace MOBase
{

// forward declarations
namespace shell
{
  extern Result ExploreFileInDirectory(const QFileInfo& info);
  extern QString toUNC(const QFileInfo& path);
}  // namespace shell

QString fileErrorToString(QFileDevice::FileError error)
{
  switch (error) {

  case QFileDevice::NoError:
    return QStringLiteral("No error occurred.");
  case QFileDevice::ReadError:
    return QStringLiteral("An error occurred when reading from the file.");
  case QFileDevice::WriteError:
    return QStringLiteral("An error occurred when writing to the file.");
  case QFileDevice::FatalError:
    return QStringLiteral("A fatal error occurred.");
  case QFileDevice::ResourceError:
    return QStringLiteral(
        "Out of resources (e.g., too many open files, out of memory, etc.)");
  case QFileDevice::OpenError:
    return QStringLiteral("The file could not be opened.");
  case QFileDevice::AbortError:
    return QStringLiteral("The operation was aborted.");
  case QFileDevice::TimeOutError:
    return QStringLiteral("A timeout occurred.");
  case QFileDevice::RemoveError:
    return QStringLiteral("The file could not be removed.");
  case QFileDevice::RenameError:
    return QStringLiteral("The file could not be renamed.");
  case QFileDevice::PositionError:
    return QStringLiteral("The position in the file could not be changed.");
  case QFileDevice::ResizeError:
    return QStringLiteral("The file could not be resized.");
  case QFileDevice::PermissionsError:
    return QStringLiteral("The file could not be accessed.");
  case QFileDevice::CopyError:
    return QStringLiteral("The file could not be copied.");
  case QFileDevice::UnspecifiedError:
  default:
    return QStringLiteral("An unspecified error occurred.");
  }
}

bool removeDir(const QString& dirName)
{
  QDir dir(dirName);

  if (dir.exists()) {
    Q_FOREACH (QFileInfo info,
               dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden |
                                     QDir::AllDirs | QDir::Files,
                                 QDir::DirsFirst)) {
      if (info.isDir()) {
        if (!removeDir(info.absoluteFilePath())) {
          return false;
        }
      } else {
        QFile file(info.absoluteFilePath());
        file.setPermissions(QFileDevice::WriteOwner | QFileDevice::WriteUser);
        if (!file.remove()) {
          reportError(QObject::tr("removal of \"%1\" failed: %2")
                          .arg(info.absoluteFilePath())
                          .arg(file.errorString()));
          return false;
        }
      }
    }

    if (!dir.rmdir(dirName)) {
      reportError(QObject::tr("removal of \"%1\" failed").arg(dir.absolutePath()));
      return false;
    }
  } else {
    reportError(QObject::tr("\"%1\" doesn't exist (remove)").arg(dirName));
    return false;
  }

  return true;
}

bool copyDir(const QString& sourceName, const QString& destinationName, bool merge)
{
  QDir sourceDir(sourceName);
  if (!sourceDir.exists()) {
    return false;
  }
  QDir destDir(destinationName);
  if (!destDir.exists()) {
    destDir.mkdir(destinationName);
  } else if (!merge) {
    return false;
  }

  QStringList files = sourceDir.entryList(QDir::Files);
  foreach (QString fileName, files) {
    QString srcName  = sourceName + "/" + fileName;
    QString destName = destinationName + "/" + fileName;
    QFile::copy(srcName, destName);
  }

  files.clear();
  // we leave out symlinks because that could cause an endless recursion
  QStringList subDirs =
      sourceDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
  foreach (QString subDir, subDirs) {
    QString srcName  = sourceName + "/" + subDir;
    QString destName = destinationName + "/" + subDir;
    copyDir(srcName, destName, merge);
  }
  return true;
}

namespace shell
{

  Result::Result(bool success, DWORD error, QString message, HANDLE process)
      : m_success(success), m_error(error), m_message(std::move(message)),
        m_process(process)
  {
    if (m_message.isEmpty()) {
      m_message = ToQString(formatSystemMessage(m_error));
    }
  }

  Result Result::makeFailure(DWORD error, QString message)
  {
    return Result(false, error, std::move(message), INVALID_HANDLE_VALUE);
  }

  Result Result::makeSuccess(HANDLE process)
  {
    return Result(true, ERROR_SUCCESS, {}, process);
  }

  bool Result::success() const
  {
    return m_success;
  }

  Result::operator bool() const
  {
    return m_success;
  }

  DWORD Result::error() const
  {
    return m_error;
  }

  const QString& Result::message() const
  {
    return m_message;
  }

  HANDLE Result::processHandle() const
  {
    return m_process.get();
  }

  HANDLE Result::stealProcessHandle()
  {
    const auto h = m_process.release();
    m_process.reset(INVALID_HANDLE_VALUE);
    return h;
  }

  QString Result::toString() const
  {
    if (m_message.isEmpty()) {
      return QObject::tr("Error %1").arg(m_error);
    } else {
      return m_message;
    }
  }

  // check if file exists and is readable
  int CheckFile(const QFileInfo& info)
  {
    [[maybe_unused]] QNtfsPermissionCheckGuard permissionGuard;
    if (!info.exists()) {
      return ERROR_PATH_NOT_FOUND;
    }
    if (!info.isReadable()) {
      // If the NTFS permissions check has not been enabled,
      // the result on Windows will merely reflect whether the entry exists.
      return ERROR_ACCESS_DENIED;
    }

    return 0;
  }
  int CheckFile(const QString& path)
  {
    return CheckFile(QFileInfo(path));
  }

  Result ExploreDirectory(const QFileInfo& info)
  {
    auto check = CheckFile(info);
    if (check != 0) {
      return Result::makeFailure(check, formatError(check));
    }
    auto result = QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
    if (!result) {
      const auto e = GetLastError();
      return Result::makeFailure(e, formatError(e));
    }
    return Result::makeSuccess();
  }

  Result Explore(const QFileInfo& info)
  {
    if (info.isFile()) {
      return ExploreFileInDirectory(info);
    } else if (info.isDir()) {
      return ExploreDirectory(info);
    } else {
      // try the parent directory
      const auto parent = info.dir();

      if (parent.exists()) {
        return ExploreDirectory(QFileInfo(parent.absolutePath()));
      } else {
        return Result::makeFailure(ERROR_FILE_NOT_FOUND);
      }
    }
  }

  Result Explore(const QString& path)
  {
    return Explore(QFileInfo(path));
  }

  Result Explore(const QDir& dir)
  {
    return Explore(QFileInfo(dir.absolutePath()));
  }

  Result Open(const QString& path)
  {
    auto check = CheckFile(path);
    if (check != 0) {
      return Result::makeFailure(check, formatError(check));
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
      const auto e = GetLastError();
      return Result::makeFailure(e, formatError(e));
    }
    return Result::makeSuccess();
  }

  Result OpenURL(const QUrl& url)
  {
    if (!url.isValid()) {
      return Result::makeFailure(ERROR_BAD_ARGUMENTS, url.errorString());
    }
    if (!QDesktopServices::openUrl(url)) {
      const auto e = GetLastError();
      return Result::makeFailure(e, formatError(e));
    }
    return Result::makeSuccess();
  }

  Result Delete(const QFileInfo& path)
  {
    QFile file(toUNC(path));

    if (!file.remove()) {
      return Result::makeFailure(file.error(), file.errorString());
    }
    return Result::makeSuccess();
  }

  Result Rename(const QFileInfo& src, const QFileInfo& dest)
  {
    QFile source(toUNC(src));
    QFile destination(toUNC(dest));

    if (!source.rename(destination.fileName())) {
      return Result::makeFailure(source.error(), source.errorString());
    }

    return Result::makeSuccess();
  }

  Result Rename(const QFileInfo& src, const QFileInfo& dest, bool copyAllowed)
  {
    return Rename(src, dest);
  }

  Result CreateDirectories(const QDir& dir)
  {
    if (!dir.mkpath(u"."_s)) {
      const auto e = GetLastError();
      return Result::makeFailure(e, ToQString(formatSystemMessage(e)));
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

bool moveFileRecursive(const QString& source, const QString& baseDir,
                       const QString& destination)
{
  QStringList pathComponents = destination.split('/');
  QString path               = baseDir;
  for (QStringList::Iterator iter = pathComponents.begin();
       iter != pathComponents.end() - 1; ++iter) {
    path.append('/' % *iter);
    if (!QDir(path).exists() && !QDir().mkdir(path)) {
      reportError(QObject::tr("failed to create directory \"%1\"").arg(path));
      return false;
    }
  }

  QString destinationAbsolute = baseDir.mid(0).append('/' % destination);
  if (!QFile::rename(source, destinationAbsolute)) {
    // move failed, try copy & delete
    if (!QFile::copy(source, destinationAbsolute)) {
      reportError(QObject::tr("failed to copy \"%1\" to \"%2\"")
                      .arg(source)
                      .arg(destinationAbsolute));
      return false;
    } else {
      QFile::remove(source);
    }
  }
  return true;
}

bool copyFileRecursive(const QString& source, const QString& baseDir,
                       const QString& destination)
{
  QStringList pathComponents = destination.split('/');
  QString path               = baseDir;
  for (QStringList::Iterator iter = pathComponents.begin();
       iter != pathComponents.end() - 1; ++iter) {
    path.append('/' % *iter);
    if (!QDir(path).exists() && !QDir().mkdir(path)) {
      reportError(QObject::tr("failed to create directory \"%1\"").arg(path));
      return false;
    }
  }

  QString destinationAbsolute = baseDir.mid(0).append("/").append(destination);
  if (!QFile::copy(source, destinationAbsolute)) {
    reportError(QObject::tr("failed to copy \"%1\" to \"%2\"")
                    .arg(source)
                    .arg(destinationAbsolute));
    return false;
  }
  return true;
}

int promptUserForOverwrite(const QString& file, QWidget* dialog = nullptr)
{
  QMessageBox msgBox;
  msgBox.setText(u"Target file already exists"_s);
  msgBox.setDetailedText(
      QStringLiteral("\"%1\" already exists. Would you like to overwrite it?")
          .arg(file));
  msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No |
                            QMessageBox::Cancel);
  msgBox.setDefaultButton(QMessageBox::Cancel);
  msgBox.setParent(dialog);

  return msgBox.exec();
}

bool shellDeleteQuiet(const QString& fileName, QWidget* dialog)
{
  if (!QFile::remove(fileName)) {
    return shellDelete(QStringList(fileName), false, dialog);
  }
  return true;
}

std::wstring ToWString(const QString& source)
{
  return source.toStdWString();
}

std::string ToString(const QString& source, bool utf8)
{
  QByteArray array8bit;
  if (utf8) {
    array8bit = source.toUtf8();
  } else {
    array8bit = source.toLocal8Bit();
  }
  return std::string(array8bit.constData());
}

QString ToQString(const std::string& source)
{
  // return QString::fromUtf8(source.c_str());
  return QString::fromStdString(source);
}

QString ToQString(const std::wstring& source)
{
  // return QString::fromWCharArray(source.c_str());
  return QString::fromStdWString(source);
}

static int naturalCompareI(const QString& a, const QString& b)
{
  static QCollator c = [] {
    QCollator temp;
    temp.setNumericMode(true);
    temp.setCaseSensitivity(Qt::CaseInsensitive);
    return temp;
  }();

  return c.compare(a, b);
}

int naturalCompare(const QString& a, const QString& b, Qt::CaseSensitivity cs)
{
  if (cs == Qt::CaseInsensitive) {
    return naturalCompareI(a, b);
  }

  static QCollator c = [] {
    QCollator temp;
    temp.setNumericMode(true);
    return temp;
  }();

  return c.compare(a, b);
}

QDir getKnownFolder(QStandardPaths::StandardLocation location)
{
  auto paths = QStandardPaths::standardLocations(location);
  if (paths.empty()) {
    throw std::runtime_error("couldn't get known folder path");
  }

  return paths.first();
}

QString getOptionalKnownFolder(QStandardPaths::StandardLocation location)
{
  auto paths = QStandardPaths::standardLocations(location);
  if (paths.empty()) {
    return {};
  }

  return paths.first();
}

QString getDesktopDirectory()
{
  return QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first();
}

QString getStartMenuDirectory()
{
  return QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)
      .first();
}

QString readFileText(const QString& fileName, QString* encoding)
{

  QFile textFile(fileName);
  if (!textFile.open(QIODevice::ReadOnly)) {
    return QString();
  }

  QByteArray buffer = textFile.readAll();
  return decodeTextData(buffer, encoding);
}

QString decodeTextData(const QByteArray& fileData, QString* encoding)
{
  QStringConverter::Encoding codec = QStringConverter::Encoding::Utf8;
  QStringEncoder encoder(codec);
  QStringDecoder decoder(codec);
  QString text = decoder.decode(fileData);

  // check reverse conversion. If this was unicode text there can't be data loss
  // this assumes QString doesn't normalize the data in any way so this is a bit unsafe
  if (encoder.encode(text) != fileData) {
    log::debug("conversion failed assuming local encoding");
    auto codecSearch = QStringConverter::encodingForData(fileData);
    if (codecSearch.has_value()) {
      codec   = codecSearch.value();
      decoder = QStringDecoder(codec);
    } else {
      decoder = QStringDecoder(QStringConverter::Encoding::System);
    }
    text = decoder.decode(fileData);
  }

  if (encoding != nullptr) {
    *encoding = QStringConverter::nameForEncoding(codec);
  }

  return text;
}

void removeOldFiles(const QString& path, const QString& pattern, int numToKeep,
                    QDir::SortFlags sorting)
{
  QFileInfoList files =
      QDir(path).entryInfoList(QStringList(pattern), QDir::Files, sorting);

  if (files.count() > numToKeep) {
    QStringList deleteFiles;
    for (int i = 0; i < files.count() - numToKeep; ++i) {
      deleteFiles.append(files.at(i).absoluteFilePath());
    }

    auto result = shellDelete(deleteFiles);

    if (!result) {
      const int error = errno;
      log::warn("failed to remove log files: {}", strerror(error));
    }
  }
}

QIcon iconForExecutable(const QString& filepath)
{
  QFile exeFile(filepath);
  QBuffer buffer;

  if (!exeFile.open(QIODeviceBase::ReadOnly) ||
      !buffer.open(QIODeviceBase::ReadWrite)) {
    return QIcon(QStringLiteral(":/MO/gui/executable"));
  }

  if (!PeExtractor::loadIconData(&exeFile, &buffer)) {
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
  QBuffer buffer;

  if (!exeFile.open(QIODeviceBase::ReadOnly) ||
      !buffer.open(QIODeviceBase::ReadWrite)) {
    return {};
  }

  if (!PeExtractor::loadVersionData(&exeFile, &buffer)) {
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

void deleteChildWidgets(QWidget* w)
{
  auto* ly = w->layout();
  if (!ly) {
    return;
  }

  while (auto* item = ly->takeAt(0)) {
    delete item->widget();
    delete item;
  }
}

QString formatMessage(DWORD id, const QString& message)
{
  QString s = QStringLiteral("0x%1").arg(id, 0, 16);

  if (message.isEmpty()) {
    return s;
  }

  return QStringLiteral("%1 (%2)").arg(message, s);
}

QString localizedSize(unsigned long long bytes, const QString& B, const QString& KB,
                      const QString& MB, const QString& GB, const QString& TB)
{
  constexpr unsigned long long OneKB = 1024ull;
  constexpr unsigned long long OneMB = 1024ull * 1024;
  constexpr unsigned long long OneGB = 1024ull * 1024 * 1024;
  constexpr unsigned long long OneTB = 1024ull * 1024 * 1024 * 1024;

  auto makeNum = [&](int factor) {
    const double n = static_cast<double>(bytes) / std::pow(1024.0, factor);

    // avoids rounding something like "1.999" to "2.00 KB"
    const double truncated =
        static_cast<double>(static_cast<unsigned long long>(n * 100)) / 100.0;

    return QString().setNum(truncated, 'f', 2);
  };

  if (bytes < OneKB) {
    return B.arg(bytes);
  } else if (bytes < OneMB) {
    return KB.arg(makeNum(1));
  } else if (bytes < OneGB) {
    return MB.arg(makeNum(2));
  } else if (bytes < OneTB) {
    return GB.arg(makeNum(3));
  } else {
    return TB.arg(makeNum(4));
  }
}

QDLLEXPORT QString localizedByteSize(unsigned long long bytes)
{
  return localizedSize(bytes, QObject::tr("%1 B"), QObject::tr("%1 KB"),
                       QObject::tr("%1 MB"), QObject::tr("%1 GB"),
                       QObject::tr("%1 TB"));
}

QDLLEXPORT QString localizedByteSpeed(unsigned long long bps)
{
  return localizedSize(bps, QObject::tr("%1 B/s"), QObject::tr("%1 KB/s"),
                       QObject::tr("%1 MB/s"), QObject::tr("%1 GB/s"),
                       QObject::tr("%1 TB/s"));
}

QDLLEXPORT QString localizedTimeRemaining(unsigned int remaining)
{
  QString Result;
  double interval;
  qint64 intval;

  // Hours
  interval = 60.0 * 60.0 * 1000.0;
  intval   = (qint64)trunc((double)remaining / interval);
  if (intval < 0)
    intval = 0;
  remaining -= static_cast<unsigned int>(trunc((double)intval * interval));
  qint64 hours = intval;

  // Minutes
  interval = 60.0 * 1000.0;
  intval   = (qint64)trunc((double)remaining / interval);
  if (intval < 0)
    intval = 0;
  remaining -= static_cast<unsigned int>(trunc((double)intval * interval));
  qint64 minutes = intval;

  // Seconds
  interval = 1000.0;
  intval   = (qint64)trunc((double)remaining / interval);
  if (intval < 0)
    intval = 0;
  remaining -= static_cast<unsigned int>(trunc((double)intval * interval));
  qint64 seconds = intval;

  // Whatever is left over is milliseconds

  char buffer[25];
  memset(buffer, 0, 25);

  if (hours > 0) {
    if (hours < 10)
      sprintf_s(buffer, "0%lld", hours);
    else
      sprintf_s(buffer, "%lld", hours);
    Result.append(QStringLiteral("%1:").arg(buffer));
  }

  if (minutes > 0 || hours > 0) {
    if (minutes < 10 && hours > 0)
      sprintf_s(buffer, "0%lld", minutes);
    else
      sprintf_s(buffer, "%lld", minutes);
    Result.append(QStringLiteral("%1:").arg(buffer));
  }

  if (seconds < 10 && (minutes > 0 || hours > 0))
    sprintf_s(buffer, "0%lld", seconds);
  else
    sprintf_s(buffer, "%lld", seconds);
  Result.append(QStringLiteral("%1").arg(buffer));

  if (hours > 0)
    //: Time remaining hours
    Result.append(QApplication::translate("uibase", "h"));
  else if (minutes > 0)
    //: Time remaining minutes
    Result.append(QApplication::translate("uibase", "m"));
  else
    //: Time remaining seconds
    Result.append(QApplication::translate("uibase", "s"));

  return Result;
}

QDLLEXPORT void localizedByteSizeTests()
{
  auto f = [](unsigned long long n) {
    return localizedByteSize(n).toStdString();
  };

#define CHECK_EQ(a, b)                                                                 \
  if ((a) != (b)) {                                                                    \
    std::cerr << "failed: " << a << " == " << b << "\n";                               \
    DebugBreak();                                                                      \
  }

  CHECK_EQ(f(0), "0 B");
  CHECK_EQ(f(1), "1 B");
  CHECK_EQ(f(999), "999 B");
  CHECK_EQ(f(1000), "1000 B");
  CHECK_EQ(f(1023), "1023 B");

  CHECK_EQ(f(1024), "1.00 KB");
  CHECK_EQ(f(2047), "1.99 KB");
  CHECK_EQ(f(2048), "2.00 KB");
  CHECK_EQ(f(1048575), "1023.99 KB");

  CHECK_EQ(f(1048576), "1.00 MB");
  CHECK_EQ(f(1073741823), "1023.99 MB");

  CHECK_EQ(f(1073741824), "1.00 GB");
  CHECK_EQ(f(1099511627775), "1023.99 GB");

  CHECK_EQ(f(1099511627776), "1.00 TB");
  CHECK_EQ(f(2759774185818), "2.51 TB");

#undef CHECK_EQ
}

TimeThis::TimeThis(const QString& what) : m_running(false)
{
  start(what);
}

TimeThis::~TimeThis()
{
  stop();
}

void TimeThis::start(const QString& what)
{
  stop();

  m_what    = what;
  m_start   = Clock::now();
  m_running = true;
}

void TimeThis::stop()
{
  using namespace std::chrono;

  if (!m_running) {
    return;
  }

  const auto end = Clock::now();
  const auto d   = duration_cast<milliseconds>(end - m_start).count();

  if (m_what.isEmpty()) {
    log::debug("timing: {} ms", d);
  } else {
    log::debug("timing: {} {} ms", m_what, d);
  }

  m_running = false;
}

}  // namespace MOBase
