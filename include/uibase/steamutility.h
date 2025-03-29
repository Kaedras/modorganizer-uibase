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

#ifndef STEAMUTILITY_H
#define STEAMUTILITY_H

#include "dllimport.h"
#include <QString>

namespace MOBase
{

namespace Steam
{
  struct QDLLEXPORT Game
  {
    std::string name;
    std::filesystem::path installDir;  // directory name in steamapps/common/
    std::string appID;
  };

  struct QDLLEXPORT Library
  {
    std::filesystem::path path;
    std::vector<Game> games;
  };
}  // namespace Steam

/**
 * @brief Gets the installation path to Steam according to the registy
 **/
QDLLEXPORT QString findSteam();

/**
 * @brief Gets the installation path to Steam according to the registy and caches it
 * for faster subsequent calls.
 * @return
 */
QDLLEXPORT QString findSteamCached();

/**
 * @brief Gets the installation path to a Steam game
 *
 * @param appName The Steam application name, i.e., the expected steamapps folder name
 * @param validFile If the given file exists in the found installation path, the game is
          consider to be valid.  May be blank.
 **/
QDLLEXPORT QString findSteamGame(const QString& appName, const QString& validFile);

/**
 * @brief Gets a list of all Steam libraries.
 */
QDLLEXPORT std::vector<Steam::Library> getAllSteamLibraries();

/**
 * @brief Gets a list of all Steam libraries and caches it for faster subsequent calls.
 */
QDLLEXPORT std::vector<Steam::Library> getAllSteamLibrariesCached();

/**
 * @brief Gets a list of all installed Steam games.
 */
QDLLEXPORT std::vector<Steam::Game> getAllSteamGames();

/**
 * @brief Gets the appID of the game located in the specified location by parsing
 * ../../appmanifest_*.acf
 * @param gameLocation Location of game
 * @return Steam appID of game in specified location
 */
QString appIdByGamePath(const QString& gameLocation);

// additional functions for linux, proton specific
#ifdef __unix__

/**
 * @brief Gets the proton name that is configured for the specified appID.
 * @param appID Steam appID of application
 * @return Proton name, e.g. proton_9
 */
QDLLEXPORT std::string getProtonNameByAppID(const QString& appID);

/**
 * @brief Gets proton executable for specified appID
 * @param appID Steam appID of application
 * @return Absolute path of proton executable, empty string if not found
 */
QDLLEXPORT QString findProtonByAppID(const QString& appID);

/**
 * @brief Returns path of compat data directory for the specified appID
 * @param appID Steam appID of application
 * @return Absolute path to compat data, empty string if not found
 */
QDLLEXPORT QString findCompatDataByAppID(const QString& appID);

#endif  // __unix__
}  // namespace MOBase

#endif  // STEAMUTILITY_H
