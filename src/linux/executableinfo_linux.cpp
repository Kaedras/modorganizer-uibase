#include "executableinfo.h"

using namespace MOBase;

ExecutableInfo& ExecutableInfo::withPrefixDirectory(const QDir& prefixDirectory)
{
  m_PrefixDirectory = prefixDirectory;
  return *this;
}

ExecutableInfo& ExecutableInfo::withSteamAPI(bool enabled)
{
  m_EnableSteamAPI = enabled;
  return *this;
}

ExecutableInfo& ExecutableInfo::withSteamOverlay(bool enabled)
{
  m_EnableSteamOverlay = enabled;
  return *this;
}

QDir ExecutableInfo::prefixDirectory() const
{
  return m_PrefixDirectory;
}

std::optional<bool> ExecutableInfo::enableSteamAPI() const
{
  return m_EnableSteamAPI;
}

std::optional<bool> ExecutableInfo::enableSteamOverlay() const
{
  return m_EnableSteamOverlay;
}
