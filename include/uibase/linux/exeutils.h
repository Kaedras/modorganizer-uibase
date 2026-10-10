#ifndef EXE_UTILS_H
#define EXE_UTILS_H

// modified version of kio-extras exeutils
// source:
// https://invent.kde.org/network/kio-extras/-/blob/master/thumbnail/exeutils.h
// https://invent.kde.org/network/kio-extras/-/blob/master/thumbnail/exeutils.cpp

#include "dllimport.h"

class QIODevice;

namespace ExeUtils
{

/**
 * @brief Extracts the primary icon contained in the provided PE file.
 * @param inputDevice Input device to read from.
 * @param outputDevice Output device to write to.
 * @return True on success, false on error.
 */
bool QDLLEXPORT loadIconData(QIODevice* inputDevice, QIODevice* outputDevice);
/**
 * @brief Extracts the primary icon contained in the provided PE file.
 * @param exeFile File to extract data from.
 * @param outputDevice Output device to write to.
 * @return True on success, false on error.
 */
bool QDLLEXPORT loadIconData(const QString& exeFile, QIODevice* outputDevice);
/**
 * @brief Reads version information from a PE file.
 * @param inputDevice Input device to read from.
 * @param outputDevice Output device to write to. It will contain the file version and
 * product version.
 * @return True on success, false on error.
 */
bool QDLLEXPORT loadVersionData(QIODevice* inputDevice, QIODevice* outputDevice);
/**
 * @brief Reads version information from a PE file.
 * @param exeFile File to extract data from.
 * @param outputDevice Output device to write to. It will contain the file version and
 * product version.
 * @return True on success, false on error.
 */
bool QDLLEXPORT loadVersionData(const QString& exeFile, QIODevice* outputDevice);

}  // namespace ExeUtils

#endif
