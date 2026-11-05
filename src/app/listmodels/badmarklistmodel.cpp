#include "listmodels/badmarklistmodel.h"

#include "model/program.h"

#include <QStringList>

QVariant BadMarkListModel::data(const QModelIndex &index, int role) const
{
    const Row *r = rowAt(index);
    const Board *b = board();
    const BadMark *m = r && b ? itemAt(b->badMarks, r->index) : nullptr;
    if (!m)
        return {};
    switch (role) {
    case IdRole:
        return m->id;
    case XRole:
        return m->x;
    case YRole:
        return m->y;
    }
    return {};
}

QHash<int, QByteArray> BadMarkListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {XRole, "x"},
        {YRole, "y"},
    };
}

QVector<ProgramListModel::Row> BadMarkListModel::buildRows() const
{
    QVector<Row> rows;
    if (const Board *b = board()) {
        for (int i = 0; i < b->badMarks.size(); ++i)
            rows.append({b->badMarks.at(i).id, i, -1});
    }
    return rows;
}

bool BadMarkListModel::setRole(const Row &row, int role, const QVariant &value)
{
    if (!board())
        return false;
    const int b = boardIndex();
    const int i = row.index;
    const QString text = tr("修改坏板标记 %1").arg(row.key);
    auto mark = [b, i](Program &p) -> BadMark & { return p.boards[b].badMarks[i]; };
    switch (role) {
    case IdRole: {
        const QString id = value.toString().trimmed();
        return editRename(tr("重命名坏板标记 %1").arg(row.key),
                          {IdRename::Scope::BadMark, boardId(), row.key, id},
                          [&](Program &p) { mark(p).id = id; });
    }
    case XRole:
        return editNumber(text, value, [&](Program &p, double v) { mark(p).x = v; });
    case YRole:
        return editNumber(text, value, [&](Program &p, double v) { mark(p).y = v; });
    }
    return false;
}

int BadMarkListModel::add()
{
    const Board *b = board();
    if (!b)
        return -1;
    QStringList ids;
    for (const BadMark &m : b->badMarks)
        ids << m.id;
    BadMark m;
    m.id = ProgramEdits::uniqueId(QStringLiteral("BM"), ids);
    const int index = boardIndex();
    const int row = b->badMarks.size();
    if (!edit(tr("添加坏板标记 %1").arg(m.id),
              [&](Program &p) { p.boards[index].badMarks.append(m); }))
        return -1;
    return row;
}

bool BadMarkListModel::remove(int row)
{
    const Row *r = rowAt(row);
    if (!r || !board())
        return false;
    const int b = boardIndex();
    const int i = r->index;
    return edit(tr("删除坏板标记 %1").arg(r->key),
                [&](Program &p) { p.boards[b].badMarks.removeAt(i); });
}
