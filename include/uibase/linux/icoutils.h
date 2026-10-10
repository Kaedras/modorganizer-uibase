/*
  icoutils.h - Extract Microsoft Window icons and images

  SPDX-FileCopyrightText: 2009-2010 Pali Rohár <pali.rohar@gmail.com>

  SPDX-FileCopyrightText: 2013 Andrius da Costa Ribas <andriusmao@gmail.com>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

// source: https://invent.kde.org/network/kio-extras

#ifndef ICO_UTILS_H
#define ICO_UTILS_H

#include "dllimport.h"
#include <QtGlobal>

class QString;
class QIODevice;
class QImage;
class QImageReader;

namespace IcoUtils
{

bool QDLLEXPORT loadIcoImageFromExe(QIODevice* inputDevice, QImage& image,
                                    int needWidth = 512, int needHeight = 512);
bool QDLLEXPORT loadIcoImageFromExe(const QString& inputFileName, QImage& image,
                                    int needWidth = 512, int needHeight = 512);

bool QDLLEXPORT loadIcoImage(QIODevice* inputDevice, QImage& image, int needWidth = 512,
                             int needHeight = 512);
bool QDLLEXPORT loadIcoImage(const QString& inputFileName, QImage& image,
                             int needWidth = 512, int needHeight = 512);
bool QDLLEXPORT loadIcoImage(QImageReader& reader, QImage& image, int needWidth,
                             int needHeight);

}  // namespace IcoUtils

#endif  // ICO_UTILS_H
