#pragma once

#include "model/fiducial.h"
#include "model/placement.h"

#include <QString>
#include <QVector>

// Position checked for a bad-board mark; if a mark is found the whole board
// instance is skipped.
struct BadMark
{
    QString id;
    double x = 0.0;
    double y = 0.0;
};

inline bool operator==(const BadMark &a, const BadMark &b)
{
    return a.id == b.id && a.x == b.x && a.y == b.y;
}

// A board definition. Stored once and referenced by any number of BoardInstances.
struct Board
{
    QString id;
    double width = 0.0;
    double height = 0.0;
    double thickness = 0.0;
    QVector<Fiducial> fiducials;
    QVector<BadMark> badMarks;
    QVector<Placement> placements;
};

inline bool operator==(const Board &a, const Board &b)
{
    return a.id == b.id && a.width == b.width && a.height == b.height
        && a.thickness == b.thickness && a.fiducials == b.fiducials
        && a.badMarks == b.badMarks && a.placements == b.placements;
}
