#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

enum class VisionMethod { None, Bottom, Top };

struct PickParams
{
    QStringList nozzles;  // allowed nozzle types; empty = any
    double pickDepth = 0.0;
    bool vacuumCheck = true;
};

inline bool operator==(const PickParams &a, const PickParams &b)
{
    return a.nozzles == b.nozzles && a.pickDepth == b.pickDepth
        && a.vacuumCheck == b.vacuumCheck;
}

struct PlaceParams
{
    double placeDepth = 0.0;
    int speed = 100;  // percent, 1-100
};

inline bool operator==(const PlaceParams &a, const PlaceParams &b)
{
    return a.placeDepth == b.placeDepth && a.speed == b.speed;
}

struct VisionParams
{
    VisionMethod method = VisionMethod::Bottom;
    double tolerance = 0.1;
};

inline bool operator==(const VisionParams &a, const VisionParams &b)
{
    return a.method == b.method && a.tolerance == b.tolerance;
}

// Body outline plus default pick / place / vision parameters, shared by every
// Component that uses this package.
struct Package
{
    QString id;
    double bodyLength = 0.0;  // X
    double bodyWidth = 0.0;   // Y
    double height = 0.0;
    int pinCount = 0;
    PickParams pick;
    PlaceParams place;
    VisionParams vision;
    QVector<QString> extensions;  // raw <Extension> elements, written back unchanged
};

inline bool operator==(const Package &a, const Package &b)
{
    return a.id == b.id && a.bodyLength == b.bodyLength && a.bodyWidth == b.bodyWidth
        && a.height == b.height && a.pinCount == b.pinCount && a.pick == b.pick
        && a.place == b.place && a.vision == b.vision && a.extensions == b.extensions;
}
