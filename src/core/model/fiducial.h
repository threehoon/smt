#pragma once

#include <QString>

enum class FiducialShape { Circle, Square, Cross };

// Mark point used for optical alignment. Board fiducials use board coordinates,
// panel fiducials use panel coordinates.
struct Fiducial
{
    QString id;
    double x = 0.0;
    double y = 0.0;
    FiducialShape shape = FiducialShape::Circle;
    double size = 1.0;  // diameter or edge length
};

inline bool operator==(const Fiducial &a, const Fiducial &b)
{
    return a.id == b.id && a.x == b.x && a.y == b.y && a.shape == b.shape && a.size == b.size;
}
