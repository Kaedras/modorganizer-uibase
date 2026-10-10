#include "linux/exeutils.h"
#include <QFile>
#include <QIODevice>

namespace
{

struct DosHeader
{
  char signature[2];
  quint32 newHeaderOffset;
};

struct PeResourceDataEntry
{
  quint32 dataAddress;
  quint32 size;
  quint32 codepage;
  quint32 reserved;
};

// Resource Directory
enum class ResourceType : quint32
{
  Icon      = 3,
  GroupIcon = 14,
  Version   = 16,
};

struct RtGroupIconDirectory
{
  quint16 reserved;
  quint16 type;
  quint16 count;
};

struct RtGroupIconDirectoryEntry
{
  quint8 width;
  quint8 height;
  quint8 colorCount;
  quint8 reserved;
  quint16 numPlanes;
  quint16 bpp;
  quint32 size;
  quint16 resourceId;
};

// Icon file (.ico)
struct IconDir
{
  quint16 reserved;
  quint16 type;
  quint16 count;
};

constexpr int IconDirSize = 6;

struct IconDirEntry
{
  quint8 width;
  quint8 height;
  quint8 colorCount;
  quint8 reserved;
  quint16 numPlanes;
  quint16 bpp;
  quint32 size;
  quint32 imageOffset;

