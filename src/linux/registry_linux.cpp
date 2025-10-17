#include "registry.h"

#include "inipp.h"
#include <filesystem>
#include <fstream>
#include <ranges>

#include <linux/compatibility.h>

using namespace std;
namespace fs = std::filesystem;

namespace
{
template <typename T, typename... ValidTypes>
constexpr bool is_one_of()
{
  return (is_same_v<T, ValidTypes> || ...);
}

template <typename CharT>
bool readIni(const fs::path& filename, inipp::Ini<CharT>& ini)
{
  static_assert(is_one_of<CharT, char, wchar_t>(),
                "template parameter must be char or wchar_t");

  using InStream = conditional_t<is_same_v<CharT, char>, ifstream, wifstream>;

  locale loc(setlocale(LC_ALL, ""));

  InStream in(filename);
  in.imbue(loc);

  if (!in.is_open()) {
    errno = EIO;
    return false;
  }
  ini.parse(in);
  if (!ini.errors.empty()) {
    errno = EIO;
    return false;
  }

  return true;
}

template <typename CharT>
bool saveIni(const fs::path& filename, inipp::Ini<CharT>& ini)
{
  static_assert(is_one_of<CharT, char, wchar_t>(),
                "template parameter must be char or wchar_t");

  using OutStream = conditional_t<is_same_v<CharT, char>, ofstream, wofstream>;

  locale loc(setlocale(LC_ALL, ""));

  OutStream out(filename, ios::trunc);
  out.imbue(loc);

  if (!out.is_open()) {
    errno = EIO;
    return false;
  }

  ini.generate(out);
  out.close();
  return ini.errors.empty();
}

/**
 * @brief Copy the contents of a string to another string
 * @param src String to copy from
 * @param dest String to copy to
 * @param dstSize Destination string size
 * @return Number of characters copied to the buffer, not including the terminating null
 * character
 */
template <typename CharT>
uint32_t copy_string(const basic_string<CharT>& src, CharT* dest, size_t dstSize)
{
  static_assert(is_one_of<CharT, char, wchar_t>(),
                "template parameter must be char or wchar_t");

  // truncate string if the destination size is too small
  if (src.size() > dstSize) {
    memcpy(dest, src.c_str(), dstSize);
    dest[dstSize - 1] = '\0';
    return dstSize - 1;
  }

  memcpy(dest, src.c_str(), src.size() * sizeof(CharT));
  return src.size();
}

template <typename CharT>
uint32_t GetPrivateProfileString(const CharT* section, const CharT* key,
                                 const CharT* defaultValue, CharT* returnedString,
                                 size_t length, const fs::path& path)
{
  static_assert(is_one_of<CharT, char, wchar_t>());
  using InStream = conditional_t<is_same_v<CharT, char>, ifstream, wifstream>;

  errno           = 0;
  uint32_t copied = 0;

  // check if the file exists
  if (!exists(path)) {
    errno = ENOENT;
    return copied;
  }

  // get effective default value (empty string if null)
  const CharT* effectiveDefault = defaultValue;
  if (effectiveDefault == nullptr) {
    if constexpr (is_same_v<CharT, char>) {
      effectiveDefault = "";
    } else {
      effectiveDefault = L"";
    }
  }

  basic_string<CharT> result;

  // read ini file
  inipp::Ini<CharT> ini;
  InStream in(path);
  if (!in.is_open()) {
    errno = EIO;
    return copied;
  }
  ini.parse(in);

  if (section == nullptr) {
    // list all section names
    for (const auto& sectionName : ini.sections | views::keys) {
      result += sectionName + static_cast<CharT>(0);
    }
  } else if (key == nullptr) {
    // list all keys in the specified section
    for (const auto& keyName : ini.sections[section] | views::keys) {
      result += keyName + static_cast<CharT>(0);
    }
  } else {
    // get specified value
    try {
      result = ini.sections.at(section).at(key);
    } catch (const out_of_range&) {
      // value does not exist, use default
      result = effectiveDefault;
    }
    return copy_string(result, returnedString, length);
  }

  // copy result to the output buffer
  copied = copy_string(result, returnedString, length);

  // handle truncation for list results (add double null termination)
  if (copied < result.size() && length >= 2) {
    returnedString[length - 2] = 0;
    returnedString[length - 1] = 0;
    --copied;
  }

  return copied;
}

template <typename CharT>
uint32_t GetPrivateProfileSectionNames(CharT* buffer, uint32_t bufferSize,
                                       const fs::path& path)
{
  return GetPrivateProfileString(
      static_cast<const CharT*>(nullptr), static_cast<const CharT*>(nullptr),
      static_cast<const CharT*>(nullptr), buffer, bufferSize, path);
}

template <typename CharT>
bool WritePrivateProfileString(const CharT* section, const CharT* key,
                               const CharT* value, const fs::path& filename)
{
  static_assert(is_one_of<CharT, char, wchar_t>(),
                "template parameter must be char or wchar_t");

  errno = 0;

  if (!section) {
    errno = EINVAL;
    return false;
  }

  inipp::Ini<CharT> ini;

  if (exists(filename)) {
    if (!readIni(filename, ini)) {
      return false;
    }
  }

  if (key == nullptr) {
    // remove section if key is nullptr
    ini.sections.erase(section);
  } else if (value == nullptr) {
    // remove key if value is nullptr
    ini.sections[section].erase(key);
  } else {
    ini.sections[section][key] = value;
  }

  return saveIni(filename, ini);
}

template <typename CharT>
bool WritePrivateProfileSection(const CharT* section, const CharT* data,
                                const fs::path& filename)
{
  static_assert(is_one_of<CharT, char, wchar_t>(),
                "template parameter must be char or wchar_t");

  errno = 0;

  if (!section) {
    errno = EINVAL;
    return false;
  }

  inipp::Ini<CharT> ini;

  if (exists(filename)) {
    if (!readIni(filename, ini)) {
      return false;
    }
  }

  // remove all existing keys in the section
  ini.sections.erase(section);

  if (data == nullptr) {
    // create a new section
    ini.sections[section];
  } else {
    // create all keys
    size_t pos = 0;
    basic_string<CharT> line;
    while (true) {
      line = data + pos;

      auto equalSignPos = line.find('=');
      if (equalSignPos == basic_string<CharT>::npos) {
        cerr << "'=' not found\n";
        return false;
      }
      basic_string<CharT> key    = line.substr(0, equalSignPos);
      basic_string<CharT> value  = line.substr(equalSignPos + 1);
      ini.sections[section][key] = value;

      pos += line.size() + 1;
      if (data[pos + 1] == 0) {
        break;
      }
    }
  }

  return saveIni(filename, ini);
}

}  // namespace

