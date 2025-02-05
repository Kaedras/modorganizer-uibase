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
#include <QCollator>
#include <QDir>
#include <QImage>
#include <QScreen>
#include <QStringEncoder>
#include <QUuid>
#include <QtDebug>
#include <memory>
#include <sstream>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <format>

namespace MOBase
{

namespace shell
{

  Result ShellExecuteWrapper(const QString& operation, const QString& file,
                             const QString& params);

  Result ExploreFileInDirectory(const QFileInfo& info)
  {
    const QString path = QDir::toNativeSeparators(info.absoluteFilePath());
    QString params     = QString(R"(/select,"%1")").arg(path);

    return ShellExecuteWrapper(nullptr, "explorer", params);
  }

}  // namespace shell

QString ToString(const SYSTEMTIME& time)
{
  char dateBuffer[100];
  char timeBuffer[100];
  int size = 100;
  GetDateFormatA(LOCALE_USER_DEFAULT, LOCALE_USE_CP_ACP, &time, nullptr, dateBuffer,
                 size);
  GetTimeFormatA(LOCALE_USER_DEFAULT, LOCALE_USE_CP_ACP, &time, nullptr, timeBuffer,
                 size);
  return QString::fromLocal8Bit(dateBuffer) + " " + QString::fromLocal8Bit(timeBuffer);
}

std::wstring formatMessage(DWORD id, const std::wstring& message)
{
  std::wstring s;

  std::wostringstream oss;
  oss << L"0x" << std::hex << id;

  if (message.empty()) {
    s = oss.str();
  } else {
    s += message + L" (" + oss.str() + L")";
  }

  return s;
}

void trimWString(std::wstring& s)
{
  s.erase(std::remove_if(s.begin(), s.end(),
                         [](wint_t ch) {
                           return std::iswspace(ch);
                         }),
          s.end());
}

std::wstring getMessage(DWORD id, HMODULE mod)
{
  wchar_t* message = nullptr;

  DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS;

  void* source = nullptr;

  if (mod != 0) {
    flags |= FORMAT_MESSAGE_FROM_HMODULE;
    source = mod;
  }

  const auto ret =
      FormatMessageW(flags, source, id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                     reinterpret_cast<LPWSTR>(&message), 0, NULL);

  std::wstring s;

  if (ret != 0 && message) {
    s = message;
    trimWString(s);
    LocalFree(message);
  }

  return s;
}

std::wstring formatSystemMessage(DWORD id)
{
  return formatMessage(id, getMessage(id, 0));
}

std::wstring formatNtMessage(NTSTATUS s)
{
  const DWORD id = static_cast<DWORD>(s);
  return formatMessage(id, getMessage(id, ::GetModuleHandleW(L"ntdll.dll")));
}

QString windowsErrorString(DWORD errorCode)
{
  return QString::fromStdWString(formatSystemMessage(errorCode));
}

struct CoTaskMemFreer
{
  void operator()(void* p) { ::CoTaskMemFree(p); }
};

template <class T>
using COMMemPtr = std::unique_ptr<T, CoTaskMemFreer>;

QString getOptionalKnownFolder(KNOWNFOLDERID id)
{
  COMMemPtr<wchar_t> path;

  {
    wchar_t* rawPath = nullptr;
    HRESULT res      = SHGetKnownFolderPath(id, 0, nullptr, &rawPath);

    if (FAILED(res)) {
      return {};
    }

    path.reset(rawPath);
  }

  return QString::fromWCharArray(path.get());
}

QString getOptionalKnownFolder(KNOWNFOLDERID id)
{
  COMMemPtr<wchar_t> path;

  {
    wchar_t* rawPath = nullptr;
    HRESULT res      = SHGetKnownFolderPath(id, 0, nullptr, &rawPath);

    if (FAILED(res)) {
      return {};
    }

    path.reset(rawPath);
  }

  return QString::fromWCharArray(path.get());
}

QDir getKnownFolder(KNOWNFOLDERID id, const QString& what)
{
  COMMemPtr<wchar_t> path;

  {
    wchar_t* rawPath = nullptr;
    HRESULT res      = SHGetKnownFolderPath(id, 0, nullptr, &rawPath);

    if (FAILED(res)) {
      log::error("failed to get known folder '{}', {}",
                 what.isEmpty() ? QUuid(id).toString() : what,
                 formatSystemMessage(res));

      throw std::runtime_error("couldn't get known folder path");
    }

    path.reset(rawPath);
  }

  return QString::fromWCharArray(path.get());
}

}  // namespace MOBase
