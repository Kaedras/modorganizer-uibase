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
#include <QCollator>
#include <QDir>
#include <QImage>
#include <QScreen>
#include <QStringEncoder>
#include <QUuid>
#include <QtDebug>
#include <memory>
#include <sstream>

#ifdef __unix__
#include "linux/compatibility.h"
#endif

namespace MOBase
{

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
        file.setPermissions(file.permissions() | QFileDevice::WriteUser);
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

  DWORD Result::error()
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

  // definitions are located in os-specific source files
  extern Result ExploreDirectory(const QFileInfo&);
  extern Result ExploreFileInDirectory(const QFileInfo&);

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

  Result Rename(const QFileInfo& src, const QFileInfo& dest)
  {
    return Rename(src, dest, true);
  }

}  // namespace shell

bool moveFileRecursive(const QString& source, const QString& baseDir,
                       const QString& destination)
{
  QStringList pathComponents = destination.split("/");
  QString path               = baseDir;
  for (QStringList::Iterator iter = pathComponents.begin();
       iter != pathComponents.end() - 1; ++iter) {
    path.append("/").append(*iter);
    if (!QDir(path).exists() && !QDir().mkdir(path)) {
      reportError(QObject::tr("failed to create directory \"%1\"").arg(path));
      return false;
    }
  }

  QString destinationAbsolute = baseDir.mid(0).append("/").append(destination);
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
  QStringList pathComponents = destination.split("/");
  QString path               = baseDir;
  for (QStringList::Iterator iter = pathComponents.begin();
       iter != pathComponents.end() - 1; ++iter) {
    path.append("/").append(*iter);
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

QString ToString(const SYSTEMTIME& time)
{
  QDate d(time.wYear, time.wMonth, time.wDay);
  QTime t(time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);

  QDateTime dt(d, t);

  return dt.toString(QLocale::system().dateFormat());
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

QString getOptionalKnownFolder(QStandardPaths::StandardLocation location)
{
  QStringList locations = QStandardPaths::standardLocations(location);
  if (locations.isEmpty()) {
    return {};
  }

  return locations.first();
}

QDir getKnownFolder(QStandardPaths::StandardLocation location, const QString& what)
{
  QStringList locations = QStandardPaths::standardLocations(location);

  if (locations.isEmpty()) {
    log::error("failed to get known folder '{}'",
               what.isEmpty() ? QString::number(location) : what);

    throw std::runtime_error("couldn't get known folder path");
  }

  return locations.first();
}

QString getDesktopDirectory()
{
  return getKnownFolder(QStandardPaths::DesktopLocation, "desktop").absolutePath();
}

QString getStartMenuDirectory()
{
  return getKnownFolder(QStandardPaths::ApplicationsLocation, "start menu")
      .absolutePath();
}

bool shellDeleteQuiet(const QString& fileName, QWidget* dialog)
{
  if (!QFile::remove(fileName)) {
    return shellDelete(QStringList(fileName), false, dialog);
  }
  return true;
}

QString readFileText(const QString& fileName, QString* encoding, bool* hadBOM)
{

  QFile textFile(fileName);
  if (!textFile.open(QIODevice::ReadOnly)) {
    return QString();
  }

  QByteArray buffer = textFile.readAll();
  return decodeTextData(buffer, encoding, hadBOM);
}

QString decodeTextData(const QByteArray& fileData, QString* encoding, bool* hadBOM)
{
  QStringConverter::Encoding codec = QStringConverter::Encoding::Utf8;
  QStringEncoder encoder(codec);
  QStringDecoder decoder(codec, QStringConverter::Flag::ConvertInitialBom);
  QString text = decoder.decode(fileData);

  // embedded nulls probably mean it was UTF-16 - they're rare/illegal in text files
  bool hasEmbeddedNulls = false;
  for (const auto& character : text) {
    if (character.isNull()) {
      hasEmbeddedNulls = true;
      break;
    }
  }

  // check reverse conversion. If this was unicode text there can't be data loss
  // this assumes QString doesn't normalize the data in any way so this is a bit unsafe
  if (hasEmbeddedNulls || encoder.encode(text) != fileData) {
    log::debug("conversion failed assuming local encoding");
    auto codecSearch = QStringConverter::encodingForData(fileData);
    if (codecSearch.has_value()) {
      codec   = codecSearch.value();
      decoder = QStringDecoder(codec, QStringConverter::Flag::ConvertInitialBom);
    } else {
      // encodingForData doesn't handle UTF-16 without BOM
      decoder = QStringDecoder(hasEmbeddedNulls ? QStringConverter::Encoding::Utf16
                                                : QStringConverter::Encoding::System);
    }
    text = decoder.decode(fileData);
  }

  if (encoding != nullptr) {
    *encoding = QStringConverter::nameForEncoding(codec);
  }

  if (!text.isEmpty() && text.startsWith(QChar::ByteOrderMark)) {
    text.remove(0, 1);

    if (hadBOM != nullptr) {
      *hadBOM = true;
    }
  } else if (hadBOM != nullptr) {
    *hadBOM = false;
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

    if (!shellDelete(deleteFiles)) {
      const auto e = ::GetLastError();
      log::warn("failed to remove log files: {}", formatSystemMessage(e));
    }
  }
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

QString windowsErrorString(DWORD errorCode)
{
  return ToQString(formatSystemMessage(errorCode));
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
    Result.append(QString("%1:").arg(buffer));
  }

  if (minutes > 0 || hours > 0) {
    if (minutes < 10 && hours > 0)
      sprintf_s(buffer, "0%lld", minutes);
    else
      sprintf_s(buffer, "%lld", minutes);
    Result.append(QString("%1:").arg(buffer));
  }

  if (seconds < 10 && (minutes > 0 || hours > 0))
    sprintf_s(buffer, "0%lld", seconds);
  else
    sprintf_s(buffer, "%lld", seconds);
  Result.append(QString("%1").arg(buffer));

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
