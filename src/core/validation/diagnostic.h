#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include <algorithm>

// Which element of an in-memory Program a diagnostic is about. Indices are
// into the vectors of the Program; ProgramReader turns them into file lines.
struct ProgramLocation
{
    enum class Kind {
        None,
        Package,        // index = package
        PackagePick,    // index = package; its <Pick>
        PackagePlace,   // index = package; its <Place>
        PackageVision,  // index = package; its <Vision>
        Component,      // index = component
        Board,          // index = board
        BoardFiducial,  // index = board, subIndex = fiducial
        BadMark,        // index = board, subIndex = bad mark
        Placement,      // index = board, subIndex = placement
        Panel,
        PanelFiducial,  // index = fiducial
        BoardInstance,  // index = instance
        Skip,           // index = instance, subIndex = entry in skippedPlacements
        Feeder,         // index = feeder
        FeederParams,   // index = feeder; its <Tape> / <Tray> / <Tube>
    };

    Kind kind = Kind::None;
    int index = -1;
    int subIndex = -1;

    bool isValid() const { return kind != Kind::None; }
};

inline bool operator==(const ProgramLocation &a, const ProgramLocation &b)
{
    return a.kind == b.kind && a.index == b.index && a.subIndex == b.subIndex;
}

inline uint qHash(const ProgramLocation &l, uint seed = 0)
{
    const quint64 key = (quint64(quint32(l.index)) << 32) | quint32(l.subIndex);
    return ::qHash(key, seed) ^ uint(l.kind);
}

struct Diagnostic
{
    enum class Severity { Warning, Error };

    Severity severity = Severity::Error;
    qint64 line = 0;  // 1-based; 0 when there is no meaningful line
    QString message;

    // Set by ProgramValidator; the reader also uses them to map to lines.
    ProgramLocation location;
    QString field;            // file attribute name; empty = the whole element
    ProgramLocation related;  // duplicate id: where the id was first used
};

inline bool hasErrors(const QVector<Diagnostic> &list)
{
    return std::any_of(list.cbegin(), list.cend(), [](const Diagnostic &d) {
        return d.severity == Diagnostic::Severity::Error;
    });
}

inline QVector<Diagnostic> filterSeverity(const QVector<Diagnostic> &list,
                                          Diagnostic::Severity severity)
{
    QVector<Diagnostic> result;
    for (const Diagnostic &d : list) {
        if (d.severity == severity)
            result.append(d);
    }
    return result;
}
