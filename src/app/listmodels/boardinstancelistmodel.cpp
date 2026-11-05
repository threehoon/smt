#include "listmodels/boardinstancelistmodel.h"

#include "model/angle.h"
#include "model/program.h"

#include <QHash>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>

QVariant BoardInstanceListModel::data(const QModelIndex &index, int role) const
{
    const Row *r = rowAt(index);
    const BoardInstance *bi = r ? itemAt(program().panel.instances, r->index) : nullptr;
    if (!bi)
        return {};
    switch (role) {
    case IdRole:
        return bi->id;
    case BoardRole:
        return bi->board;
    case XRole:
        return bi->x;
    case YRole:
        return bi->y;
    case RotationRole:
        return bi->rotation;
    case EnabledRole:
        return bi->enabled;
    case SkipsRole:
        return bi->skippedPlacements.join(QStringLiteral(", "));
    case PlacementCountRole:
        return m_placementCount.value(r->index);
    }
    return {};
}

QHash<int, QByteArray> BoardInstanceListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {BoardRole, "board"},
        {XRole, "x"},
        {YRole, "y"},
        {RotationRole, "rotation"},
        {EnabledRole, "enabled"},
        {SkipsRole, "skips"},
        {PlacementCountRole, "placementCount"},
    };
}

QVector<ProgramListModel::Row> BoardInstanceListModel::buildRows() const
{
    QVector<Row> rows;
    const QVector<BoardInstance> &list = program().panel.instances;
    for (int i = 0; i < list.size(); ++i)
        rows.append({list.at(i).id, i, -1});
    return rows;
}

QString BoardInstanceListModel::renamedKey(const QString &key,
                                           const QVector<IdRename> &renames) const
{
    return followRenames(key, renames, IdRename::Scope::BoardInstance);
}

bool BoardInstanceListModel::rebuild(const QVector<IdRename> &)
{
    const Program &p = program();
    QHash<QString, const Board *> boards;
    for (const Board &b : p.boards)
        boards.insert(b.id, &b);

    m_placementCount.clear();
    m_placementCount.reserve(p.panel.instances.size());
    for (const BoardInstance &bi : p.panel.instances) {
        int n = 0;
        if (const Board *b = boards.value(bi.board)) {
            for (const Placement &pl : b->placements) {
                if (pl.enabled && !bi.skippedPlacements.contains(pl.id))
                    ++n;
            }
        }
        m_placementCount.append(n);
    }
    return true;
}

bool BoardInstanceListModel::setRole(const Row &row, int role, const QVariant &value)
{
    const int i = row.index;
    const QString text = tr("修改子板实例 %1").arg(row.key);
    auto instance = [i](Program &p) -> BoardInstance & { return p.panel.instances[i]; };
    switch (role) {
    case IdRole: {
        const QString id = value.toString().trimmed();
        return editRename(tr("重命名子板实例 %1").arg(row.key),
                          {IdRename::Scope::BoardInstance, {}, row.key, id},
                          [&](Program &p) { instance(p).id = id; });
    }
    case BoardRole: {
        const QString board = value.toString().trimmed();
        return edit(text, [&](Program &p) {
            BoardInstance &bi = instance(p);
            if (bi.board == board)
                return;
            bi.board = board;
            // Skips name placements of the old board; keep those the new one has.
            QStringList kept;
            for (const Board &b : qAsConst(p.boards)) {
                if (b.id != board)
                    continue;
                for (const QString &skip : qAsConst(bi.skippedPlacements)) {
                    if (std::any_of(b.placements.cbegin(), b.placements.cend(),
                                    [&](const Placement &pl) { return pl.id == skip; }))
                        kept.append(skip);
                }
            }
            bi.skippedPlacements = kept;
        });
    }
    case XRole:
        return editNumber(text, value, [&](Program &p, double v) { instance(p).x = v; });
    case YRole:
        return editNumber(text, value, [&](Program &p, double v) { instance(p).y = v; });
    case RotationRole:
        return editNumber(text, value,
                          [&](Program &p, double v) { instance(p).rotation = normalizeAngle(v); });
    case EnabledRole:
        return edit(text, [&](Program &p) { instance(p).enabled = value.toBool(); });
    case SkipsRole: {
        // Separated by commas (either width) or spaces. Duplicates are kept so
        // the validator can name them instead of silently dropping one.
        static const QRegularExpression kSeparators(QStringLiteral("[,，\\s]+"));
        const QStringList skips =
            value.toString().split(kSeparators, QString::SkipEmptyParts);
        return edit(text, [&](Program &p) { instance(p).skippedPlacements = skips; });
    }
    }
    return false;
}

int BoardInstanceListModel::add()
{
    const Program &prog = program();
    if (prog.boards.isEmpty())
        return -1;
    QStringList ids;
    for (const BoardInstance &bi : prog.panel.instances)
        ids << bi.id;
    BoardInstance bi;
    bi.id = ProgramEdits::uniqueId(QString(), ids);
    bi.board = prog.panel.instances.isEmpty() ? prog.boards.first().id
                                              : prog.panel.instances.last().board;
    const int row = prog.panel.instances.size();
    if (!edit(tr("添加子板实例 %1").arg(bi.id), [&](Program &p) { p.panel.instances.append(bi); }))
        return -1;
    return row;
}

bool BoardInstanceListModel::remove(int row)
{
    const Row *r = rowAt(row);
    if (!r)
        return false;
    const int i = r->index;
    return edit(tr("删除子板实例 %1").arg(r->key),
                [&](Program &p) { p.panel.instances.removeAt(i); });
}

int BoardInstanceListModel::move(int row, int delta)
{
    const Row *r = rowAt(row);
    if (!r)
        return -1;
    const int from = r->index;
    const int to = from + delta;
    if (delta == 0 || to < 0 || to >= program().panel.instances.size())
        return -1;
    if (!edit(tr("移动子板实例 %1").arg(r->key),
              [&](Program &p) { p.panel.instances.move(from, to); }))
        return -1;
    return to;
}