  IconDirEntry(const RtGroupIconDirectoryEntry& entry, quint32 dataOffset)
  {
    width       = entry.width;
    height      = entry.height;
    colorCount  = entry.colorCount;
    reserved    = entry.reserved;
    numPlanes   = entry.numPlanes;
    bpp         = entry.bpp;
    size        = entry.size;
    imageOffset = dataOffset;
  }
};

constexpr int IconDirEntrySize = 16;

// version info
struct PeVersionInfo
{
  quint16 StructLength;
  quint16 ValueLength;
  quint16 StructType;
  QChar Info[16];
  quint8 Padding[2];
  quint32 Signature;
  quint16 StructVersion[2];
  quint16 FileVersion[4];
  quint16 ProductVersion[4];
  quint32 FileFlagsMask[2];
  quint32 FileFlags;
  quint32 FileOS;
  quint32 FileType;
  quint32 FileSubtype;
  quint32 FileTimestamp;
};

// Win32 Portable Executable

constexpr quint16 PeOptionalHeaderMagicPe32     = 0x010b;
constexpr quint16 PeOptionalHeaderMagicPe32Plus = 0x020b;
constexpr quint32 PeSubdirBitMask               = 0x80000000;

constexpr int PeSignatureSize                 = 4;
constexpr int PeFileHeaderSize                = 20;
constexpr int PeOffsetToDataDirectoryPe32     = 120;
constexpr int PeOffsetToDataDirectoryPe32Plus = 136;
constexpr int PeDataDirectorySize             = 8;

enum class PeDataDirectoryIndex
{
  Resource = 2,
};

struct PeFileHeader
{
  quint16 machine;
  quint16 numSections;
  quint32 timestamp;
  quint32 offsetToSymbolTable;
  quint32 numberOfSymbols;
  quint16 sizeOfOptionalHeader;
  quint16 fileCharacteristics;
};

struct PeDataDirectory
{
  quint32 virtualAddress, size;
};

struct PeSection
{
  char name[8];
  quint32 virtualSize, virtualAddress;
  quint32 sizeOfRawData, pointerToRawData;
  quint32 pointerToRelocs, pointerToLineNums;
  quint16 numRelocs, numLineNums;
  quint32 characteristics;
};

struct PeResourceDirectoryTable
{
  quint32 characteristics;
  quint32 timestamp;
  quint16 majorVersion, minorVersion;
  quint16 numNameEntries, numIDEntries;
};

struct PeResourceDirectoryEntry
{
  quint32 resourceId, offset;
};

QDataStream& operator>>(QDataStream& s, DosHeader& v)
{
  s.readRawData(v.signature, sizeof(v.signature));
  s.device()->skip(58);
  s >> v.newHeaderOffset;
  return s;
}

QDataStream& operator>>(QDataStream& s, RtGroupIconDirectory& v)
{
  s >> v.reserved >> v.type >> v.count;
  return s;
}

QDataStream& operator>>(QDataStream& s, RtGroupIconDirectoryEntry& v)
{
  s >> v.width >> v.height >> v.colorCount >> v.reserved >> v.numPlanes >> v.bpp >>
      v.size >> v.resourceId;
  return s;
}

QDataStream& operator<<(QDataStream& s, const IconDir& v)
{
  s << v.reserved << v.type << v.count;
  return s;
}

QDataStream& operator<<(QDataStream& s, const IconDirEntry& v)
{
  s << v.width << v.height << v.colorCount << v.reserved << v.numPlanes << v.bpp
    << v.size << v.imageOffset;
  return s;
}

QDataStream& operator>>(QDataStream& s, PeVersionInfo& v)
{
  // FileVersion and ProductVersion order is [1] [0] [3] [2], not [0] [1] [2] [3]
  // because they are stored as 32-bit values
  s >> v.StructLength >> v.ValueLength >> v.StructType >> v.Info[0] >> v.Info[1] >>
      v.Info[2] >> v.Info[3] >> v.Info[4] >> v.Info[5] >> v.Info[6] >> v.Info[7] >>
      v.Info[8] >> v.Info[9] >> v.Info[10] >> v.Info[11] >> v.Info[12] >> v.Info[13] >>
      v.Info[14] >> v.Info[15] >> v.Padding[0] >> v.Padding[1] >> v.Signature >>
      v.StructVersion[0] >> v.StructVersion[1] >> v.FileVersion[1] >>
      v.FileVersion[0] >> v.FileVersion[3] >> v.FileVersion[2] >> v.ProductVersion[1] >>
      v.ProductVersion[0] >> v.ProductVersion[3] >> v.ProductVersion[2] >>
      v.FileFlagsMask[0] >> v.FileFlagsMask[1] >> v.FileFlags >> v.FileOS >>
      v.FileType >> v.FileSubtype >> v.FileTimestamp;
  return s;
}

QDataStream& operator>>(QDataStream& s, PeFileHeader& v)
{
  s >> v.machine >> v.numSections >> v.timestamp >> v.offsetToSymbolTable >>
      v.numberOfSymbols >> v.sizeOfOptionalHeader >> v.fileCharacteristics;
  return s;
}

QDataStream& operator>>(QDataStream& s, PeDataDirectory& v)
{
  s >> v.virtualAddress >> v.size;
  return s;
}

QDataStream& operator>>(QDataStream& s, PeSection& v)
{
  s.readRawData(v.name, sizeof(v.name));
  s >> v.virtualSize >> v.virtualAddress >> v.sizeOfRawData >> v.pointerToRawData >>
      v.pointerToRelocs >> v.pointerToLineNums >> v.numRelocs >> v.numLineNums >>
      v.characteristics;
  return s;
}

QDataStream& operator>>(QDataStream& s, PeResourceDirectoryTable& v)
{
  s >> v.characteristics >> v.timestamp >> v.majorVersion >> v.minorVersion >>
      v.numNameEntries >> v.numIDEntries;
  return s;
}

QDataStream& operator>>(QDataStream& s, PeResourceDirectoryEntry& v)
{
  s >> v.resourceId >> v.offset;
  return s;
}

QDataStream& operator>>(QDataStream& s, PeResourceDataEntry& v)
{
  s >> v.dataAddress >> v.size >> v.codepage >> v.reserved;
  return s;
}

qint64 addressToOffset(const QVector<PeSection>& sections, quint32 rva)
{
  for (const auto& section : sections) {
    quint32 sectionBegin = section.virtualAddress;
    auto effectiveSize   = section.sizeOfRawData;
    if (section.virtualSize) {
      effectiveSize = std::min(effectiveSize, section.virtualSize);
    }
    auto sectionEnd = section.virtualAddress + effectiveSize;
    if (rva >= sectionBegin && rva < sectionEnd) {
      return rva - sectionBegin + section.pointerToRawData;
    }
  }
  return -1;
}

QVector<PeResourceDirectoryEntry> readResourceDataDirectoryEntry(QDataStream& ds)
{
  PeResourceDirectoryTable table{};
  ds >> table;
  QVector<PeResourceDirectoryEntry> entries;
  for (int i = 0; i < table.numNameEntries + table.numIDEntries; i++) {
    PeResourceDirectoryEntry entry{};
    ds >> entry;
    entries.append(entry);
  }
  return entries;
}

quint32 selectBestLanguage(const QMap<quint32, PeResourceDataEntry>& langMap)
{
  if (langMap.empty()) {
    return 0;
  }
  for (auto it = langMap.keyBegin(); it != langMap.keyEnd(); ++it) {
    if ((*it & 0xff) == 9) {
      return *it;
    }
  }
  for (auto it = langMap.keyBegin(); it != langMap.keyEnd(); ++it) {
    if (*it != 0 && *it != 1024 && *it != 2048) {
      return *it;
    }
  }
  if (langMap.contains(0)) {
    return 0;
  }
  if (langMap.contains(1024)) {
    return 1024;
  }
  return langMap.firstKey();
}

class PeExtractor
{
public:
  explicit PeExtractor(QIODevice* inputDevice, QIODevice* outputDevice);

