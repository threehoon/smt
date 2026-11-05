#pragma once

#include "model/boardinstance.h"
#include "model/fiducial.h"

#include <QVector>

// The panel. A single board is still a panel with one BoardInstance, so run
// time has only one path: panel -> instance -> placement.
struct Panel
{
    double width = 0.0;
    double height = 0.0;
    QVector<Fiducial> fiducials;
    QVector<BoardInstance> instances;
};

inline bool operator==(const Panel &a, const Panel &b)
{
    return a.width == b.width && a.height == b.height && a.fiducials == b.fiducials
        && a.instances == b.instances;
}
