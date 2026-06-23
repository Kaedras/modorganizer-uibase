#include "../steamutility.h"

#include "log.h"
#include "utility.h"
#include <sys/mman.h>
#include <vdf_parser.hpp>

using namespace Qt::StringLiterals;
using namespace std;

namespace MOBase
{

QString findSteam()
{
  QString home = QDir::homePath();

  static const QStringList paths = {
      home % "/.local/share/Steam"_L1, home % "/.steam/steam"_L1,
      home % "/.var/app/com.valvesoftware.Steam/.local/share/Steam"_L1};

  for (const auto& path : paths) {
    if (QFile::exists(path)) {
      return path;
    }
  }

  return "";
}

QString protonNameByAppID(const QString& appID)
{
  // proton versions can be parsed from <steamDir>/config/config.vdf
  // InstallConfigStore -> Software -> Valve -> Steam -> CompatToolMapping
  // default version is stored as appID 0
  try {
    QDir steamDir(findSteamCached());
    if (!steamDir.exists()) {
      return {};
    }

    string configPath = steamDir.filesystemAbsolutePath() / "config/config.vdf";

    log::debug("parsing {}", configPath);

    ifstream config(configPath);
    if (!config.is_open()) {
      const int error = errno;
      log::error("could not open steam config file {}: {}", configPath,
                 strerror(error));
      return {};
    }
    auto root     = tyti::vdf::read(config);
    auto software = root.childs.at("Software");
    // according to ProtonUp-Qt source code, the key can either be "Valve" or "valve"
    auto valve = software->childs["Valve"];
    if (valve == nullptr) {
      // "Valve" does not exist, try "valve"
      valve = software->childs.at("valve");
    }
    auto compatToolMapping = valve->childs.at("Steam")->childs.at("CompatToolMapping");
    auto tmp               = compatToolMapping->childs[appID.toStdString()];
    if (tmp != nullptr) {
      return QString::fromStdString(tmp->attribs.at("name"));
    }
    // version is not set manually, use the version from appID 0
    return QString::fromStdString(
        compatToolMapping->childs.at("0")->attribs.at("name"));
  } catch (const out_of_range& ex) {
    log::error("Error getting proton name for appid {}, {}", appID, ex.what());
    return {};
  }
}

QString protonByAppID(const QString& appID)
{
  QDir steamDir(findSteamCached());
  if (!steamDir.exists()) {
    return {};
  }

  QString protonName = protonNameByAppID(appID);
  if (protonName.isEmpty()) {
    return {};
  }

  log::debug("Found proton name {}", protonName);

  // create the directory name from proton name, e.g.
  // proton_411 -> Proton 4.11
  // proton_5 -> Proton 5.0
  // proton_513 -> Proton 5.13

  QString proton;
  if (protonName.startsWith("proton_"_L1)) {
    // normal proton, installed as steam tool

    // convert "proton_<version>" to "Proton <version>"
    protonName.replace("proton_"_L1, "Proton "_L1);

    static const QRegularExpression regex(u"\\d"_s);
    qsizetype index = protonName.indexOf(regex);
    if (index == -1) {
      log::error("Could not find proton path for appid {}", appID);
      return {};
    }

    // append `.0` if name contains a single digit
    if (index == protonName.size() - 1) {
      protonName.append(".0"_L1);
    } else {
      // insert `.` after first digit
      protonName.insert(index + 1, '.');
    }

    proton = findSteamGame(protonName, u"proton"_s);
  } else {
    // custom proton e.g. GE-Proton9-25, located in <steamDir>/compatibilitytools.d/
    proton = steamDir.absolutePath() % u"/compatibilitytools.d/"_s % protonName %
             u"/proton"_s;
    if (!QFile::exists(proton)) {
      log::error("Detected proton path \"{}\" does not exist", proton);
      return {};
    }
  }
  log::debug("Proton found at {}", proton);
  return proton;
}

QString protonByPrefixPath(const QDir& prefixPath)
{
  // read proton path from <prefix>/config_info

  if (!prefixPath.exists()) {
    log::error("prefix path {} does not exist", prefixPath.absolutePath());
    return {};
  }

  // fallback for old proton versions
  if (!prefixPath.exists(u"config_info"_s)) {
    log::debug("config_info does not exist, falling back to protonByAppID()");
    return protonByAppID(prefixPath.dirName());
  }

  QFile info(prefixPath.absoluteFilePath(u"config_info"_s));
  if (!info.open(QIODeviceBase::ReadOnly)) {
    log::error("error opening {}, {}", info.fileName(), info.errorString());
    return {};
  }

  // skip the first line
  info.readLine();

  QString result = info.readLine();
  if (!result.endsWith("/files/share/fonts/\n"_L1)) {
    log::error("Error parsing {}", info.fileName());
    return {};
  }

  // remove trailing "files/share/fonts/\n"
  result.chop(19);

  // append "proton"
  result.append(u"proton"_s);

  return result;
}

// types, classes and functions for getRuntime()
namespace
{
  constexpr size_t stringTableOffsetLocation = 8;

