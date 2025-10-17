/*
This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "registry.h"
#include "log.h"
#include "report.h"
#include <QApplication>
#include <QMessageBox>
#include <QString>
#include <fstream>
#include <inipp.h>
#ifdef __unix__
#include <linux/compatibility.h>
#endif

namespace
{

template <typename T, typename... ValidTypes>
constexpr bool is_one_of()
{
  return (std::is_same_v<T, ValidTypes> || ...);
}

// helper function that mirrors the behaviour of WritePrivateProfileString as described
// in
// https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-writeprivateprofilestringw
template <typename CharT>
bool SetValue(const CharT* appName, const CharT* keyName, const CharT* value,
              const std::filesystem::path& fileName)
{
  // check types
  static_assert(is_one_of<CharT, char, wchar_t>(),
                "template parameter must be char or wchar_t");

  // use ifstream/ofstream when CharT is char and wifstream/wofstream when CharT is
  // wchar_t
  using InStream =
      std::conditional_t<std::is_same_v<CharT, char>, std::ifstream, std::wifstream>;
  using OutStream =
      std::conditional_t<std::is_same_v<CharT, char>, std::ofstream, std::wofstream>;

  inipp::Ini<CharT> ini;

  // read ini file if it exists
  if (exists(fileName)) {
    InStream in(fileName);
    if (!in.is_open()) {
      return false;
    }
    ini.parse(in);
    in.close();
  }

  if (keyName == nullptr) {
    // remove section if key is nullptr
    ini.sections.erase(appName);
  } else if (value == nullptr) {
    // remove key if value is nullptr
    ini.sections[appName].erase(keyName);
  } else {
    ini.sections[appName][keyName] = value;
  }

  // write the modified ini file
  OutStream out(fileName);
  if (!out.is_open()) {
    return false;
  }
  ini.generate(out);
  return ini.errors.empty();
}

template <typename CharT>
std::optional<std::basic_string<CharT>>
GetValue(const CharT* appName, const CharT* keyName, const CharT* defaultValue,
         const std::filesystem::path& fileName)
{
  // check types
  static_assert(is_one_of<CharT, char, wchar_t>());

  using InStream =
      std::conditional_t<std::is_same_v<CharT, char>, std::ifstream, std::wifstream>;
  using String = std::basic_string<CharT>;
  CharT newline;
  if constexpr (std::is_same_v<CharT, char>) {
    newline = '\n';
  } else {
    newline = L'\n';
  }

  // read ini file
  inipp::Ini<CharT> ini;
  {
    InStream in(fileName);
    if (!in.is_open()) {
      return {};
    }
    ini.parse(in);
  }

  if (appName == nullptr) {
    // return all section names in the file
    String result;
    for (const auto& section : ini.sections) {
      result.append(section.first);
      result += newline;
    }
    result.pop_back();
    return result;
  }
  if (keyName == nullptr) {
    // return all key names in the section specified by the appName parameter
    String result;
    if (!ini.sections.contains(appName)) {
      return result;
    }
    for (const auto& key : ini.sections[appName]) {
      result.append(key.first);
      result += newline;
    }
    result.pop_back();
    return result;
  }

  try {
    return ini.sections.at(appName).at(keyName);
  } catch (...) {
    return defaultValue;
  }
}

template <typename CharT>
bool WriteValue(CharT appName, CharT keyName, CharT value,
                const std::filesystem::path& fileName)
{
  // check types
  static_assert(is_one_of<CharT, const char*, const wchar_t*>(),
                "template parameter must be const char* or const wchar_t*");

  bool success = true;

  if (!SetValue(appName, keyName, value, fileName)) {
    const int error = GetLastError();
    success         = false;
    if (error == ERROR_ACCESS_DENIED) {
#ifdef _WIN32
      // On NTFS file systems, ownership and permissions checking is disabled by default
      // for performance reasons. source:
      // https://doc.qt.io/qt-6/qfileinfo.html#ntfs-permissions
      QNtfsPermissionCheckGuard permissionGuard;
#endif
      QFile file(fileName);
      auto attrs = file.permissions();
      if (file.exists() && !file.isWritable() && file.isReadable()) {
        QMessageBox::StandardButton result =
            MOBase::TaskDialog(qApp->activeModalWidget(),
                               QObject::tr("INI file is read-only"))
                .main(QObject::tr("INI file is read-only"))
                .content(QObject::tr("Mod Organizer is attempting to write to \"%1\" "
                                     "which is currently set to read-only.")
                             .arg(file.fileName()))
                .icon(QMessageBox::Warning)
                .button({QObject::tr("Clear the read-only flag"), QMessageBox::Yes})
                .button({QObject::tr("Allow the write once"),
                         QObject::tr("The file will be set to read-only again."),
                         QMessageBox::Ignore})
                .button({QObject::tr("Skip this file"), QMessageBox::No})
                .remember("clearReadOnly", file.fileName())
                .exec();

        // clear the read-only flag if requested
        if (result & (QMessageBox::Yes | QMessageBox::Ignore)) {
          attrs |= QFile::Permission::WriteUser;
          if (file.setPermissions(attrs)) {
            if (SetValue(appName, keyName, value, fileName)) {
              success = true;
            }
          }
        }

        // set the read-only flag if requested
        if (result == QMessageBox::Ignore) {
          attrs &= ~QFile::Permission::WriteUser;
          file.setPermissions(attrs);
        }
      }
    }
  }

  return success;
}

}  // namespace

namespace MOBase
{

bool WriteRegistryValue(const wchar_t* appName, const wchar_t* keyName,
                        const wchar_t* value, const wchar_t* fileName)
{
  return WriteValue(appName, keyName, value, std::filesystem::path(fileName));
}

bool WriteRegistryValue(const char* appName, const char* keyName, const char* value,
                        const char* fileName)
{
  return WriteValue(appName, keyName, value, std::filesystem::path(fileName));
}

std::optional<std::wstring> ReadRegistryValue(const wchar_t* appName,
                                              const wchar_t* keyName,
                                              const wchar_t* defaultValue,
                                              const wchar_t* fileName)
{
  return GetValue(appName, keyName, defaultValue, fileName);
}

std::optional<std::string> ReadRegistryValue(const char* appName, const char* keyName,
                                             const char* defaultValue,
                                             const char* fileName)
{
  return GetValue(appName, keyName, defaultValue, fileName);
}

}  // namespace MOBase
