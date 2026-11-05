#pragma once

#include <QStringList>

// Attribute spellings for model enums, indexed by the enum's underlying value.
// Shared by ProgramReader and ProgramWriter so the two cannot drift apart.

inline const QStringList &sideNames()
{
    static const QStringList names{QStringLiteral("top"), QStringLiteral("bottom")};
    return names;
}

inline const QStringList &fiducialShapeNames()
{
    static const QStringList names{QStringLiteral("circle"), QStringLiteral("square"),
                                   QStringLiteral("cross")};
    return names;
}

inline const QStringList &visionMethodNames()
{
    static const QStringList names{QStringLiteral("none"), QStringLiteral("bottom"),
                                   QStringLiteral("top")};
    return names;
}

// Known feeder types only; FeederType::Unknown has no fixed spelling.
inline const QStringList &feederTypeNames()
{
    static const QStringList names{QStringLiteral("tape"), QStringLiteral("tray"),
                                   QStringLiteral("tube"), QStringLiteral("bulk")};
    return names;
}
