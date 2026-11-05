#include "edit/programedits.h"

#include <QSet>

#include <algorithm>

namespace {

template <typename T, typename Pred>
int removeIf(QVector<T> &list, Pred pred)
{
    const auto end = std::remove_if(list.begin(), list.end(), pred);
    const int removed = int(list.end() - end);
    list.erase(end, list.end());
    return removed;
}

int countPlacementsUsing(const Program &program, const QSet<QString> &components)
{
    int n = 0;
    for (const Board &b : program.boards) {
        n += int(std::count_if(b.placements.cbegin(), b.placements.cend(),
                               [&](const Placement &p) { return components.contains(p.component); }));
    }
    return n;
}

int countFeedersUsing(const Program &program, const QSet<QString> &components)
{
    if (!program.feederSetup)
        return 0;
    const QVector<Feeder> &feeders = program.feederSetup->feeders;
    return int(std::count_if(feeders.cbegin(), feeders.cend(),
                             [&](const Feeder &f) { return components.contains(f.component); }));
}

// Removes the placements and feeders of `components`, and the skip entries of
// the removed placements.
void removeUsesOfComponents(Program &program, const QSet<QString> &components)
{
    for (Board &b : program.boards) {
        QSet<QString> removed;
        for (const Placement &p : qAsConst(b.placements)) {
            if (components.contains(p.component))
                removed.insert(p.id);
        }
        if (removed.isEmpty())
            continue;
        removeIf(b.placements, [&](const Placement &p) { return removed.contains(p.id); });
        for (BoardInstance &bi : program.panel.instances) {
            if (bi.board == b.id) {
                bi.skippedPlacements.erase(
                    std::remove_if(bi.skippedPlacements.begin(), bi.skippedPlacements.end(),
                                   [&](const QString &id) { return removed.contains(id); }),
                    bi.skippedPlacements.end());
            }
        }
    }
    if (program.feederSetup) {
        removeIf(program.feederSetup->feeders,
                 [&](const Feeder &f) { return components.contains(f.component); });
    }
}

}  // namespace

namespace ProgramEdits {

QString uniqueId(const QString &prefix, const QStringList &existing)
{
    const QSet<QString> taken(existing.cbegin(), existing.cend());
    for (int n = 1;; ++n) {
        const QString id = prefix + QString::number(n);
        if (!taken.contains(id))
            return id;
    }
}

Program blankProgram()
{
    Program p;
    p.info.name = QStringLiteral("NEW");

    Board b;
    b.id = QStringLiteral("PCB");
    b.width = 100.0;
    b.height = 80.0;
    b.thickness = 1.6;
    p.boards.append(b);

    p.panel.width = b.width;
    p.panel.height = b.height;
    BoardInstance bi;
    bi.id = QStringLiteral("1");
    bi.board = b.id;
    p.panel.instances.append(bi);
    return p;
}

void renamePackage(Program &program, int index, const QString &id)
{
    Package &p = program.packages[index];
    for (Component &c : program.components) {
        if (c.package == p.id)
            c.package = id;
    }
    p.id = id;
}

void renameComponent(Program &program, int index, const QString &id)
{
    Component &c = program.components[index];
    for (Board &b : program.boards) {
        for (Placement &pl : b.placements) {
            if (pl.component == c.id)
                pl.component = id;
        }
    }
    if (program.feederSetup) {
        for (Feeder &f : program.feederSetup->feeders) {
            if (f.component == c.id)
                f.component = id;
        }
    }
    c.id = id;
}

void renameBoard(Program &program, int index, const QString &id)
{
    Board &b = program.boards[index];
    for (BoardInstance &bi : program.panel.instances) {
        if (bi.board == b.id)
            bi.board = id;
    }
    b.id = id;
}

void renamePlacement(Program &program, int board, int index, const QString &id)
{
    const Board &b = program.boards.at(board);
    const QString old = b.placements.at(index).id;
    for (BoardInstance &bi : program.panel.instances) {
        if (bi.board != b.id)
            continue;
        for (QString &skip : bi.skippedPlacements) {
            if (skip == old)
                skip = id;
        }
    }
    program.boards[board].placements[index].id = id;
}

RemovalImpact packageRemovalImpact(const Program &program, int index)
{
    const QString &id = program.packages.at(index).id;
    QSet<QString> components;
    for (const Component &c : program.components) {
        if (c.package == id)
            components.insert(c.id);
    }
    RemovalImpact impact;
    impact.components = components.size();
    impact.placements = countPlacementsUsing(program, components);
    impact.feeders = countFeedersUsing(program, components);
    return impact;
}

RemovalImpact componentRemovalImpact(const Program &program, int index)
{
    const QSet<QString> components{program.components.at(index).id};
    RemovalImpact impact;
    impact.placements = countPlacementsUsing(program, components);
    impact.feeders = countFeedersUsing(program, components);
    return impact;
}

RemovalImpact boardRemovalImpact(const Program &program, int index)
{
    const Board &b = program.boards.at(index);
    RemovalImpact impact;
    impact.placements = b.placements.size();
    impact.instances = int(std::count_if(program.panel.instances.cbegin(),
                                         program.panel.instances.cend(),
                                         [&](const BoardInstance &bi) { return bi.board == b.id; }));
    return impact;
}

void removePackage(Program &program, int index)
{
    const QString id = program.packages.at(index).id;
    QSet<QString> components;
    for (const Component &c : qAsConst(program.components)) {
        if (c.package == id)
            components.insert(c.id);
    }
    removeUsesOfComponents(program, components);
    removeIf(program.components, [&](const Component &c) { return components.contains(c.id); });
    program.packages.removeAt(index);
}

void removeComponent(Program &program, int index)
{
    removeUsesOfComponents(program, {program.components.at(index).id});
    program.components.removeAt(index);
}

void removeBoard(Program &program, int index)
{
    const QString id = program.boards.at(index).id;
    removeIf(program.panel.instances, [&](const BoardInstance &bi) { return bi.board == id; });
    program.boards.removeAt(index);
}

void removePlacement(Program &program, int board, int index)
{
    const Board &b = program.boards.at(board);
    const QString id = b.placements.at(index).id;
    for (BoardInstance &bi : program.panel.instances) {
        if (bi.board == b.id)
            bi.skippedPlacements.removeAll(id);
    }
    program.boards[board].placements.removeAt(index);
}

}  // namespace ProgramEdits
