#include "../steamutility.h"

#include "log.h"
#include "utility.h"

#include <QString>
#include <fstream>
#include <vdf-parser/vdf_parser.hpp>

using namespace Qt::StringLiterals;
using namespace std;

namespace MOBase
{

QString findSteam()
{
  QString home = QDir::homePath();

  // try ~/.local/share/Steam
  QString steam = home % u"/.local/share/Steam"_s;
  if (QFile::exists(steam)) {
    return steam;
  }

  // try ~/.steam/steam
  steam = home % u"/.steam/steam"_s;
  if (QFile::exists(steam)) {
    return steam;
  }

  // try flatpak
  steam = home % u"/.var/app/com.valvesoftware.Steam/.local/share/Steam"_s;
  if (QFile::exists(steam)) {
    return steam;
  }

  return "";
}

std::string getProtonNameByAppID(const QString& appID)
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

    const string message = format("error parsing {}", configPath);
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
      return tmp->attribs.at("name");
    }
    // version is not set manually, use the version from appID 0
    return compatToolMapping->childs.at("0")->attribs.at("name");
  } catch (const out_of_range& ex) {
    log::error("Error getting proton name for appid {}, {}", appID, ex.what());
    return {};
  }
}

// separate function to return from nested loops
QString findInstalledProton(string_view protonName)
{
  for (const auto& library : getAllSteamLibrariesCached()) {
    for (const auto& game : library.games) {
      if (game.name.starts_with(protonName)) {
        QString location = QString::fromStdString(
            (library.path / "steamapps/common" / game.installDir / "proton").string());
        if (QFile::exists(location)) {
          log::debug("found proton location: {}", location);
          return location;
        }
        log::warn("found proton in config, but file {} does not exist", location);
      }
    }
  }
  return {};
}

QString findProtonByAppID(const QString& appID)
{
  QDir steamDir(findSteamCached());
  if (!steamDir.exists()) {
    return {};
  }

  string protonName = getProtonNameByAppID(appID);
  if (protonName.empty()) {
    return {};
  }

  log::debug("Found proton name {}", protonName);

  QString proton;
  if (protonName.starts_with("proton_")) {
    // normal proton, installed as steam tool

    // convert "proton_<version>" to "Proton <version>"
    protonName.replace(0, strlen("proton_"), "Proton ");

    proton = findInstalledProton(protonName);
  } else {
    // custom proton e.g. GE-Proton9-25, located in <steamDir>/compatibilitytools.d/
    proton = steamDir.absolutePath() % u"/compatibilitytools.d/"_s %
             QString::fromStdString(protonName) % u"/proton"_s;
    if (!QFile::exists(proton)) {
      log::error("Detected proton path \"{}\" does not exist", proton);
      return {};
    }
  }
  log::debug("Proton found at {}", proton);
  return proton;
}

QString findCompatDataByAppID(const QString& appID)
{
  QDir steamDir(findSteamCached());
  if (!steamDir.exists()) {
    return {};
  }

  for (const auto& library : getAllSteamLibrariesCached()) {
    for (const auto& game : library.games) {
      if (appID.toStdString() == game.appID) {
        filesystem::path compatDataPath = library.path / "steamapps/common" /
                                          game.installDir / "../../compatdata" /
                                          appID.toStdString();
        // clean up path
        compatDataPath = canonical(compatDataPath);
        log::debug("found compatdata for appID {}: {}", appID, compatDataPath.string());
        return QString::fromStdString(absolute(compatDataPath).string());
      }
    }
  }
  return {};
}

}  // namespace MOBase
