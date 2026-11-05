#include "listmodels/jobplacementlistmodel.h"

#include "display/displaytext.h"
#include "model/program.h"

#include <QStringList>

#include <algorithm>

namespace {

const QChar kKeySeparator = QLatin1Char('\n');  // cannot occur in an id typed in a field

QString rowKey(const QString &instance, const QString &placement)
{
    return instance + kKeySeparator + placement;
}

}  // namespace

JobPlacementListModel::JobPlacementListModel(ProgramDocument *document, QObject *parent)
    : ProgramListModel(document, parent)
{
    // enabledCount is recomputed on every reset and edit, like count.
    connect(this, &ProgramListModel::countChanged, this,
            &JobPlacementListModel::enabledCountChanged);
}

const BoardInstance *JobPlacementListModel::instance(const Row &row) const
{
    return itemAt(program().panel.instances, row.index);
}

const Placement *JobPlacementListModel::placement(const Row &row) const
{
    const Board *b = itemAt(program().boards, m_instanceBoard.value(row.index, -1));
    return b ? itemAt(b->placements, row.sub) : nullptr;
}

QVariant JobPlacementListModel::data(const QModelIndex &index, int role) const
{
    const Row *r = rowAt(index);
    const BoardInstance *bi = r ? instance(*r) : nullptr;
    const Placement *pl = r ? placement(*r) : nullptr;
    if (!bi || !pl)
        return {};
    switch (role) {
    case InstanceRole:
        return bi->id;
    case IdRole:
        return pl->id;
    case ComponentRole:
        return pl->component;
    case SideRole:
        return DisplayText::side(pl->side);
    case XRole:
        return pl->x;
    case YRole:
        return pl->y;
    case RotationRole:
        return pl->rotation;
    case FeederRole:
        return m_feederSlots.value(pl->component);
    case SkippedRole:
        return bi->skippedPlacements.contains(pl->id);
    case EnabledRole:
        return bi->enabled && pl->enabled && !bi->skippedPlacements.contains(pl->id);
    }
    return {};
}

QHash<int, QByteArray> JobPlacementListModel::roleNames() const
{
    return {
        {InstanceRole, "instance"},
        {IdRole, "id"},
        {ComponentRole, "component"},
        {SideRole, "side"},
        {XRole, "x"},
        {YRole, "y"},
        {RotationRole, "rotation"},
        {FeederRole, "feeder"},
        {SkippedRole, "skipped"},
        {EnabledRole, "enabled"},
    };
}

QVector<ProgramListModel::Row> JobPlacementListModel::buildRows() const
{
    const Program &p = program();
    QVector<Row> rows;
    for (int i = 0; i < p.panel.instances.size(); ++i) {
        const Board *b = itemAt(p.boards, m_instanceBoard.value(i, -1));
        if (!b)
            continue;  // the validator rejects such a program; nothing to expand
        const QString &id = p.panel.instances.at(i).id;
        for (int j = 0; j < b->placements.size(); ++j)
            rows.append({rowKey(id, b->placements.at(j).id), i, j});
    }
    return rows;
}

QString JobPlacementListModel::renamedKey(const QString &key,
                                          const QVector<IdRename> &renames) const
{
    const int split = key.indexOf(kKeySeparator);
    const QString instance =
        followRenames(key.left(split), renames, IdRename::Scope::BoardInstance);
    QString placement = key.mid(split + 1);

    // Placement renames are per board; find the instance's board after the edit.
    const Program &p = program();
    QString board;
    for (int i = 0; i < p.panel.instances.size(); ++i) {
        if (p.panel.instances.at(i).id == instance) {
            board = p.panel.instances.at(i).board;
            break;
        }
    }
    for (const IdRename &r : renames) {
        if (r.scope == IdRename::Scope::Placement && r.board == board && r.from == placement)
            placement = r.to;
    }
    return rowKey(instance, placement);
}

bool JobPlacementListModel::rebuild(const QVector<IdRename> &)
{
    const Program &p = program();

    QHash<QString, int> boardIndex;
    for (int i = 0; i < p.boards.size(); ++i)
        boardIndex.insert(p.boards.at(i).id, i);

    m_instanceBoard.clear();
    m_enabledCount = 0;
    for (const BoardInstance &bi : p.panel.instances) {
        const int b = boardIndex.value(bi.board, -1);
        m_instanceBoard.append(b);
        if (b < 0 || !bi.enabled)
            continue;
        for (const Placement &pl : p.boards.at(b).placements) {
            if (pl.enabled && !bi.skippedPlacements.contains(pl.id))
                ++m_enabledCount;
        }
    }

    // Higher priority first (docs/program-format.md 4.7), file order among
    // equal priorities.
    m_feederSlots.clear();
    if (p.feederSetup) {
        QVector<const Feeder *> feeders;
        for (const Feeder &f : p.feederSetup->feeders)
            feeders.append(&f);
        std::stable_sort(feeders.begin(), feeders.end(),
                         [](const Feeder *a, const Feeder *b) { return a->priority > b->priority; });
        QHash<QString, QStringList> byComponent;
        for (const Feeder *f : feeders)
            byComponent[f->component].append(f->slot);
        for (auto it = byComponent.cbegin(); it != byComponent.cend(); ++it)
            m_feederSlots.insert(it.key(), it.value().join(QStringLiteral(", ")));
    }
    return true;
}

bool JobPlacementListModel::toggleSkip(int row)
{
    const Row *r = rowAt(row);
    const BoardInstance *bi = r ? instance(*r) : nullptr;
    const Placement *pl = r ? placement(*r) : nullptr;
    if (!bi || !pl)
        return false;
    const int i = r->index;
    const QString id = pl->id;
    const bool skipped = bi->skippedPlacements.contains(id);
    const QString text = skipped ? tr("恢复子板 %1 的贴片位 %2").arg(bi->id, id)
                                 : tr("跳过子板 %1 的贴片位 %2").arg(bi->id, id);
    return edit(text, [&](Program &p) {
        QStringList &skips = p.panel.instances[i].skippedPlacements;
        if (skipped)
            skips.removeAll(id);
        else
            skips.append(id);
    });
}
