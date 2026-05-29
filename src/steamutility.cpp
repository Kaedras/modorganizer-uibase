/*
Mod Organizer shared UI functionality

Copyright (C) 2019 MO2 Contributors. All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "steamutility.h"

#include "log.h"
#include "utility.h"

#include <QDir>
#include <QDirIterator>
#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QTextStream>
#include <fstream>
#include <ranges>
#include <vdf_parser.hpp>

using namespace Qt::StringLiterals;

namespace MOBase
{

// Lines that contains libraries are in the format:
//    "1" "Path\to\library"
static const QRegularExpression
    kSteamLibraryFilter("^\\s*\"(?<idx>[0-9]+)\"\\s*\"(?<path>.*)\"");

QString findSteamCached()
{
  static const QString steam = findSteam();
  return steam;
}

QString findSteamGame(const QString& appName, const QString& validFile)
{
  QStringList libraryFolders;  // list of Steam libraries to search
  QDir steamDir(findSteam());  // Steam installation directory

  // Can do nothing if Steam doesn't exist
  if (!steamDir.exists())
    return "";

  // The Steam install is always a valid library
  libraryFolders << steamDir.absolutePath();

  // Search libraryfolders.vdf for additional libraries
  QFile libraryFoldersFile(steamDir.absoluteFilePath("steamapps/libraryfolders.vdf"));
  if (libraryFoldersFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream in(&libraryFoldersFile);
    while (!in.atEnd()) {
      QString line                  = in.readLine();
      QRegularExpressionMatch match = kSteamLibraryFilter.match(line);
      if (match.hasMatch()) {
        QString folder = match.captured("path");
        folder.replace("/", "\\").replace("\\\\", "\\");
        libraryFolders << folder;
      }
    }
  }

  // Search the Steam libraries for the game directory
  for (auto library : libraryFolders) {
    QDir libraryDir(library);
    if (!libraryDir.cd("steamapps/common/" + appName))
      continue;
    if (validFile.isEmpty() || libraryDir.exists(validFile))
      return libraryDir.absolutePath();
  }

  return "";
}

QString appIdByGamePath(const QString& gameLocation)
{
  log::debug("Looking up appID for game path {}", gameLocation);

  // check for steam_appid.txt inside the game directory
  // according to the steamworks documentation, applications should not ship this file,
  // but some developers do this anyway
  QFile steamAppIdFile(gameLocation % "/steam_appid.txt"_L1);
  if (steamAppIdFile.exists()) {
    if (steamAppIdFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QTextStream in(&steamAppIdFile);
      QString appId = in.readLine();
      log::debug("Found appID {}", appId);
      return appId;
    }
  }

  // get steamApps directory for the library the game is located in
  QString steamAppsPath    = gameLocation;
  qsizetype commonPosition = steamAppsPath.indexOf(u"common"_s, Qt::CaseInsensitive);
  // remove everything from "common" to end, including "common"
  steamAppsPath.truncate(commonPosition);

  // get the game installation path in steamapps/common/
  QString installPath = gameLocation;

  // remove everything from beginning to common/
  installPath.remove(0, commonPosition + QStringLiteral("common/").size());

  // remove everything from the first slash to end in case gameLocation is a
  // subdirectory
  auto pos = installPath.indexOf('/');
  if (pos != -1) {
    installPath.truncate(pos);
  }

  // iterate over app manifests
  QDirIterator it(steamAppsPath, QStringList(u"appmanifest_*.acf"_s), QDir::Files);
  while (it.hasNext()) {
    QString item = it.next();

    // open a manifest file
    std::ifstream file(item.toStdString());
    if (!file.is_open()) {
      const int e = errno;
      log::warn("Error opening manifest file {}, {}", item.toStdString(), strerror(e));
      return {};
    }

    // read the manifest file
    auto root              = tyti::vdf::read(file);
    std::string installdir = root.attribs["installdir"];
    if (installdir.empty()) {
      log::error("Error parsing appmanifest: installdir not found");
      return {};
    }

    // compare installation paths
    if (installPath.toStdString() == installdir) {
      QString appID = QString::fromStdString(root.attribs["appid"]);
      log::debug("Found appID {}", appID);
      return appID;
    }
  }

  log::error("Error getting appID for path {}", gameLocation);
  return {};
}

}  // namespace MOBase
