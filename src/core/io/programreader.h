#pragma once

#include "model/program.h"
#include "validation/diagnostic.h"

#include <QCoreApplication>
#include <QString>
#include <QVector>

class QIODevice;

struct ReadResult
{
    Program program;  // only meaningful when ok()
    QVector<Diagnostic> diagnostics;
    // File was written by a newer minor version: it opens, but content this
    // version does not know was dropped and saving would lose it.
    bool newerMinorVersion = false;

    bool ok() const;
    QVector<Diagnostic> errors() const;
    QVector<Diagnostic> warnings() const;
};

// Reads a program file and performs the open-time checks of
// docs/program-format.md section 5.2: syntax checks while parsing, then
// ProgramValidator on the result, with its diagnostics mapped to file lines.
// Run-time checks (feeders available, nozzle compatibility) are not done here.
class ProgramReader
{
    Q_DECLARE_TR_FUNCTIONS(ProgramReader)

public:
    static ReadResult read(QIODevice *device);
    static ReadResult read(const QByteArray &data);
    static ReadResult readFile(const QString &path);
};
