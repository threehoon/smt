#pragma once

#include "model/program.h"
#include "validation/diagnostic.h"

#include <QCoreApplication>
#include <QVector>

// Consistency rules of docs/program-format.md 5.1 and 5.2 that can be checked
// on an in-memory Program: id uniqueness, references, value ranges, at least
// one board instance. ProgramReader runs it after parsing and the editor runs
// it before accepting a change, so anything that can be saved can be opened.
//
// Syntax-level checks (XML, required attributes, number / enum spelling,
// sections) stay in ProgramReader; they cannot fail on a Program in memory.
// Diagnostics carry a location and no line.
class ProgramValidator
{
    Q_DECLARE_TR_FUNCTIONS(ProgramValidator)

public:
    static QVector<Diagnostic> validate(const Program &program);
};
