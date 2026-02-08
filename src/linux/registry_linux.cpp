#include "log.h"
#include "registry.h"

#include "qinipp.h"

using namespace std;
namespace fs = std::filesystem;

namespace
{
bool readIni(const QString& filename, qinipp::Ini& ini)
{
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    MOBase::log::error("Error opening file '{}': {}", filename, file.errorString());
    errno = EIO;
    return false;
  }

  QTextStream in(&file);
  ini.parse(in);
  if (!ini.errors.empty()) {
    errno = EIO;
    return false;
  }

  return true;
}

bool saveIni(const QString& filename, qinipp::Ini& ini)
{
  QFile file(filename);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    MOBase::log::error("Error opening file '{}': {}", filename, file.errorString());
    errno = EIO;
    return false;
  }

  QTextStream out(&file);
  ini.generate(out);
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
uint32_t copy_string(const QString& src, QString& dest, size_t dstSize)
{
  // truncate string if the destination size is too small
  if (src.size() > dstSize) {
    memcpy(dest.data(), src.data(), dstSize);
    dest[dstSize - 1] = '\0';
    return dstSize - 1;
  }

  memcpy(dest.data(), src.data(), src.size() * sizeof(QChar));
  return src.size();
}

uint32_t GetPrivateProfileString(const QString& section, const QString& key,
                                 const QString& defaultValue, QString& returnedString,
                                 size_t length, const QString& path)
{
  errno           = 0;
  uint32_t copied = 0;

  // check if the file exists
  if (!QFileInfo::exists(path)) {
    errno = ENOENT;
    return copied;
  }

  // get effective default value (empty string if null)
  QString effectiveDefault = defaultValue.isNull() ? "" : defaultValue;

  QString result;

  // read ini file
  qinipp::Ini ini;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    MOBase::log::error("Error opening file '{}': {}", path, file.errorString());
    errno = EIO;
    return copied;
  }

  QTextStream in(&file);
  ini.parse(in);

  if (section == nullptr) {
    // list all section names
    for (const auto& sectionName : ini.sections | views::keys) {
      result += sectionName % '\0';
    }
  } else if (key == nullptr) {
    // list all keys in the specified section
    for (const auto& keyName : ini.sections[section] | views::keys) {
      result += keyName % '\0';
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
    returnedString[length - 2] = '\0';
    returnedString[length - 1] = '\0';
    --copied;
  }

  return copied;
}

uint32_t GetPrivateProfileSectionNames(QString& buffer, uint32_t bufferSize,
                                       const QString& path)
{
  return GetPrivateProfileString(QString(), QString(), QString(), buffer, bufferSize,
                                 path);
}

bool WritePrivateProfileString(const QString& section, const QString& key,
                               const QString& value, const QString& filename)
{
  errno = 0;

  if (section.isNull()) {
    errno = EINVAL;
    return false;
  }

  qinipp::Ini ini;

  if (QFileInfo::exists(filename)) {
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

bool WritePrivateProfileSection(const QString& section, const QString& data,
                                const QString& filename)
{
  errno = 0;

  if (section.isNull()) {
    errno = EINVAL;
    return false;
  }

  qinipp::Ini ini;

  if (QFileInfo::exists(filename)) {
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

    auto lines = data.split("\0", Qt::SkipEmptyParts);
    for (const auto& line : lines) {
      auto equalSignPos = line.indexOf('=');
      if (equalSignPos == -1) {
        MOBase::log::error("WritePrivateProfileSection(): '=' not found");
        return false;
      }
      QString key                = line.first(equalSignPos);
      QString value              = line.sliced(equalSignPos + 1);
      ini.sections[section][key] = value;
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
  return WritePrivateProfileSection(QString::fromWCharArray(lpAppName),
                                    QString::fromWCharArray(lpString),
                                    QString::fromWCharArray(lpFileName));
}

bool WritePrivateProfileStringW(const wchar_t* lpAppName, const wchar_t* lpKeyName,
                                const wchar_t* lpString, const wchar_t* lpFileName)
{
  return WritePrivateProfileString(
      QString::fromWCharArray(lpAppName), QString::fromWCharArray(lpKeyName),
      QString::fromWCharArray(lpString), QString::fromWCharArray(lpFileName));
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
  QString returnedString;
  uint32_t result = GetPrivateProfileString(
      QString::fromWCharArray(lpAppName), QString::fromWCharArray(lpKeyName),
      QString::fromWCharArray(lpDefault), returnedString, nSize,
      QString::fromWCharArray(lpFileName));

  vector<wchar_t> buf(nSize, 0);
  returnedString.toWCharArray(buf.data());
  memcpy(lpReturnedString, buf.data(), result);
  return result;
}

uint32_t GetPrivateProfileStringA(const char* lpAppName, const char* lpKeyName,
                                  const char* lpDefault, char* lpReturnedString,
                                  uint32_t nSize, const char* lpFileName)
{
  QString returnedString;
  auto result = GetPrivateProfileString(
      QString::fromLocal8Bit(lpAppName), QString::fromLocal8Bit(lpKeyName),
      QString::fromLocal8Bit(lpDefault), returnedString, nSize, lpFileName);

  auto buf = returnedString.toLocal8Bit();
  memcpy(lpReturnedString, buf.constData(), result);
  return result;
}

uint32_t GetPrivateProfileSectionNamesA(char* lpszReturnBuffer, uint32_t nSize,
                                        const char* lpFileName)
{
  QString returnBuffer;
  auto result = GetPrivateProfileSectionNames(returnBuffer, nSize,
                                              QString::fromLocal8Bit(lpFileName));

  auto buf = returnBuffer.toLocal8Bit();
  memcpy(lpszReturnBuffer, buf.constData(), result);
  return result;
}

uint32_t GetPrivateProfileSectionNamesW(wchar_t* lpszReturnBuffer, uint32_t nSize,
                                        const wchar_t* lpFileName)
{
  QString returnBuffer;
  auto result = GetPrivateProfileSectionNames(returnBuffer, nSize,
                                              QString::fromWCharArray(lpFileName));

  vector<wchar_t> buf(nSize, 0);
  returnBuffer.toWCharArray(buf.data());
  memcpy(lpszReturnBuffer, buf.data(), result);
  return result;
}
