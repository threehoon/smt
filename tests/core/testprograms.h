#pragma once

#include "model/program.h"
#include "validation/diagnostic.h"

#include <QStringList>

// Small valid programs built in memory, shared by the core tests.
namespace TestPrograms {

// Two packages, three components, board PCB with two fiducials, a bad mark and
// four placements, two instances (the second skips R2), three feeders.
inline Program minimal()
{
    Program p;
    p.info.name = QStringLiteral("MIN");

    Package r0603;
    r0603.id = QStringLiteral("R0603");
    r0603.bodyLength = 1.6;
    r0603.bodyWidth = 0.8;
    r0603.height = 0.45;
    Package c0402 = r0603;
    c0402.id = QStringLiteral("C0402");
    p.packages = {r0603, c0402};

    p.components = {
        {QStringLiteral("RC-10K"), QStringLiteral("R0603"), QStringLiteral("10K"), {}},
        {QStringLiteral("RC-1K"), QStringLiteral("R0603"), QStringLiteral("1K"), {}},
        {QStringLiteral("CC-100N"), QStringLiteral("C0402"), QStringLiteral("100nF"), {}},
    };

    Board b;
    b.id = QStringLiteral("PCB");
    b.width = 50.0;
    b.height = 40.0;
    b.thickness = 1.6;
    b.fiducials = {{QStringLiteral("M1"), 3.0, 3.0, FiducialShape::Circle, 1.0},
                   {QStringLiteral("M2"), 47.0, 37.0, FiducialShape::Circle, 1.0}};
    b.badMarks = {{QStringLiteral("BM1"), 47.0, 3.0}};
    auto placement = [](const char *id, const char *component, double x, double y) {
        Placement pl;
        pl.id = QLatin1String(id);
        pl.component = QLatin1String(component);
        pl.x = x;
        pl.y = y;
        return pl;
    };
    b.placements = {placement("R1", "RC-10K", 10, 10), placement("R2", "RC-10K", 20, 10),
                    placement("R3", "RC-1K", 30, 10), placement("C1", "CC-100N", 10, 20)};
    p.boards = {b};

    p.panel.width = 120.0;
    p.panel.height = 50.0;
    p.panel.fiducials = {{QStringLiteral("PM1"), 2.0, 2.0, FiducialShape::Circle, 1.0}};
    BoardInstance one;
    one.id = QStringLiteral("1");
    one.board = QStringLiteral("PCB");
    one.x = 5.0;
    one.y = 5.0;
    BoardInstance two = one;
    two.id = QStringLiteral("2");
    two.x = 60.0;
    two.skippedPlacements = QStringList{QStringLiteral("R2")};
    p.panel.instances = {one, two};

    FeederSetup setup;
    setup.machineModel = QStringLiteral("DEMO");
    auto feeder = [](const char *id, const char *slot, const char *component) {
        Feeder f;
        f.id = QLatin1String(id);
        f.slot = QLatin1String(slot);
        f.component = QLatin1String(component);
        return f;
    };
    setup.feeders = {feeder("F1", "S1", "RC-10K"), feeder("F2", "S2", "RC-1K"),
                     feeder("F3", "S3", "RC-10K")};
    p.feederSetup = setup;
    return p;
}

inline QString describe(const QVector<Diagnostic> &list)
{
    QStringList lines;
    for (const Diagnostic &d : list) {
        lines << QStringLiteral("%1 kind %2 [%3,%4] field '%5' line %6: %7")
                     .arg(d.severity == Diagnostic::Severity::Error ? QStringLiteral("error")
                                                                    : QStringLiteral("warning"))
                     .arg(int(d.location.kind))
                     .arg(d.location.index)
                     .arg(d.location.subIndex)
                     .arg(d.field)
                     .arg(d.line)
                     .arg(d.message);
    }
    return lines.isEmpty() ? QStringLiteral("(no diagnostics)") : lines.join(QLatin1Char('\n'));
}

}  // namespace TestPrograms
