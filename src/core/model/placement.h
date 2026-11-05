#pragma once

#include <QString>

enum class Side { Top, Bottom };

// One reference designator on a board definition. Shared by every BoardInstance
// of that board; a placement target at run time is (instance id, placement id).
struct Placement
{
    QString id;         // reference designator, unique within its board
    QString component;  // Component::id
    double x = 0.0;     // board coordinates, viewed from the top side
    double y = 0.0;
    double rotation = 0.0;
    Side side = Side::Top;
    bool enabled = true;  // false = do not place (DNP)
};

inline bool operator==(const Placement &a, const Placement &b)
{
    return a.id == b.id && a.component == b.component && a.x == b.x && a.y == b.y
        && a.rotation == b.rotation && a.side == b.side && a.enabled == b.enabled;
}
