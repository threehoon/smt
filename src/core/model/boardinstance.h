#pragma once

#include <QString>
#include <QStringList>

// One board on the panel: which Board definition, and where its origin sits in
// panel coordinates.
struct BoardInstance
{
    QString id;     // unique within the panel; shown as "board N" in the UI
    QString board;  // Board::id
    double x = 0.0;
    double y = 0.0;
    double rotation = 0.0;  // about the board origin
    bool enabled = true;
    QStringList skippedPlacements;  // Placement ids not placed on this instance
};

inline bool operator==(const BoardInstance &a, const BoardInstance &b)
{
    return a.id == b.id && a.board == b.board && a.x == b.x && a.y == b.y
        && a.rotation == b.rotation && a.enabled == b.enabled
        && a.skippedPlacements == b.skippedPlacements;
}
