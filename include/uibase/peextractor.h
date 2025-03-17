#pragma once

// modified version of kio-extras exeutils
// source:
// https://invent.kde.org/network/kio-extras/-/blob/master/thumbnail/exeutils.cpp

#include "dllimport.h"
#include "petypes.h"
#include <QDataStream>
#include <QMap>
#include <QVector>
#include <optional>

class QDataStream;
class QIODevice;

class QDLLEXPORT PeExtractor
{
public:
  /**
   * @brief Extracts the primary icon contained in the provided PE file.
   * @param inputDevice Input device to read from.
   * @param outputDevice Output device to write to.
   * @return True on success, false on error.
   */
  static bool loadIconData(QIODevice* inputDevice, QIODevice* outputDevice);
  /**
   * @brief Extracts the primary icon contained in the provided PE file.
   * @param exeFile File to extract data from.
   * @param outputDevice Output device to write to.
   * @return True on success, false on error.
   */
  static bool loadIconData(const QString& exeFile, QIODevice* outputDevice);
  /**
   * @brief Reads version information from a PE file.
   * @param inputDevice Input device to read from.
   * @param outputDevice Output device to write to. It will contain file version and
   * product version.
   * @return True on success, false on error.
   */
  static bool loadVersionData(QIODevice* inputDevice, QIODevice* outputDevice);
  /**
   * @brief Reads version information from a PE file.
   * @param exeFile File to extract data from.
   * @param outputDevice Output device to write to. It will contain file version and
   * product version.
   * @return True on success, false on error.
   */
  static bool loadVersionData(const QString& exeFile, QIODevice* outputDevice);

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
  bool readVersionInfo() const;
};
