#pragma once

// modified version of kio-extras exeutils
// source:
// https://invent.kde.org/network/kio-extras/-/blob/master/thumbnail/exeutils.cpp

#include "dllimport.h"
#include "petypes.h"
#include <QMap>
#include <QVector>
#include <optional>

class QDataStream;
class QIODevice;

class QDLLEXPORT PeExtractor
{
public:
  static bool loadIconData(QIODevice* inputDevice, QIODevice* outputDevice);
  static bool loadVersionData(QIODevice* inputDevice, QIODevice* outputDevice);

private:
  explicit PeExtractor(QIODevice* inputDevice, QIODevice* outputDevice);

  QVector<peTypes::PeSection> m_sections;
  QIODevice* m_inputDevice  = nullptr;
  QIODevice* m_outputDevice = nullptr;
  QDataStream m_inputStream;
  peTypes::DosHeader m_dosHeader{};

  QMap<quint32, peTypes::PeResourceDataEntry> m_iconResources;
  std::optional<peTypes::PeResourceDataEntry> m_primaryIconGroupResource;
  std::optional<peTypes::PeResourceDataEntry> m_versionResource;

  bool readPeData();
  bool readIcon();
  bool readVersionInfo();
};
