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
#include <QDir>
#include <QString>

namespace MOBase
{

/**
 * @brief Gets the installation path to Steam according to the registy
 **/
QDLLEXPORT QString findSteam();

/**
 * @brief Gets the installation path to Steam according to the registy and caches it
 * for faster subsequent calls.
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
 * @brief Gets the appID of the game located in the specified location by parsing
 * "../../appmanifest_*.acf"
 * @param gameLocation Location of game
 * @return Steam appID of game in the specified location
 */
QDLLEXPORT QString appIdByGamePath(const QString& gameLocation);

#ifdef __unix__
// proton and linux-specific functions

/**
 * @brief Gets the proton executable for the specified prefix path
 * @param prefixPath Prefix path of the application
 * @return Absolute path of proton executable, empty string if not found
 * @note This does not work with very old proton versions e.g. proton 4
 */
QDLLEXPORT QString protonByPrefixPath(const QDir& prefixPath);

/**
 * @brief Gets the linux runtime that should be used for the specified game.
 * @details Reads `Steam/appcache/appconfig.vdf` and
 * `<gameLocation>/../../appmanifest_<appID>.vdf` to determine the linux runtime to use
 * for the specified game. Retrieving this dynamically is required because game updates
 * and beta/legacy branches can require different runtimes.
 * @note This is only relevant for native linux games.
 * @throws std::runtime_error
 */
QDLLEXPORT QString getRequiredLinuxRuntime(const QString& gameLocation,
                                           const QString& appID) noexcept(false);

#endif  // __unix__

}  // namespace MOBase

#endif  // STEAMUTILITY_H
