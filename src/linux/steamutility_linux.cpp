#include "../steamutility.h"

#include <QSettings>
#include <QString>

// undefine signals from qtmetamacros.h because it conflicts with glib
#ifdef signals
#undef signals
#endif
#include <flatpak/flatpak.h>

using namespace Qt::StringLiterals;

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

}  // namespace MOBase