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

#ifndef REGISTRY_H
#define REGISTRY_H

#include "dllimport.h"

namespace MOBase
{

QDLLEXPORT bool WriteRegistryValue(const wchar_t* appName, const wchar_t* keyName,
                                   const wchar_t* value, const wchar_t* fileName);

QDLLEXPORT bool WriteRegistryValue(const char* appName, const char* keyName,
                                   const char* value, const char* fileName);

QDLLEXPORT std::optional<std::wstring> ReadRegistryValue(const wchar_t* appName,
                                                         const wchar_t* keyName,
                                                         const wchar_t* defaultValue,
                                                         const wchar_t* fileName);

QDLLEXPORT std::optional<std::string> ReadRegistryValue(const char* appName,
                                                        const char* keyName,
                                                        const char* defaultValue,
                                                        const char* fileName);

#ifdef __unix__

QDLLEXPORT uint32_t GetPrivateProfileStringA(const char* lpAppName,
                                             const char* lpKeyName,
                                             const char* lpDefault,
                                             char* lpReturnedString, uint32_t nSize,
                                             const char* lpFileName);

QDLLEXPORT uint32_t GetPrivateProfileStringW(const wchar_t* lpAppName,
                                             const wchar_t* lpKeyName,
                                             const wchar_t* lpDefault,
                                             wchar_t* lpReturnedString, uint32_t nSize,
                                             const wchar_t* lpFileName);

QDLLEXPORT bool WritePrivateProfileStringA(const char* lpAppName, const char* lpKeyName,
                                           const char* lpString,
                                           const char* lpFileName);

QDLLEXPORT bool WritePrivateProfileStringW(const wchar_t* lpAppName,
                                           const wchar_t* lpKeyName,
                                           const wchar_t* lpString,
                                           const wchar_t* lpFileName);

#endif

}  // namespace MOBase

#endif  // REGISTRY_H
