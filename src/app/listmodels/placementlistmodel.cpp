#include "listmodels/placementlistmodel.h"

#include "display/displaytext.h"
#include "document/programdocument.h"
#include "model/angle.h"
#include "model/program.h"

#include <QStringList>

QVariant PlacementListModel::data(const QModelIndex &index, int role) const
{
    const Row *r = rowAt(index);
    const Board *b = board();
    const Placement *p = r && b ? itemAt(b->placements, r->index) : nullptr;
    if (!p)
        return {};
    switch (role) {
    case IdRole:
        return p->id;
    case ComponentRole:
        return p->component;
    case SideRole:
        return DisplayText::side(p->side);
    case SideIndexRole:
        return static_cast<int>(p->side);
    case XRole:
        return p->x;
    case YRole:
        return p->y;
    case RotationRole:
        return p->rotation;
    case EnabledRole:
        return p->enabled;
    }
    return {};
}

QHash<int, QByteArray> PlacementListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {ComponentRole, "component"},
        {SideRole, "side"},
        {SideIndexRole, "sideIndex"},
        {XRole, "x"},
        {YRole, "y"},
        {RotationRole, "rotation"},
        {EnabledRole, "enabled"},
    };
}

QVector<ProgramListModel::Row> PlacementListModel::buildRows() const
{
    QVector<Row> rows;
    if (const Board *b = board()) {
        for (int i = 0; i < b->placements.size(); ++i)
            rows.append({b->placements.at(i).id, i, -1});
    }
    return rows;
}

bool PlacementListModel::setRole(const Row &row, int role, const QVariant &value)
{
    if (!board())
        return false;
    const int b = boardIndex();
    const int i = row.index;
    const QString text = tr("修改贴片位 %1").arg(row.key);
    auto placement = [b, i](Program &p) -> Placement & { return p.boards[b].placements[i]; };
    switch (role) {
    case IdRole: {
        const QString id = value.toString().trimmed();
        return editRename(tr("重命名贴片位 %1").arg(row.key),
                          {IdRename::Scope::Placement, boardId(), row.key, id},
                          [&](Program &p) { ProgramEdits::renamePlacement(p, b, i, id); });
    }
    case ComponentRole: {
        const QString component = value.toString().trimmed();
        return edit(text, [&](Program &p) { placement(p).component = component; });
    }
    case SideIndexRole: {
        const int side = value.toInt();
        if (side != int(Side::Top) && side != int(Side::Bottom))
            return false;
        return edit(text, [&](Program &p) { placement(p).side = static_cast<Side>(side); });
    }
    case XRole:
        return editNumber(text, value, [&](Program &p, double v) { placement(p).x = v; });
    case YRole:
        return editNumber(text, value, [&](Program &p, double v) { placement(p).y = v; });
    case RotationRole:
        return editNumber(text, value,
                          [&](Program &p, double v) { placement(p).rotation = normalizeAngle(v); });
    case EnabledRole:
        return edit(text, [&](Program &p) { placement(p).enabled = value.toBool(); });
    }
    return false;
}

int PlacementListModel::add()
{
    const Board *b = board();
    if (!b)
        return -1;
    const Program &prog = program();
    if (prog.components.isEmpty()) {
        document()->reject(tr("不能添加贴片位"), tr("贴片位必须指定元件，请先在元件页添加元件。"));
        return -1;
    }
    QStringList ids;
    for (const Placement &p : b->placements)
        ids << p.id;
    Placement pl;
    pl.id = ProgramEdits::uniqueId(QStringLiteral("P"), ids);
    pl.component = prog.components.first().id;
    const int index = boardIndex();
    const int row = b->placements.size();
    if (!edit(tr("添加贴片位 %1").arg(pl.id),
              [&](Program &p) { p.boards[index].placements.append(pl); }))
        return -1;
    return row;
}

bool PlacementListModel::remove(int row)
{
    const Row *r = rowAt(row);
    if (!r || !board())
        return false;
    const int b = boardIndex();
    const int i = r->index;
    return edit(tr("删除贴片位 %1").arg(r->key),
                [&](Program &p) { ProgramEdits::removePlacement(p, b, i); });
}
