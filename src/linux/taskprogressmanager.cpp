#include "taskprogressmanager.h"

#include "log.h"
#include <QDBusConnection>
#include <QDBusMessage>

namespace MOBase
{
TaskProgressManager& TaskProgressManager::instance()
{
  static TaskProgressManager s_Instance;
  return s_Instance;
}

void TaskProgressManager::forgetMe(quint32 id)
{
  if (!m_successful) {
    return;
  }
  auto iter = m_Percentages.find(id);
  if (iter != m_Percentages.end()) {
    m_Percentages.erase(iter);
  }
  showProgress();
}

void TaskProgressManager::updateProgress(quint32 id, qint64 value, qint64 max)
{
  QMutexLocker lock(&m_Mutex);
  if (!m_successful) {
    return;
  }
  if (value == max) {
    auto iter = m_Percentages.find(id);
    if (iter != m_Percentages.end()) {
      m_Percentages.erase(iter);
    }
  } else {
    m_Percentages[id] = std::make_pair(QTime::currentTime(), (value * 100) / max);
  }

  showProgress();
}

quint32 TaskProgressManager::getId()
{
  QMutexLocker lock(&m_Mutex);
  return m_NextId++;
}

bool TaskProgressManager::tryCreateTaskbar()
{
  auto message = QDBusMessage::createSignal(
      QStringLiteral("/org/ModOrganizer2/ModOrganizer2"),
      QStringLiteral("com.canonical.Unity.LauncherEntry"), QStringLiteral("Update"));

  QVariantMap properties;
  properties.insert(QStringLiteral("progress-visible"), false);
  message << QStringLiteral("application://") + QGuiApplication::desktopFileName()
          << properties;
  return QDBusConnection::sessionBus().send(message);
}

void TaskProgressManager::showProgress()
{
  auto message = QDBusMessage::createSignal(
      QStringLiteral("/org/ModOrganizer2/ModOrganizer2"),
      QStringLiteral("com.canonical.Unity.LauncherEntry"), QStringLiteral("Update"));

  QVariantMap properties;
  if (!m_Percentages.empty()) {
    properties.insert(QStringLiteral("progress-visible"), true);  // enable the progress

    QTime now                = QTime::currentTime();
    unsigned long long total = 0;
    unsigned long long count = 0;

    for (auto iter = m_Percentages.begin(); iter != m_Percentages.end();) {
      if (iter->second.first.secsTo(now) < 15) {
        total += static_cast<unsigned long long>(iter->second.second);
        ++iter;
        ++count;
      } else {
        // if there was no progress in 15 seconds remove this progress
        log::debug("no progress in 15 seconds ({})", iter->second.first.secsTo(now));
        iter = m_Percentages.erase(iter);
      }
    }
    log::debug("setting progress to {}", total / (count * 100));
    properties.insert(QStringLiteral("progress"),
                      total /
                          (count * 100));  // set the progress value (from 0.0 to 1.0)
  } else {
    properties.insert(QStringLiteral("progress-visible"), false);
  }

  message << QStringLiteral("application://") + QGuiApplication::desktopFileName()
          << properties;
  bool result = QDBusConnection::sessionBus().send(message);
  if (!result) {
    log::warn("failed to set progress");
  }
}

TaskProgressManager::TaskProgressManager() : m_NextId(1)
{
  if (QGuiApplication::desktopFileName().isEmpty()) {
    log::warn("MO2 has no desktop file name, task bar progress not available");
    m_successful = false;
    return;
  }
  m_successful = true;
}

}  // namespace MOBase
