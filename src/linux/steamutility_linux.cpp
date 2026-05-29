#include "../steamutility.h"

#include "log.h"
#include "utility.h"
#include <vdf_parser.hpp>

using namespace Qt::StringLiterals;
using namespace std;

namespace MOBase
{

QString findSteam()
{
  QString home = QDir::homePath();

  static const QStringList paths = {
      home % "/.local/share/Steam"_L1, home % "/.steam/steam"_L1,
      home % "/.var/app/com.valvesoftware.Steam/.local/share/Steam"_L1};

  for (const auto& path : paths) {
    if (QFile::exists(path)) {
      return path;
    }
  }

  return "";
}

QString protonNameByAppID(const QString& appID)
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
      return QString::fromStdString(tmp->attribs.at("name"));
    }
    // version is not set manually, use the version from appID 0
    return QString::fromStdString(
        compatToolMapping->childs.at("0")->attribs.at("name"));
  } catch (const out_of_range& ex) {
    log::error("Error getting proton name for appid {}, {}", appID, ex.what());
    return {};
  }
}

QString protonByAppID(const QString& appID)
{
  QDir steamDir(findSteamCached());
  if (!steamDir.exists()) {
    return {};
  }

  QString protonName = protonNameByAppID(appID);
  if (protonName.isEmpty()) {
    return {};
  }

  log::debug("Found proton name {}", protonName);

  QString proton;
  if (protonName.startsWith("proton_"_L1)) {
    // normal proton, installed as steam tool

    // convert "proton_<version>" to "Proton <version>"
    protonName.replace("proton_"_L1, "Proton "_L1);

    proton = findSteamGame(protonName, u"proton"_s);
  } else {
    // custom proton e.g. GE-Proton9-25, located in <steamDir>/compatibilitytools.d/
    proton = steamDir.absolutePath() % u"/compatibilitytools.d/"_s % protonName %
             u"/proton"_s;
    if (!QFile::exists(proton)) {
      log::error("Detected proton path \"{}\" does not exist", proton);
      return {};
    }
  }
  log::debug("Proton found at {}", proton);
  return proton;
}

QString protonByPrefixPath(const QDir& prefixPath)
{
  // read proton path from <prefix>/config_info

  QFile info(prefixPath.absoluteFilePath(u"config_info"_s));
  if (!info.open(QIODeviceBase::ReadOnly)) {
    log::error("error opening {}, {}", info.fileName(), info.errorString());
    return {};
  }

  // skip the first line
  info.readLine();

  QString result = info.readLine();

  // remove trailing "files/share/fonts/\n"
  result.chop(19);

  // append "proton"
  result.append(u"proton"_s);

  return result;
}

}  // namespace MOBase
