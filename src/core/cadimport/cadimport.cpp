#include "cadimport/cadimport.h"

#include <QHash>
#include <QPair>
#include <QSet>

#include <algorithm>

namespace CadImport {

namespace {

using PartKey = QPair<QString, QString>;  // footprint, value

PartKey keyOf(const CadPlacement &p)
{
    return {p.footprint, p.value};
}

const Component *findComponent(const Program &program, const QString &id)
{
    for (const Component &c : program.components) {
        if (c.id == id)
            return &c;
    }
    return nullptr;
}

bool hasPackage(const Program &program, const QString &id)
{
    return std::any_of(program.packages.cbegin(), program.packages.cend(),
                       [&](const Package &p) { return p.id == id; });
}

// Same rule as ProgramValidator: a part may overhang the board edge.
bool isOutside(const Placement &p, const Board &b)
{
    return b.width > 0.0 && b.height > 0.0
        && (p.x < 0.0 || p.y < 0.0 || p.x > b.width || p.y > b.height);
}

// `base`, or base-2, base-3 ... when taken.
QString unique(const QString &base, QSet<QString> &taken)
{
    QString id = base;
    for (int n = 2; taken.contains(id); ++n)
        id = base + QLatin1Char('-') + QString::number(n);
    taken.insert(id);
    return id;
}

bool samePlacement(const Placement &a, const Placement &b)
{
    return a.component == b.component && a.x == b.x && a.y == b.y && a.rotation == b.rotation
        && a.side == b.side;
}

}  // namespace

QString componentIdFor(const QString &footprint, const QString &value)
{
    if (!footprint.isEmpty() && !value.isEmpty())
        return footprint + QLatin1Char('-') + value;
    if (!footprint.isEmpty())
        return footprint;
    if (!value.isEmpty())
        return value;
    return QStringLiteral("PART");
}

void suggest(CadPart &part, const Program &program)
{
    part.component.clear();
    part.package.clear();
    for (const Component &c : program.components) {
        if (!part.footprint.isEmpty() && c.package == part.footprint && c.value == part.value) {
            part.component = c.id;
            return;
        }
    }
    if (findComponent(program, componentIdFor(part.footprint, part.value))) {
        part.component = componentIdFor(part.footprint, part.value);
        return;
    }
    if (!part.footprint.isEmpty() && hasPackage(program, part.footprint))
        part.package = part.footprint;
}

QVector<CadPart> groupParts(const QVector<CadPlacement> &rows, const Program &program)
{
    QVector<CadPart> parts;
    QHash<PartKey, int> index;
    for (const CadPlacement &r : rows) {
        if (!r.isValid())
            continue;
        const PartKey key = keyOf(r);
        auto it = index.find(key);
        if (it == index.end()) {
            CadPart part;
            part.footprint = r.footprint;
            part.value = r.value;
            suggest(part, program);
            it = index.insert(key, parts.size());
            parts.append(part);
        }
        parts[*it].designators << r.designator;
    }
    return parts;
}

CadImportSummary apply(Program &program, int board, const QVector<CadPlacement> &rows,
                       const QVector<CadPart> &parts, CadImportMode mode)
{
    CadImportSummary summary;
    summary.rowResults.resize(rows.size());
    if (board < 0 || board >= program.boards.size())
        return summary;

    QSet<QString> packageIds;
    for (const Package &p : program.packages)
        packageIds.insert(p.id);
    // Ids taken, including those this import creates.
    QSet<QString> componentIds;
    for (const Component &c : program.components)
        componentIds.insert(c.id);

    // One new package per footprint, shared by the parts that need it.
    QHash<QString, QString> newPackageFor;
    QHash<PartKey, QString> componentFor;
    for (const CadPart &part : parts) {
        QString component = part.component;
        if (const Component *chosen = component.isEmpty() ? nullptr
                                                          : findComponent(program, component)) {
            summary.partPackages << chosen->package;
        } else {
            QString package = part.package;
            if (package.isEmpty() || !packageIds.contains(package)) {
                auto it = newPackageFor.constFind(part.footprint);
                if (it == newPackageFor.cend()) {
                    Package pkg;
                    pkg.id = unique(part.footprint.isEmpty() ? QStringLiteral("PKG") : part.footprint,
                                    packageIds);
                    pkg.bodyLength = kNewPackageSize;
                    pkg.bodyWidth = kNewPackageSize;
                    pkg.height = kNewPackageSize;
                    program.packages.append(pkg);
                    summary.newPackages << pkg.id;
                    it = newPackageFor.insert(part.footprint, pkg.id);
                }
                package = *it;
            }
            Component c;
            c.id = unique(componentIdFor(part.footprint, part.value), componentIds);
            c.package = package;
            c.value = part.value;
            program.components.append(c);
            summary.newComponents << c.id;
            summary.partPackages << package;
            component = c.id;
        }
        componentFor.insert({part.footprint, part.value}, component);
        summary.partComponents << component;
    }

    Board &b = program.boards[board];
    QHash<QString, int> existing;
    for (int i = 0; i < b.placements.size(); ++i)
        existing.insert(b.placements.at(i).id, i);

    QVector<Placement> result = mode == CadImportMode::Merge ? b.placements : QVector<Placement>();
    QSet<QString> imported;
    for (int i = 0; i < rows.size(); ++i) {
        const CadPlacement &r = rows.at(i);
        CadRowResult &rowResult = summary.rowResults[i];
        const auto component = componentFor.constFind(keyOf(r));
        if (!r.isValid() || component == componentFor.cend())
            continue;
        Placement p;
        p.id = r.designator;
        p.component = *component;
        p.x = r.x;
        p.y = r.y;
        p.rotation = r.rotation;
        p.side = r.side;
        imported.insert(p.id);
        if (isOutside(p, b)) {
            ++summary.outsideBoard;
            rowResult.outsideBoard = true;
        }

        const auto old = existing.constFind(p.id);
        if (old == existing.cend()) {
            ++summary.added;
            rowResult.outcome = CadRowOutcome::Added;
            result.append(p);
            continue;
        }
        const Placement &before = b.placements.at(*old);
        p.enabled = before.enabled;  // the file has no DNP column
        if (samePlacement(before, p)) {
            ++summary.unchanged;
            rowResult.outcome = CadRowOutcome::Unchanged;
        } else {
            ++summary.updated;
            rowResult.outcome = CadRowOutcome::Updated;
        }
        if (mode == CadImportMode::Merge)
            result[*old] = p;
        else
            result.append(p);
    }

    if (mode == CadImportMode::Replace) {
        for (const Placement &p : b.placements) {
            if (!imported.contains(p.id))
                ++summary.removed;
        }
        for (BoardInstance &bi : program.panel.instances) {
            if (bi.board != b.id)
                continue;
            bi.skippedPlacements.erase(
                std::remove_if(bi.skippedPlacements.begin(), bi.skippedPlacements.end(),
                               [&](const QString &id) { return !imported.contains(id); }),
                bi.skippedPlacements.end());
        }
    }
    b.placements = result;
    return summary;
}

}  // namespace CadImport
