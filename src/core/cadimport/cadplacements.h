#pragma once

#include "cadimport/cadcolumns.h"
#include "cadimport/cadtable.h"
#include "model/placement.h"

#include <QString>
#include <QVector>

// One data row of a CAD placement file, converted to board coordinates.
struct CadPlacement
{
    int line = 0;  // 1-based line in the file
    QString designator;
    double x = 0.0;  // mm, relative to the board origin
    double y = 0.0;
    double rotation = 0.0;  // degrees in [0, 360)
    Side side = Side::Top;
    QString footprint;
    QString value;
    // Why this row cannot be imported; empty when it can.
    QString error;

    bool isValid() const { return error.isEmpty(); }
};

struct CadExtractOptions
{
    CadMapping mapping;
    CadUnit unit = CadUnit::Millimeter;  // of X and Y unless a value says otherwise
    // Position of the board origin (lower left corner, top view) in the
    // file's coordinates, in mm. Subtracted from every X / Y.
    double originX = 0.0;
    double originY = 0.0;
};

namespace CadPlacements {

// The unit of the X / Y columns: from the X column title, else from a
// comment such as "## Unit = mm", else mm.
CadUnit detectUnit(const CadTable &table, const CadMapping &mapping, bool *found = nullptr);

// Every data row of the table, in file order. A row that cannot be read gets
// an error (missing designator, X / Y not a number, unknown side, designator
// already used by an earlier row) instead of being dropped, so the user can
// see it. Coordinates are as in the file, without mirroring: like the
// program format, CAD tools give bottom side positions viewed from the top.
QVector<CadPlacement> extract(const CadTable &table, const CadExtractOptions &options);

// A length such as "12.7", "-3,5" (decimal comma), "500mil", "12.70mm".
// A unit suffix overrides `unit`. Returns false when it is not a number.
bool parseLength(const QString &text, CadUnit unit, double *mm);

// "TOP", "F.Cu", "TopLayer", "T", "顶面" ... Returns false when unknown.
bool parseSide(const QString &text, Side *side);

}  // namespace CadPlacements