bool WritePrivateProfileSectionA(const char* lpAppName, const char* lpString,
                                 const char* lpFileName)
{
  return WritePrivateProfileSection(lpAppName, lpString, lpFileName);
}

bool WritePrivateProfileSectionW(const wchar_t* lpAppName, const wchar_t* lpString,
                                 const wchar_t* lpFileName)
{
  return WritePrivateProfileSection(lpAppName, lpString, lpFileName);
}

bool WritePrivateProfileStringW(const wchar_t* lpAppName, const wchar_t* lpKeyName,
                                const wchar_t* lpString, const wchar_t* lpFileName)
{
  return WritePrivateProfileString(lpAppName, lpKeyName, lpString, lpFileName);
}

bool WritePrivateProfileStringA(const char* lpAppName, const char* lpKeyName,
                                const char* lpString, const char* lpFileName)
{
  return WritePrivateProfileString(lpAppName, lpKeyName, lpString, lpFileName);
}

uint32_t GetPrivateProfileStringW(const wchar_t* lpAppName, const wchar_t* lpKeyName,
                                  const wchar_t* lpDefault, wchar_t* lpReturnedString,
                                  uint32_t nSize, const wchar_t* lpFileName)
{
  return GetPrivateProfileString(lpAppName, lpKeyName, lpDefault, lpReturnedString,
                                 nSize, lpFileName);
}

uint32_t GetPrivateProfileStringA(const char* lpAppName, const char* lpKeyName,
                                  const char* lpDefault, char* lpReturnedString,
                                  uint32_t nSize, const char* lpFileName)
{
  return GetPrivateProfileString(lpAppName, lpKeyName, lpDefault, lpReturnedString,
                                 nSize, lpFileName);
}

uint32_t GetPrivateProfileSectionNamesA(char* lpszReturnBuffer, uint32_t nSize,
                                        const char* lpFileName)
{
  return GetPrivateProfileSectionNames(lpszReturnBuffer, nSize, lpFileName);
}

uint32_t GetPrivateProfileSectionNamesW(wchar_t* lpszReturnBuffer, uint32_t nSize,
                                        const wchar_t* lpFileName)
{
  return GetPrivateProfileSectionNames(lpszReturnBuffer, nSize, lpFileName);
}
