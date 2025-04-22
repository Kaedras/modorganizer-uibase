/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "nxmurl.h"
#include "utility.h"
#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

NXMUrl::NXMUrl(const QString& url)
{
  QUrl nxm(url);
  QUrlQuery query(nxm);
  static QRegularExpression exp(u"nxm://[a-z0-9]+/mods/(\\d+)/files/(\\d+)"_s,
                                QRegularExpression::CaseInsensitiveOption);
  auto match = exp.match(url);
  if (!match.hasMatch()) {
    throw MOBase::InvalidNXMLinkException(url);
  }
  m_Game    = nxm.host();
  m_ModId   = match.captured(1).toInt();
  m_FileId  = match.captured(2).toInt();
  m_Key     = query.queryItemValue(u"key"_s);
  m_Expires = query.queryItemValue(u"expires"_s).toInt();
  m_UserId  = query.queryItemValue(u"user_id"_s).toInt();
}
