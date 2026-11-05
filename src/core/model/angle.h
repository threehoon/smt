#pragma once

#include <cmath>

// Wraps an angle in degrees into [0, 360).
inline double normalizeAngle(double degrees)
{
    double a = std::fmod(degrees, 360.0);
    if (a < 0.0)
        a += 360.0;
    // A tiny negative input such as -1e-15 becomes exactly 360.0 after the add.
    if (a >= 360.0)
        a = 0.0;
    // fmod keeps the sign of zero; return +0 so it never prints as "-0".
    return a == 0.0 ? 0.0 : a;
}