  // using memory mapping improves performance significantly when `appconfig.vdf` is ~20
  // MiB
  struct MemoryMappedFile
  {
    explicit MemoryMappedFile(const char* path)
    {
      fd = open(path, O_RDONLY);
      if (fd == -1) {
        const int e = errno;
        throw runtime_error("error opening file: "s + strerror(e));
      }
      struct stat st;
      if (fstat(fd, &st) == -1) {
        const int e = errno;
        throw runtime_error("fstat failed: "s + strerror(e));
      }

      length = st.st_size;

      addr =
          static_cast<uint8_t*>(mmap(nullptr, length, PROT_READ, MAP_PRIVATE, fd, 0));
      if (addr == MAP_FAILED) {
        close(fd);
        const int e = errno;
        throw runtime_error("mmap failed: "s + strerror(e));
      }
    }

    ~MemoryMappedFile()
    {
      if (addr != nullptr) {
        if (munmap(addr, length) == -1) {
          const int e = errno;
          log::error("munmap failed, {}", strerror(e));
        }
      }
      if (fd != -1) {
        close(fd);
      }
    }
    int fd;
    uint8_t* addr;
    size_t length;
  };

  enum Type : uint8_t
  {
    NONE,    // a nested Binary VDF document
    STRING,  // a null-terminated byte string
    INT,     // a 32-bit little-endian signed integer
    FLOAT,   // a 32-bit little-endian IEEE-754 single-precision floating point number
    PTR,  // a 32-bit pointer; not generally useful for applications of KeyValues that
          // require serialization
    WSTRING,  // a 16-bit little-endian signed integer representing the number of
              // characters in the string (not including null terminator), followed by
              // that many little-endian UCS-2 codepoints. (Note: Valve's implementation
              // of KeyValues in Source SDK 2013 does not support this.)
    COLOR,   // an RGBA8888 color, consisting of one byte each for red, green, blue, and
             // alpha, in that order
    UINT64,  // a 64-bit little-endian unsigned integer
    NUMTYPES,  // not actually a type; this "type" has no name or data and represents
               // the end of a binary VDF document.
  };

  struct Unused
  {};
  struct BinaryVdfValue;
  struct BinaryVdfDocument
  {
    string_view name;
    unordered_map<string_view, BinaryVdfValue> values;
  };

  using BinaryVdfData = std::variant<BinaryVdfDocument, string_view, Unused>;

  struct BinaryVdfValue
  {
    Type type;
    string_view name;
    BinaryVdfData data;

    const BinaryVdfDocument& asDocument() const { return get<BinaryVdfDocument>(data); }
    const string_view& asString() const { return get<string_view>(data); }
  };

  class Reader
  {
  public:
    explicit Reader(const span<const uint8_t> data)
        : m_data(data), m_position(0), m_size(data.size())
    {}

    [[nodiscard]] size_t tell() const { return m_position; }

