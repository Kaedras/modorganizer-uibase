#include "executableinfo.h"

using namespace MOBase;

ExecutableInfo& ExecutableInfo::withPrefixDirectory(const QDir& prefixDirectory)
{
  m_PrefixDirectory = prefixDirectory;
  return *this;
}

ExecutableInfo& ExecutableInfo::withSteamAPI()
{
  m_EnableSteamAPI = true;
  return *this;
}

ExecutableInfo& ExecutableInfo::withSteamOverlay()
{
  m_EnableSteamOverlay = true;
  return *this;
}

QDir ExecutableInfo::prefixDirectory() const
{
  return m_PrefixDirectory;
}

bool ExecutableInfo::enableSteamAPI() const
{
  return m_EnableSteamAPI;
}

bool ExecutableInfo::enableSteamOverlay() const
{
  return m_EnableSteamOverlay;
}
