#include "listmodels/boardlistmodel.h"

#include "model/program.h"

#include <QStringList>

QVariant BoardListModel::data(const QModelIndex &index, int role) const
{
    const Row *r = rowAt(index);
    const Board *b = r ? itemAt(program().boards, r->index) : nullptr;
    if (!b)
        return {};
    switch (role) {
    case IdRole:
        return b->id;
    case WidthRole:
        return b->width;
    case HeightRole:
        return b->height;
    case ThicknessRole:
        return b->thickness;
    case PlacementCountRole:
        return b->placements.size();
    case FiducialCountRole:
        return b->fiducials.size();
    }
    return {};
}

QHash<int, QByteArray> BoardListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {WidthRole, "width"},
        {HeightRole, "height"},
        {ThicknessRole, "thickness"},
        {PlacementCountRole, "placementCount"},
        {FiducialCountRole, "fiducialCount"},
    };
}

QVector<ProgramListModel::Row> BoardListModel::buildRows() const
{
    QVector<Row> rows;
    const QVector<Board> &list = program().boards;
    for (int i = 0; i < list.size(); ++i)
        rows.append({list.at(i).id, i, -1});
    return rows;
}

QString BoardListModel::renamedKey(const QString &key, const QVector<IdRename> &renames) const
{
    return followRenames(key, renames, IdRename::Scope::Board);
}

bool BoardListModel::setRole(const Row &row, int role, const QVariant &value)
{
    const int i = row.index;
    const QString text = tr("修改单板 %1").arg(row.key);
    switch (role) {
    case IdRole: {
        const QString id = value.toString().trimmed();
        return editRename(tr("重命名单板 %1").arg(row.key),
                          {IdRename::Scope::Board, {}, row.key, id},
                          [&](Program &p) { ProgramEdits::renameBoard(p, i, id); });
    }
    case WidthRole:
        return editNumber(text, value, [&](Program &p, double v) { p.boards[i].width = v; });
    case HeightRole:
        return editNumber(text, value, [&](Program &p, double v) { p.boards[i].height = v; });
    case ThicknessRole:
        return editNumber(text, value, [&](Program &p, double v) { p.boards[i].thickness = v; });
    }
    return false;
}

int BoardListModel::add()
{
    QStringList ids;
    for (const Board &b : program().boards)
        ids << b.id;
    Board b;
    b.id = ProgramEdits::uniqueId(QStringLiteral("PCB"), ids);
    b.width = 100.0;
    b.height = 80.0;
    b.thickness = 1.6;
    const int row = program().boards.size();
    if (!edit(tr("添加单板 %1").arg(b.id), [&](Program &p) { p.boards.append(b); }))
        return -1;
    return row;
}

bool BoardListModel::remove(int row)
{
    const Row *r = rowAt(row);
    if (!r)
        return false;
    const int i = r->index;
    return edit(tr("删除单板 %1").arg(r->key), [&](Program &p) { ProgramEdits::removeBoard(p, i); });
}

QString BoardListModel::removalImpact(int row) const
{
    const Row *r = rowAt(row);
    return r ? describe(ProgramEdits::boardRemovalImpact(program(), r->index)) : QString();
}