  QVector<PeSection> m_sections;
  QIODevice* m_inputDevice  = nullptr;
  QIODevice* m_outputDevice = nullptr;
  QDataStream m_inputStream;
  DosHeader m_dosHeader{};

  QMap<quint32, QMap<quint32, PeResourceDataEntry>> m_iconResources;
  QMap<quint32, QMap<quint32, PeResourceDataEntry>> m_groupIconResources;
  std::optional<PeResourceDataEntry> m_versionResource;

  bool readPeData();
  bool readIcon();
  bool readVersionInfo() const;
};

}  // namespace

bool PeExtractor::readPeData()
{
  m_inputDevice->seek(0);
  m_inputStream.setDevice(m_inputDevice);
  m_inputStream.setByteOrder(QDataStream::LittleEndian);

  // Read DOS header.
  m_inputStream >> m_dosHeader;

  // Verify the MZ header.
  if (m_dosHeader.signature[0] != 'M' || m_dosHeader.signature[1] != 'Z') {
    return false;
  }

  PeFileHeader fileHeader{};
  bool isPe32Plus;

  // Seek to + verify PE header. We're at the file header after this.
  if (!m_inputDevice->seek(m_dosHeader.newHeaderOffset)) {
    return false;
  }

  char signature[4];
  if (m_inputStream.readRawData(signature, sizeof(signature)) == -1) {
    return false;
  }

  if (signature[0] != 'P' || signature[1] != 'E' || signature[2] != 0 ||
      signature[3] != 0) {
    return false;
  }

  m_inputStream >> fileHeader;

  // Read optional header magic to determine if this is PE32 or PE32+.
  quint16 optMagic;
  m_inputStream >> optMagic;

  switch (optMagic) {
  case PeOptionalHeaderMagicPe32:
    isPe32Plus = false;
    break;

  case PeOptionalHeaderMagicPe32Plus:
    isPe32Plus = true;
    break;

  default:
    return false;
  }

  // Read section table now, so we can interpret RVAs.
  quint64 sectionTableOffset = m_dosHeader.newHeaderOffset;
  sectionTableOffset += PeSignatureSize + PeFileHeaderSize;
  sectionTableOffset += fileHeader.sizeOfOptionalHeader;
  if (!m_inputStream.device()->seek(sectionTableOffset)) {
    return false;
  }

  for (int i = 0; i < fileHeader.numSections; i++) {
    PeSection section{};
    m_inputStream >> section;
    m_sections.append(section);
  }

  // Find resource directory.
  qint64 dataDirOffset = m_dosHeader.newHeaderOffset;
  if (isPe32Plus) {
    dataDirOffset += PeOffsetToDataDirectoryPe32Plus;
  } else {
    dataDirOffset += PeOffsetToDataDirectoryPe32;
  }
  dataDirOffset +=
      static_cast<qint64>(PeDataDirectoryIndex::Resource) * PeDataDirectorySize;
  if (!m_inputStream.device()->seek(dataDirOffset)) {
    return false;
  }
  PeDataDirectory resourceDirectory{};
  m_inputStream >> resourceDirectory;

  // Read resource tree.
  auto resourceOffset = addressToOffset(m_sections, resourceDirectory.virtualAddress);
  if (resourceOffset < 0) {
    return false;
  }

  if (!m_inputStream.device()->seek(resourceOffset)) {
    return false;
  }

  const auto level1 = readResourceDataDirectoryEntry(m_inputStream);

  for (auto entry1 : level1) {
    if ((entry1.offset & PeSubdirBitMask) == 0)
      continue;
    if (!m_inputStream.device()->seek(resourceOffset +
                                      (entry1.offset & ~PeSubdirBitMask))) {
      return false;
    }

    const auto level2 = readResourceDataDirectoryEntry(m_inputStream);

    for (auto entry2 : level2) {
      if ((entry2.offset & PeSubdirBitMask) == 0)
        continue;
      if (!m_inputStream.device()->seek(resourceOffset +
                                        (entry2.offset & ~PeSubdirBitMask))) {
        return false;
      }

      // Read subdirectory.
      const auto level3 = readResourceDataDirectoryEntry(m_inputStream);

      for (auto entry3 : level3) {
        if ((entry3.offset & PeSubdirBitMask) == PeSubdirBitMask)
          continue;
        if (!m_inputStream.device()->seek(resourceOffset +
                                          (entry3.offset & ~PeSubdirBitMask))) {
          return false;
        }

        // Read data.
        PeResourceDataEntry dataEntry{};
        m_inputStream >> dataEntry;

        switch (static_cast<ResourceType>(entry1.resourceId)) {
        case ResourceType::Icon:
          m_iconResources[entry2.resourceId][entry3.resourceId] = dataEntry;
          break;

        case ResourceType::GroupIcon:
          m_groupIconResources[entry2.resourceId][entry3.resourceId] = dataEntry;
          break;

        case ResourceType::Version:
          m_versionResource = dataEntry;
          break;
        }
      }
    }
  }

  return true;
}

