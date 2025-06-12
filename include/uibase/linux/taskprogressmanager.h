#ifndef TASKPROGRESSMANAGER_H
#define TASKPROGRESSMANAGER_H

#include "../dllimport.h"
#include <QMutexLocker>
#include <QObject>
#include <QTime>
#include <QTimer>
#include <map>

namespace MOBase
{
// source:
// https://stackoverflow.com/questions/43875343/kde-taskbar-progress/43919667#43919667
class QDLLEXPORT TaskProgressManager : QObject
{

  Q_OBJECT

public:
  static TaskProgressManager& instance();

  void forgetMe(quint32 id);
  void updateProgress(quint32 id, qint64 value, qint64 max);

  quint32 getId();

public slots:
  bool tryCreateTaskbar();

private:
  TaskProgressManager();
  void showProgress();

  std::map<quint32, std::pair<QTime, qint64>> m_Percentages;
  QMutex m_Mutex;
  quint32 m_NextId;
  QTimer m_CreateTimer;

  bool m_successful;
};

}  // namespace MOBase
#endif  // TASKPROGRESSMANAGER_H
