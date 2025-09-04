#include "registry.h"

#include "inipp.h"
#include <filesystem>
#include <fstream>
#include <ranges>

#include <linux/compatibility.h>

namespace
{
template <typename T, typename... ValidTypes>
constexpr bool is_one_of()
{
  return (std::is_same_v<T, ValidTypes> || ...);
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
uint32_t copy_string(const std::basic_string<CharT>& src, CharT* dest, size_t dstSize)
{
  static_assert(is_one_of<CharT, char, wchar_t>());

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
                                 size_t length, const std::filesystem::path& path)
{
  static_assert(is_one_of<CharT, char, wchar_t>());
  using InStream =
      std::conditional_t<std::is_same_v<CharT, char>, std::ifstream, std::wifstream>;

  errno           = 0;
  uint32_t copied = 0;

  // check if the file exists
  if (!std::filesystem::exists(path)) {
    errno = ENOENT;
    return copied;
  }

  // get effective default value (empty string if null)
  const CharT* effectiveDefault = defaultValue;
  if (effectiveDefault == nullptr) {
    if constexpr (std::is_same_v<CharT, char>) {
      effectiveDefault = "";
    } else {
      effectiveDefault = L"";
    }
  }

  std::basic_string<CharT> result;

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
    for (const auto& sectionName : ini.sections | std::views::keys) {
      result += sectionName + static_cast<CharT>(0);
    }
  } else if (key == nullptr) {
    // list all keys in the specified section
    for (const auto& keyName : ini.sections[section] | std::views::keys) {
      result += keyName + static_cast<CharT>(0);
    }
  } else {
    // get specified value
    try {
      result = ini.sections.at(section).at(key);
    } catch (const std::out_of_range&) {
      // value does not exist, use default
      result = effectiveDefault;
    }
    return copy_string(result, returnedString, length);
  }

  // copy result to output buffer
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
                                       const std::filesystem::path& path)
{
  return GetPrivateProfileString(
      static_cast<const CharT*>(nullptr), static_cast<const CharT*>(nullptr),
      static_cast<const CharT*>(nullptr), buffer, bufferSize, path);
}

template <typename CharT>
bool WritePrivateProfileString(const CharT* section, const CharT* key,
                               const CharT* value,
                               const std::filesystem::path& filename)
{
  static_assert(is_one_of<CharT, char, wchar_t>(),
                "template parameter must be char or wchar_t");

  using InStream =
      std::conditional_t<std::is_same_v<CharT, char>, std::ifstream, std::wifstream>;
  using OutStream =
      std::conditional_t<std::is_same_v<CharT, char>, std::ofstream, std::wofstream>;

  errno = 0;

  if (!section) {
    errno = EINVAL;
    return false;
  }

  std::locale loc(setlocale(LC_ALL, ""));

  inipp::Ini<CharT> ini;

  InStream in(filename);
  in.imbue(loc);
  if (in.is_open()) {
    ini.parse(in);
    if (!ini.errors.empty()) {
      errno = EIO;
      return false;
    }
  }
  in.close();

  if (key == nullptr) {
    // remove section if key is nullptr
    ini.sections.erase(section);
  } else if (value == nullptr) {
    // remove key if value is nullptr
    ini.sections[section].erase(key);
  } else {
    ini.sections[section][key] = value;
  }

  OutStream out(filename);
  out.imbue(loc);

  if (!out.is_open()) {
    errno = EIO;
    return false;
  }

  ini.generate(out);
  out.close();
  return ini.errors.empty();
}

template <typename CharT>
bool WritePrivateProfileSection(const CharT* section, const CharT* data,
                                const std::filesystem::path& filename)
{
  static_assert(is_one_of<CharT, char, wchar_t>(),
                "template parameter must be char or wchar_t");

  using InStream =
      std::conditional_t<std::is_same_v<CharT, char>, std::ifstream, std::wifstream>;
  using OutStream =
      std::conditional_t<std::is_same_v<CharT, char>, std::ofstream, std::wofstream>;

  errno = 0;

  if (!section) {
    errno = EINVAL;
    return false;
  }

  std::locale loc(setlocale(LC_ALL, ""));

  inipp::Ini<CharT> ini;

  InStream in(filename);
  in.imbue(loc);
  if (in.is_open()) {
    ini.parse(in);
    if (!ini.errors.empty()) {
      errno = EIO;
      return false;
    }
  }
  in.close();

  // remove all existing keys in section
  ini.sections.erase(section);

  if (data == nullptr) {
    // create new section
    ini.sections[section];
  } else {
    // create all keys
    size_t pos = 0;
    std::basic_string<CharT> line;
    while (true) {
      line = data + pos;

      auto equalSignPos = line.find('=');
      if (equalSignPos == std::wstring::npos) {
        std::cerr << "'=' not found" << std::endl;
        return false;
      }
      std::basic_string<CharT> key   = line.substr(0, equalSignPos);
      std::basic_string<CharT> value = line.substr(equalSignPos + 1);
      ini.sections[section][key]     = value;

      pos += line.size() + 1;
      if (data[pos + 1] == 0) {
        break;
      }
    }
  }

  OutStream out(filename);
  out.imbue(loc);

  if (!out.is_open()) {
    errno = EIO;
    return false;
  }

  ini.generate(out);
  out.close();
  return ini.errors.empty();
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

bool WritePrivateProfileStringW(const wchar_t* appName, const wchar_t* keyName,
                                const wchar_t* string, const wchar_t* fileName)
{
  return WritePrivateProfileString(appName, keyName, string, fileName);
}

bool WritePrivateProfileStringA(const char* appName, const char* keyName,
                                const char* string, const char* fileName)
{
  return WritePrivateProfileString(appName, keyName, string, fileName);
}

uint32_t GetPrivateProfileStringW(const wchar_t* appName, const wchar_t* keyName,
                                  const wchar_t* defaultString, wchar_t* returnedString,
                                  uint32_t size, const wchar_t* fileName)
{
  return GetPrivateProfileString(appName, keyName, defaultString, returnedString, size,
                                 fileName);
}

uint32_t GetPrivateProfileStringA(const char* appName, const char* keyName,
                                  const char* defaultString, char* returnedString,
                                  uint32_t size, const char* fileName)
{
  return GetPrivateProfileString(appName, keyName, defaultString, returnedString, size,
                                 fileName);
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
