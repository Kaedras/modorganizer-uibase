#include "linux/peextractor.h"
#include "linux/petypes.h"

#include <QDataStream>
#include <QFile>
#include <QIODevice>
#include <QString>
#include <QVector>

using namespace peTypes;

namespace
{
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
          m_iconResources[entry2.resourceId] = dataEntry;
          break;

        case ResourceType::GroupIcon:
          if (!m_primaryIconGroupResource.has_value()) {
            m_primaryIconGroupResource = dataEntry;
          }

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
  if (!m_primaryIconGroupResource.has_value()) {
    return false;
  }

  QDataStream ds{m_inputDevice};
  ds.setByteOrder(QDataStream::LittleEndian);

  if (!ds.device()->seek(
          addressToOffset(m_sections, m_primaryIconGroupResource->dataAddress))) {
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
    out << icoFileEntry;

    if (auto it = m_iconResources.find(entry.resourceId); it != m_iconResources.end()) {
      PeResourceDataEntry iconResource = *it;
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

bool PeExtractor::loadIconData(QIODevice* inputDevice, QIODevice* outputDevice)
{
  PeExtractor extractor(inputDevice, outputDevice);

  if (!extractor.readPeData()) {
    return false;
  }

  return extractor.readIcon();
}

bool PeExtractor::loadIconData(const QString& exeFile, QIODevice* outputDevice)
{
  QFile file(exeFile);
  if (!file.open(QIODeviceBase::ReadOnly)) {
    return false;
  }
  return loadIconData(&file, outputDevice);
}

bool PeExtractor::loadVersionData(QIODevice* inputDevice, QIODevice* outputDevice)
{
  PeExtractor extractor(inputDevice, outputDevice);

  if (!extractor.readPeData()) {
    return false;
  }

  return extractor.readVersionInfo();
}

bool PeExtractor::loadVersionData(const QString& exeFile, QIODevice* outputDevice)
{
  QFile file(exeFile);
  if (!file.open(QIODeviceBase::ReadOnly)) {
    return false;
  }
  return loadVersionData(&file, outputDevice);
}