    void seek(const size_t offset, const int whence)
    {
      switch (whence) {
      case SEEK_SET:
        if (offset > m_size) {
          throw runtime_error("Seek is out of range");
        }
        m_position = offset;
        break;
      case SEEK_CUR:
        if (m_position + offset > m_size) {
          throw runtime_error("Seek is out of range");
        }
        m_position += offset;
        break;
      case SEEK_END:
        if (offset > 0) {
          throw runtime_error("Seek is out of range");
        }
        m_position = m_size + offset;
        break;
      [[unlikely]] default:
        throw runtime_error("Invalid seek direction " + to_string(whence));
      }
    }

    template <typename T>
    void read(T& value)
    {
      if (m_position + sizeof(T) > m_size) {
        throw runtime_error("Read is out of range");
      }
      memcpy(&value, m_data.data() + m_position, sizeof(T));
      m_position += sizeof(T);
    }

    template <typename T>
    T read()
    {
      T value;
      read(value);
      return value;
    }

  protected:
    span<const uint8_t> m_data;
    size_t m_position;
    size_t m_size;
  };

  template <>
  string_view Reader::read()
  {
    // get length
    size_t length      = 0;
    const size_t start = m_position;

    while (m_data[m_position++] != '\0' && m_position <= m_size) {
      ++length;
    }

    if (m_position > m_size) {
      throw runtime_error("Read is out of range");
    }

    return {reinterpret_cast<const char*>(m_data.data()) + start, length};
  }

  class BinaryVdf : public Reader
  {
  public:
    explicit BinaryVdf(const std::span<const uint8_t> data)
        : Reader(data), m_strings(nullptr)
    {}
    void setStringTable(const std::vector<std::string_view>* stringTable)
    {
      m_strings = stringTable;
    }
    void parse()
    {
      const auto v    = parseValue();
      m_document      = get<BinaryVdfDocument>(v.data);
      m_document.name = v.name;
    }

    [[nodiscard]] const BinaryVdfDocument& getDocument() const { return m_document; }

  private:
    BinaryVdfDocument m_document;
    const std::vector<std::string_view>* m_strings;

    BinaryVdfValue parseValue()
    {
      BinaryVdfValue value;

      read(reinterpret_cast<uint8_t&>(value.type));
      if (value.type == NUMTYPES) {
        return value;
      }

      if (value.type > NUMTYPES) {
        throw runtime_error("Invalid value type " + to_string(value.type));
      }

      if (m_strings == nullptr) {
        value.name = read<string_view>();
      } else {
        value.name = (*m_strings)[read<uint32_t>()];
      }

      switch (value.type) {
      case NONE:
        value.data = parseDocument(value.name);
        break;

      case STRING:
        value.data = read<string_view>();
        break;

      case PTR:
      case INT:
      case FLOAT:
      case COLOR:
        value.data = Unused();
        seek(4, SEEK_CUR);
        break;

      case WSTRING:
        value.data = Unused();
        seek(read<uint32_t>() * sizeof(char16_t), SEEK_CUR);
        break;

      case UINT64:
        value.data = Unused();
        seek(sizeof(uint64_t), SEEK_CUR);
        break;

      [[unlikely]] default:
        throw runtime_error("Invalid value type " + to_string(value.type));
      }

      return value;
    }

    BinaryVdfDocument parseDocument(std::string_view name)
    {
      BinaryVdfDocument document;
      document.name = name;
      document.values.reserve(10);
      while (true) {
        auto result = parseValue();
        if (result.type == NUMTYPES) {
          break;
        }
        document.values.emplace(result.name, std::move(result));
      }
      return document;
    }
  };

