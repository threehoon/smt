#pragma once

#include <QString>
#include <QVector>

// Unknown is a type this version does not understand. Such a feeder is kept
// and written back unchanged, but can never supply parts.
enum class FeederType { Tape, Tray, Tube, Bulk, Unknown };

struct TapeParams
{
    double width = 8.0;  // tape width, mm
    double pitch = 4.0;  // pocket centre distance, mm
    int feedCount = 1;
};

inline bool operator==(const TapeParams &a, const TapeParams &b)
{
    return a.width == b.width && a.pitch == b.pitch && a.feedCount == b.feedCount;
}

// Defaults only matter for a feeder created or retyped in the editor; files
// must give every required value. They are valid so such a feeder can be saved.
struct TrayParams
{
    int rows = 1;
    int columns = 1;
    double rowPitch = 10.0;
    double columnPitch = 10.0;
    int startIndex = 0;  // first pocket for a new job, in [0, rows * columns)
};

inline bool operator==(const TrayParams &a, const TrayParams &b)
{
    return a.rows == b.rows && a.columns == b.columns && a.rowPitch == b.rowPitch
        && a.columnPitch == b.columnPitch && a.startIndex == b.startIndex;
}

struct TubeParams
{
    double pitch = 0.0;
};

inline bool operator==(const TubeParams &a, const TubeParams &b)
{
    return a.pitch == b.pitch;
}

struct Feeder
{
    QString id;
    FeederType type = FeederType::Tape;
    QString unknownType;  // original type attribute when type is Unknown
    QString slot;
    QString component;  // Component::id
    double pickX = 0.0;  // first pick position, machine coordinates
    double pickY = 0.0;
    double pickZ = 0.0;
    double rotation = 0.0;
    bool enabled = true;
    int priority = 0;  // higher is used first among feeders of one component

    // Only the block matching `type` is meaningful.
    TapeParams tape;
    TrayParams tray;
    TubeParams tube;
    QVector<QString> rawParams;  // child elements of an Unknown-type feeder

    QVector<QString> extensions;  // raw <Extension> elements

    // Whether this feeder may supply parts at run time.
    bool canFeed() const { return enabled && type != FeederType::Unknown; }
};

inline bool operator==(const Feeder &a, const Feeder &b)
{
    return a.id == b.id && a.type == b.type && a.unknownType == b.unknownType
        && a.slot == b.slot && a.component == b.component && a.pickX == b.pickX
        && a.pickY == b.pickY && a.pickZ == b.pickZ && a.rotation == b.rotation
        && a.enabled == b.enabled && a.priority == b.priority && a.tape == b.tape
        && a.tray == b.tray && a.tube == b.tube && a.rawParams == b.rawParams
        && a.extensions == b.extensions;
}
