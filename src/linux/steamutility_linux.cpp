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

  // create the directory name from proton name, e.g.
  // proton_411 -> Proton 4.11
  // proton_5 -> Proton 5.0
  // proton_513 -> Proton 5.13

  QString proton;
  if (protonName.startsWith("proton_"_L1)) {
    // normal proton, installed as steam tool

    // convert "proton_<version>" to "Proton <version>"
    protonName.replace("proton_"_L1, "Proton "_L1);

    static const QRegularExpression regex(u"\\d"_s);
    qsizetype index = protonName.indexOf(regex);
    if (index == -1) {
      log::error("Could not find proton path for appid {}", appID);
      return {};
    }

    // append `.0` if name contains a single digit
    if (index == protonName.size() - 1) {
      protonName.append(".0"_L1);
    } else {
      // insert `.` after first digit
      protonName.insert(index + 1, '.');
    }

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

  if (!prefixPath.exists()) {
    log::error("prefix path {} does not exist", prefixPath.absolutePath());
    return {};
  }

  // fallback for old proton versions
  if (!prefixPath.exists(u"config_info"_s)) {
    log::debug("config_info does not exist, falling back to protonByAppID()");
    return protonByAppID(prefixPath.dirName());
  }

  QFile info(prefixPath.absoluteFilePath(u"config_info"_s));
  if (!info.open(QIODeviceBase::ReadOnly)) {
    log::error("error opening {}, {}", info.fileName(), info.errorString());
    return {};
  }

  // skip the first line
  info.readLine();

  QString result = info.readLine();
  if (!result.endsWith("/files/share/fonts/\n"_L1)) {
    log::error("Error parsing {}", info.fileName());
    return {};
  }

  // remove trailing "files/share/fonts/\n"
  result.chop(19);

  // append "proton"
  result.append(u"proton"_s);

  return result;
}

}  // namespace MOBase
