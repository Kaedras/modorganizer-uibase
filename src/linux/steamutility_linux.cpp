#include "../steamutility.h"

#include "log.h"
#include "utility.h"

#include <QString>
#include <fstream>
#include <vdf-parser/vdf_parser.hpp>

// undefine signals from qtmetamacros.h because it conflicts with glib
#ifdef signals
#undef signals
#endif

#include <flatpak/flatpak.h>

using namespace Qt::StringLiterals;
using namespace std;

namespace MOBase
{

QString findSteam()
{
  // try ~/.local/share/Steam
  QString steam = QDir::homePath() % u"/.local/share/Steam"_s;
  if (QFile::exists(steam)) {
    return steam;
  }

  // try ~/.steam/steam
  steam = QDir::homePath() % u"/.steam/steam"_s;
  if (QFile::exists(steam)) {
    return steam;
  }

  // try flatpak
  GError* e                         = nullptr;
  FlatpakInstallation* installation = flatpak_installation_new_user(nullptr, &e);
  if (e != nullptr) {
    g_error_free(e);
    return {};
  }

  FlatpakInstalledRef* flatpakInstalledRef = flatpak_installation_get_installed_ref(
      installation, FLATPAK_REF_KIND_APP, "com.valvesoftware.Steam", nullptr, "stable",
      nullptr, &e);
  if (e != nullptr || flatpakInstalledRef == nullptr) {
    if (e != nullptr) {
      g_error_free(e);
    }
    return {};
  }

  return QString::fromLocal8Bit(
      flatpak_installed_ref_get_deploy_dir(flatpakInstalledRef));
}

std::string getProtonNameByAppID(const QString& appID)
{
  // proton versions can be parsed from ~/.local/Steam/config/config.vdf
  // InstallConfigStore -> Software -> Valve -> Steam -> CompatToolMapping
  try {
    QDir steamDir(findSteamCached());
    if (!steamDir.exists()) {
      return {};
    }

    // todo: add proper support for flatpak installations, config file path may be
    // different
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
    // according to ProtonUp-Qt source code the key can either be "Valve" or "valve"
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
    // version not set manually, use version from appID 0
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
          log::debug("found proton: {}", location);
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
    // custom proton e.g. GE-Proton9-25, located in steam/compatibilitytools.d/
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
      if (appID == game.appID) {
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