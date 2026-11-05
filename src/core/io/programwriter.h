#pragma once

#include "model/program.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QString>

// Writes a program in the current formatVersion (docs/program-format.md).
// Does not validate: the caller writes what the user has, even if incomplete.
class ProgramWriter
{
    Q_DECLARE_TR_FUNCTIONS(ProgramWriter)

public:
    static QByteArray toXml(const Program &program);

    // Writes through QSaveFile, so a crash or a full disk never leaves a
    // half-written program behind. Returns false and fills errorMessage on failure.
    static bool writeFile(const Program &program, const QString &path,
                          QString *errorMessage = nullptr);
};
