#include "steamutility.h"

#include <QSettings>
#include <QString>

namespace MOBase
{

QString findSteam()
{
  QSettings steamRegistry("Valve", "Steam");
  return steamRegistry.value("SteamPath").toString();
}

}  // namespace MOBase