bool PeExtractor::readIcon()
{
  if (m_groupIconResources.isEmpty()) {
    return false;
  }

  QDataStream ds{m_inputDevice};
  ds.setByteOrder(QDataStream::LittleEndian);

  const auto& primaryGroupLanguageMap = m_groupIconResources.first();
  quint32 primaryIconGroupLanguage    = selectBestLanguage(primaryGroupLanguageMap);
  auto groupIt = primaryGroupLanguageMap.find(primaryIconGroupLanguage);
  if (groupIt == primaryGroupLanguageMap.end()) {
    groupIt = primaryGroupLanguageMap.begin();
    if (groupIt == primaryGroupLanguageMap.end()) {
      return false;
    }
    primaryIconGroupLanguage = groupIt.key();
  }
  PeResourceDataEntry primaryIconGroupResource = *groupIt;

  if (!ds.device()->seek(
          addressToOffset(m_sections, primaryIconGroupResource.dataAddress))) {
    return false;
  }

  QDataStream out{m_outputDevice};
  out.setByteOrder(QDataStream::LittleEndian);

  RtGroupIconDirectory primaryIconGroup{};
  ds >> primaryIconGroup;

  IconDir icoFileHeader{0, 1 /*Always 1 for ico files.*/, primaryIconGroup.count};
  out << icoFileHeader;

  quint32 dataOffset = IconDirSize + IconDirEntrySize * primaryIconGroup.count;
  QVector<QPair<qint64, quint32>> resourceOffsetSizePairs;

  for (int i = 0; i < primaryIconGroup.count; i++) {
    RtGroupIconDirectoryEntry entry{};
    ds >> entry;

    IconDirEntry icoFileEntry{entry, dataOffset};

    if (auto it = m_iconResources.find(entry.resourceId); it != m_iconResources.end()) {
      const auto& languageMap = it.value();
      if (languageMap.isEmpty()) {
        return false;
      }
      auto langIt = languageMap.find(primaryIconGroupLanguage);
      if (langIt == languageMap.end()) {
        quint32 bestLang = selectBestLanguage(languageMap);
        langIt           = languageMap.find(bestLang);
      }
      if (langIt == languageMap.end()) {
        langIt = languageMap.begin();
      }
      PeResourceDataEntry iconResource = *langIt;
      icoFileEntry.size                = iconResource.size;
      out << icoFileEntry;
      resourceOffsetSizePairs.append(
          {addressToOffset(m_sections, iconResource.dataAddress), iconResource.size});
      dataOffset += iconResource.size;
    } else {
      return false;
    }
  }

  for (const auto& offsetSizePair : resourceOffsetSizePairs) {
    if (!ds.device()->seek(offsetSizePair.first)) {
      return false;
    }
    m_outputDevice->write(ds.device()->read(offsetSizePair.second));
  }

  return true;
}

