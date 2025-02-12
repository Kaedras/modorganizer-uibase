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

using namespace Qt::Literals::StringLiterals;

namespace MOBase
{

namespace shell
{

  static QString g_urlHandler;

  void SetUrlHandler(const QString& cmd)
  {
    g_urlHandler = cmd;
  }

  extern Result OpenURL(const QUrl& url);

  QString formatError(int i)
  {
    switch (i) {
    case 0:
      return u"The operating system is out of memory or resources"_s;

    case ERROR_FILE_NOT_FOUND:
      return u"The specified file was not found"_s;

    case ERROR_PATH_NOT_FOUND:
      return u"The specified path was not found"_s;

    case ERROR_BAD_FORMAT:
      return u"The .exe file is invalid (non-Win32 .exe or error in .exe image)"_s;

    case SE_ERR_ACCESSDENIED:
      return u"The operating system denied access to the specified file"_s;

    case SE_ERR_ASSOCINCOMPLETE:
      return u"The file name association is incomplete or invalid"_s;

    case SE_ERR_DDEBUSY:
      return u"The DDE transaction could not be completed because other DDE "
             "transactions were being processed"_s;

    case SE_ERR_DDEFAIL:
      return u"The DDE transaction failed"_s;

    case SE_ERR_DDETIMEOUT:
      return u"The DDE transaction could not be completed because the request "
             "timed out"_s;

    case SE_ERR_DLLNOTFOUND:
      return u"The specified DLL was not found"_s;

    case SE_ERR_NOASSOC:
      return u"There is no application associated with the given file name "
             "extension"_s;

    case SE_ERR_OOM:
      return u"There was not enough memory to complete the operation"_s;

    case SE_ERR_SHARE:
      return u"A sharing violation occurred"_s;

    default:
      return QString(u"Unknown error %1"_s).arg(i);
    }
  }

  void LogShellFailure(const wchar_t* operation, const wchar_t* file,
                       const wchar_t* params, DWORD error)
  {
    QStringList s;

    if (operation) {
      s << QString::fromWCharArray(operation);
    }

    if (file) {
      s << QString::fromWCharArray(file);
    }

    if (params) {
      s << QString::fromWCharArray(params);
    }

    log::error("failed to invoke '{}': {}", s.join(" "), formatSystemMessage(error));
  }

  Result ShellExecuteWrapper(const wchar_t* operation, const wchar_t* file,
                             const wchar_t* params)
  {
    SHELLEXECUTEINFOW info = {};

    info.cbSize       = sizeof(info);
    info.fMask        = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb       = operation;
    info.lpFile       = file;
    info.lpParameters = params;
    info.nShow        = SW_SHOWNORMAL;

    const auto r = ::ShellExecuteExW(&info);

    if (!r) {
      const auto e = ::GetLastError();
      LogShellFailure(operation, file, params, e);

      return Result::makeFailure(e, QString::fromStdWString(formatSystemMessage(e)));
    }

    const HANDLE process = info.hProcess ? info.hProcess : INVALID_HANDLE_VALUE;
    return Result::makeSuccess(process);
  }

  Result ExploreFileInDirectory(const QFileInfo& info)
  {
    const auto path      = QDir::toNativeSeparators(info.absoluteFilePath());
    const auto params    = "/select,\"" + path + "\"";
    const auto ws_params = params.toStdWString();

    return ShellExecuteWrapper(nullptr, L"explorer", ws_params.c_str());
  }

  Result OpenCustomURL(const std::wstring& format, const std::wstring& url)
  {
    log::debug("custom url handler: '{}'", format);

    // arguments, the first one is the url, the next 98 are empty strings because
    // FormatMessage() doesn't have a way of saying how many arguments are
    // available in the array, so this avoids a crash if there's something like
    // %2 in the format string
    const std::size_t args_count = 99;
    DWORD_PTR args[args_count];
    args[0] = reinterpret_cast<DWORD_PTR>(url.c_str());

    for (std::size_t i = 1; i < args_count; ++i) {
      args[i] = reinterpret_cast<DWORD_PTR>(L"");
    }

    wchar_t* output = nullptr;

    // formatting
    const auto n =
        ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                             FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_FROM_STRING,
                         format.c_str(), 0, 0, reinterpret_cast<LPWSTR>(&output), 0,
                         reinterpret_cast<va_list*>(args));

    if (n == 0) {
      const auto e = GetLastError();

      log::error("failed to format browser command '{}'", format);
      log::error("{}", formatSystemMessage(e));
      log::error(
          "{}",
          QObject::tr("You have an invalid custom browser command in the settings."));

      return Result::makeFailure(e);
    }

    const std::wstring cmd(output, n);
    ::LocalFree(output);

    log::debug("running '{}'", cmd);

    // creating process
    STARTUPINFO si         = {.cb = sizeof(STARTUPINFO)};
    PROCESS_INFORMATION pi = {};

    const auto r = ::CreateProcessW(nullptr, const_cast<wchar_t*>(cmd.c_str()), nullptr,
                                    nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);

    if (r == 0) {
      const auto e = GetLastError();
      log::error("failed to run '{}'", cmd);
      log::error("{}", formatSystemMessage(e));
      log::error(
          "{}",
          QObject::tr("You have an invalid custom browser command in the settings."));
      return Result::makeFailure(e);
    }

    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);

    return Result::makeSuccess();
  }

  Result Open(const QUrl& url)
  {
    log::debug("opening url '{}'", url.toString());

    const auto ws_url = url.toString().toStdWString();

    if (g_urlHandler.isEmpty()) {
      return OpenURL(url);
    } else {
      return OpenCustomURL(g_urlHandler.toStdWString(), ws_url);
    }
  }

  HANDLE GetHandleFromPid(qint64 pid)
  {
    auto result = OpenProcess(PROCESS_ALL_ACCESS, false, (DWORD)pid);
    return result ? result : INVALID_HANDLE_VALUE;
  }

  QString toUNC(const QFileInfo& path)
  {
    auto wpath = QDir::toNativeSeparators(path.absoluteFilePath()).toStdWString();
    if (!wpath.starts_with(L"\\\\?\\")) {
      wpath = L"\\\\?\\" + wpath;
    }

    return QString::fromStdWString(wpath);
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
