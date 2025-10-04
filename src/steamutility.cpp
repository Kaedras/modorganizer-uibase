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
#include <vdf-parser/vdf_parser.hpp>

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

std::vector<Steam::Library> getAllSteamLibraries()
{
  TimeThis tt(QStringLiteral("getAllSteamLibraries()"));

  QDir steamDir(findSteamCached());
  if (!steamDir.exists()) {
    return {};
  }

  std::vector<Steam::Library> libraries;

  std::ifstream libraryFoldersFile(steamDir.filesystemAbsolutePath() /
                                   "config/libraryfolders.vdf");
  if (!libraryFoldersFile.is_open()) {
    log::error("Error opening libraryfolders.vdf");
    return {};
  }

  auto root = tyti::vdf::read(libraryFoldersFile);

  // iterate over libraries
  for (const auto& library : root.childs | std::views::values) {
    // skip empty libraries
    if (library->childs["apps"] == nullptr ||
        library->childs["apps"]->attribs.empty()) {
      continue;
    }

    Steam::Library tmp;
    tmp.path = library->attribs.at("path");

    // iterate over keys in apps
    for (const auto& appID : library->childs["apps"]->attribs | std::views::keys) {
      Steam::Game game;
      game.appID = appID;

      // open a manifest file
      std::filesystem::path manifestPath = tmp.path / "steamapps/appmanifest_";
      manifestPath += game.appID + ".acf";
      std::ifstream manifestFile(manifestPath);
      if (!manifestFile.is_open()) {
        // steam may not have cleaned up
        const int e = errno;
        log::warn("Error opening manifest file {}, {}", manifestPath.generic_string(),
                  strerror(e));
        continue;
      }

      // read the manifest file
      auto manifest = tyti::vdf::read(manifestFile);
      try {
        game.name       = manifest.attribs.at("name");
        game.installDir = manifest.attribs.at("installdir");
      } catch (const std::out_of_range& ex) {
        log::error("out_of_range exception while parsing manifest file {}, key {} does "
                   "not exist",
                   manifestPath.generic_string(),
                   !manifest.attribs.contains("name") ? "name" : "installdir");
        return {};
      }
      tmp.games.push_back(game);
    }
    libraries.push_back(tmp);
  }
  return libraries;
}

std::vector<Steam::Library> getAllSteamLibrariesCached()
{
  static const std::vector<Steam::Library> libraries = getAllSteamLibraries();
  return libraries;
}

std::vector<Steam::Game> getAllSteamGames()
{
  std::vector<Steam::Game> games;
  QDir steamDir(findSteamCached());
  if (!steamDir.exists()) {
    return games;
  }

  std::vector<Steam::Library> libraries = getAllSteamLibrariesCached();
  for (auto& library : libraries) {
    games.reserve(games.size() + library.games.size());
    for (const auto& game : library.games) {
      games.emplace_back(game);
    }
  }
  return games;
}

std::vector<Steam::Game> getAllSteamGamesCached()
{
  static const std::vector<Steam::Game> games = getAllSteamGames();
  return games;
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