  QString getInstalledBranch(const QString& gameLocation, const QString& appID)
  {
    // read appmanifest
    QFile manifest(gameLocation % "/../../appmanifest_"_L1 % appID % ".acf"_L1);
    if (!manifest.open(QIODevice::ReadOnly | QIODevice::Text)) {
      throw runtime_error("Error opening app manifest, "s +
                          manifest.errorString().toStdString());
    }

    const QByteArray data = manifest.readAll();

    /*
    example data:
    \t"UserConfig"
    \t{
    \t\t"language"\t\t"english"
    \t\t"BetaKey"\t\t"7.60"
    }

    "BetaKey" corresponds to "branch" in binary vdf
     */
    static const QRegularExpression regex(
        uR"-(\t"UserConfig"\n\t\{\n.*\n\t+"BetaKey"\t+"(.*)"\n\t\})-"_s);
    const QRegularExpressionMatch result = regex.match(data);

    if (!result.hasMatch()) {
      // no match, therefore, the default branch is installed
      return {};
    }
    return result.captured(1);
  }
}  // namespace

QString getRequiredLinuxRuntime(const QString& gameLocation,
                                const QString& appID) noexcept(false)
{
  string appInfoPath = findSteamCached().toStdString() + "/appcache/appinfo.vdf";
  const MemoryMappedFile file(appInfoPath.c_str());

  Reader reader({file.addr, file.length});

  // check version
  const auto version = reader.read<uint8_t>();

  if (version < 36 || version > 41) {
    throw runtime_error("Invalid or unsupported appinfo version: " +
                        to_string(version));
  }

  const bool hasStringTable = version >= 41;

  vector<string_view> strings;
  if (hasStringTable) {
    // file uses a string table
    reader.seek(stringTableOffsetLocation, SEEK_SET);
    const auto offset = reader.read<int64_t>();

    reader.seek(offset, SEEK_SET);

    const auto length = reader.read<uint32_t>();

    strings.reserve(length);
    for (uint32_t i = 0; i < length; ++i) {
      strings.push_back(reader.read<string_view>());
    }
    reader.seek(stringTableOffsetLocation + 8, SEEK_SET);
  }

  const uint32_t appIDint = appID.toInt();
  while (true) {
    const auto entryAppID = reader.read<uint32_t>();
    if (entryAppID == 0) {
      break;
    }

    const auto size = reader.read<uint32_t>();
    if (appIDint != entryAppID) {
      // seek to next entry
      reader.seek(size, SEEK_CUR);
    } else {
      // 3 * uint32_t
      size_t offset = 12;
      // calculate offset of binary vdf section based on version
      // >= 38: uint64_t + SHA-1 -> 28 byte
      // >= 40: above + SHA-1 -> 48 byte
      if (version >= 40) {
        offset += 48;
      } else if (version >= 38) {
        offset += 28;
      }

      reader.seek(offset, SEEK_CUR);
      BinaryVdf vdf({file.addr + reader.tell(), size});
      if (hasStringTable) {
        vdf.setStringTable(&strings);
      }
      vdf.parse();

      const auto& document = vdf.getDocument();
      if (!document.values.contains("config")) {
        throw runtime_error("VDF does not contain a config segment");
      }
      const auto& config = document.values.at("config").asDocument();
      try {
        auto& mappings      = config.values.at("app_mappings").asDocument();
        const string branch = getInstalledBranch(gameLocation, appID).toStdString();
        for (const auto& value : mappings.values | views::values) {
          auto& mapping = value.asDocument();
          // skip entry if platform != linux
          if (mapping.values.at("platform").asString() != "linux") {
            continue;
          }

          if (branch.empty() && !mapping.values.contains("branch")) {
            const auto sv = mapping.values.at("tool").asString();
            return QString::fromUtf8(sv.data(), static_cast<qsizetype>(sv.length()));
          }

          if (!branch.empty() && mapping.values.contains("branch")) {
            if (mapping.values.at("branch").asString() == branch) {
              const auto sv = mapping.values.at("tool").asString();
              return QString::fromUtf8(sv.data(), static_cast<qsizetype>(sv.length()));
            }
          }
        }

        return {};
      } catch (const out_of_range&) {
        // no app_mappings, so no runtime should be used
        return {};
      }
    }
  }

  throw runtime_error("Error determining runtime");
}

}  // namespace MOBase
