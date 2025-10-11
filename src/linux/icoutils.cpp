/*
  icoutils.cpp - Extract Microsoft Window icons and images

  SPDX-FileCopyrightText: 2009-2010 Pali Rohár <pali.rohar@gmail.com>

  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "linux/icoutils.h"
#include "linux/peextractor.h"

#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QList>
#include <QString>

#include <algorithm>

namespace
{

qreal distance(int width, int height, int desiredWidth, int desiredHeight, int depth)
{
  // We want as high of a depth as possible (32-bit)
  auto targetSamples = desiredWidth * desiredHeight * 32;
  auto xscale        = (1.0 * desiredWidth) / width;
  auto yscale        = (1.0 * desiredHeight) / height;

  // clamp to the lower of the two scales
  // also clamp to one, as scaling up adds no effective
  // samples, only interpolated samples
  auto sampleScale = std::min(1.0, std::min(xscale, yscale));

  // number of effective source samples in the target
  auto effectiveSamples = width * height * sampleScale * sampleScale * depth;
  // scale down another time, to account for loss of fidelity when
  // using a downscaled image, biases towards smaller downscaling ratios
  effectiveSamples *= sampleScale;

  return targetSamples - effectiveSamples;
}

}  // namespace

bool IcoUtils::loadIcoImageFromExe(QIODevice* inputDevice, QImage& image, int needWidth,
                                   int needHeight)
{
  QBuffer iconData;
  if (!iconData.open(QIODevice::ReadWrite)) {
    return false;
  }

  if (!PeExtractor::loadIconData(inputDevice, &iconData))
    return false;

  if (!iconData.seek(0)) {
    return false;
  }

  return IcoUtils::loadIcoImage(&iconData, image, needWidth, needHeight);
}

bool IcoUtils::loadIcoImageFromExe(const QString& inputFileName, QImage& image,
                                   int needWidth, int needHeight)
{
  QFile inputFile{inputFileName};

  if (!inputFile.open(QIODevice::ReadOnly)) {
    return false;
  }

  return IcoUtils::loadIcoImageFromExe(&inputFile, image, needWidth, needHeight);
}

bool IcoUtils::loadIcoImage(QImageReader& reader, QImage& image, int needWidth,
                            int needHeight)
{
  // QTBUG-70812: for files with incorrect bits per pixel, QImageReader::canRead()
  // returns false but it can still correctly determine the imageCount() and read the
  // icon just fine.
  if (reader.imageCount() == 0) {
    return false;
  }

  QList<QImage> icons;
  do
    icons << reader.read();
  while (reader.jumpToNextImage());

  if (icons.empty())
    return false;

  int index  = icons.size() - 1;
  qreal best = std::numeric_limits<qreal>::max();

  for (int i = 0; i < icons.size(); ++i) {
    const QImage& icon = icons.at(i);

    // QtIcoHandler converts all images to 32-bit depth,
    // but it stores the actual depth of the icon extracted in custom text:
    // qtbase/src/plugins/imageformats/ico/qicohandler.cpp:455
    int depth = icon.text(QStringLiteral("_q_icoOrigDepth")).toInt();
    if (depth == 0 || depth > 32) {
      depth = icon.depth();
    }

    const qreal dist =
        distance(icon.width(), icon.height(), needWidth, needHeight, depth);

    if (dist < best) {
      index = i;
      best  = dist;
    }
  }

  image = icons.at(index);
  return true;
}

bool IcoUtils::loadIcoImage(QIODevice* inputDevice, QImage& image, int needWidth,
                            int needHeight)
{
  QImageReader reader(inputDevice, "ico");
  return IcoUtils::loadIcoImage(reader, image, needWidth, needHeight);
}

bool IcoUtils::loadIcoImage(const QString& inputFileName, QImage& image, int needWidth,
                            int needHeight)
{
  QImageReader reader(inputFileName, "ico");
  return IcoUtils::loadIcoImage(reader, image, needWidth, needHeight);
}
