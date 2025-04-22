#include "filesystemutilities.h"

#include <QRegularExpression>
#include <QString>

using namespace Qt::StringLiterals;

namespace MOBase
{

bool fixDirectoryName(QString& name)
{
  QString temp = name.simplified();
  while (temp.endsWith('.'))
    temp.chop(1);

  static QRegularExpression exp(uR"([<>:"/\\|?*])"_s);
  temp.replace(exp, "");
  static QString invalidNames[] = {
      u"CON"_s,  u"PRN"_s,  u"AUX"_s,  u"NUL"_s,  u"COM1"_s, u"COM2"_s,
      u"COM3"_s, u"COM4"_s, u"COM5"_s, u"COM6"_s, u"COM7"_s, u"COM8"_s,
      u"COM9"_s, u"LPT1"_s, u"LPT2"_s, u"LPT3"_s, u"LPT4"_s, u"LPT5"_s,
      u"LPT6"_s, u"LPT7"_s, u"LPT8"_s, u"LPT9"_s};
  for (unsigned int i = 0; i < sizeof(invalidNames) / sizeof(QString); ++i) {
    if (temp == invalidNames[i]) {
      temp = "";
      break;
    }
  }

  temp = temp.simplified();

  if (temp.length() >= 1) {
    name = temp;
    return true;
  } else {
    return false;
  }
}

QString sanitizeFileName(const QString& name, const QString& replacement)
{
  QString new_name = name;

  // Remove characters not allowed by Windows
  static QRegularExpression invalidCharacters(uR"([\x{00}-\x{1f}\\/:\*\?"<>|])"_s);
  new_name.replace(invalidCharacters, replacement);

  // Don't end with a period or a space
  // Don't be "." or ".."
  static QRegularExpression exp(uR"([\. ]*$)"_s);
  new_name.remove(exp);

  // Recurse until stuff stops changing
  if (new_name != name) {
    return sanitizeFileName(new_name);
  }
  return new_name;
}

bool validFileName(const QString& name)
{
  if (name.isEmpty()) {
    return false;
  }
  if (name == "."_L1 || name == ".."_L1) {
    return false;
  }

  return (name == sanitizeFileName(name));
}

}  // namespace MOBase