bool PeExtractor::readVersionInfo() const
{
  if (!m_versionResource.has_value()) {
    return false;
  }

  QDataStream ds{m_inputDevice};
  ds.setByteOrder(QDataStream::LittleEndian);

  if (!ds.device()->seek(addressToOffset(m_sections, m_versionResource->dataAddress))) {
    return false;
  }

  PeVersionInfo versionInfo{};
  ds >> versionInfo;

  QDataStream out{m_outputDevice};

  out << QStringLiteral("%1.%2.%3.%4")
             .arg(versionInfo.FileVersion[0])
             .arg(versionInfo.FileVersion[1])
             .arg(versionInfo.FileVersion[2])
             .arg(versionInfo.FileVersion[3]);

  out << QStringLiteral("%1.%2.%3.%4")
             .arg(versionInfo.ProductVersion[0])
             .arg(versionInfo.ProductVersion[1])
             .arg(versionInfo.ProductVersion[2])
             .arg(versionInfo.ProductVersion[3]);

  return true;
}

PeExtractor::PeExtractor(QIODevice* inputDevice, QIODevice* outputDevice)
    : m_inputDevice(inputDevice), m_outputDevice(outputDevice)
{}

bool ExeUtils::loadIconData(QIODevice* inputDevice, QIODevice* outputDevice)
{
  PeExtractor extractor(inputDevice, outputDevice);

  if (!extractor.readPeData()) {
    return false;
  }

  return extractor.readIcon();
}

bool ExeUtils::loadIconData(const QString& exeFile, QIODevice* outputDevice)
{
  QFile file(exeFile);
  if (!file.open(QIODeviceBase::ReadOnly)) {
    return false;
  }
  return loadIconData(&file, outputDevice);
}

bool ExeUtils::loadVersionData(QIODevice* inputDevice, QIODevice* outputDevice)
{
  PeExtractor extractor(inputDevice, outputDevice);

  if (!extractor.readPeData()) {
    return false;
  }

  return extractor.readVersionInfo();
}

bool ExeUtils::loadVersionData(const QString& exeFile, QIODevice* outputDevice)
{
  QFile file(exeFile);
  if (!file.open(QIODeviceBase::ReadOnly)) {
    return false;
  }
  return loadVersionData(&file, outputDevice);
}
