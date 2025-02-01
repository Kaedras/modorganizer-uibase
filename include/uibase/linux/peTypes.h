#pragma once

#include <QtTypes>
#include <QChar>

namespace peTypes
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
}  // namespace peTypes
