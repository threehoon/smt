#include "listmodels/programlistmodel.h"

#include "document/programdocument.h"

#include <QHash>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace {

// Above this many rows out of order an edit becomes a reset instead of
// one beginMoveRows per row. Only reordering board instances moves rows.
constexpr int kMaxMoves = 200;

}  // namespace

ProgramListModel::ProgramListModel(ProgramDocument *document, QObject *parent)
    : QAbstractListModel(parent), m_document(document)
{
    // The program is replaced between the two signals, so the reset must
    // straddle them rather than happen after the fact.
    connect(document, &ProgramDocument::programAboutToChange, this,
            [this] { beginResetModel(); });
    connect(document, &ProgramDocument::programChanged, this, [this] {
        programReplaced();
        rebuild({});
        m_rows = buildRows();
        endResetModel();
        emit countChanged();
    });
    connect(document, &ProgramDocument::programEdited, this, &ProgramListModel::sync);
}

int ProgramListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariantMap ProgramListModel::get(int row) const
{
    QVariantMap values;
    const QModelIndex idx = index(row);
    if (!idx.isValid())
        return values;
    const QHash<int, QByteArray> names = roleNames();
    for (auto it = names.cbegin(); it != names.cend(); ++it)
        values.insert(QString::fromUtf8(it.value()), data(idx, it.key()));
    return values;
}

bool ProgramListModel::set(int row, const QString &role, const QVariant &value)
{
    const QByteArray name = role.toUtf8();
    const QHash<int, QByteArray> names = roleNames();
    for (auto it = names.cbegin(); it != names.cend(); ++it) {
        if (it.value() == name)
            return setData(index(row), value, it.key());
    }
    qWarning("ProgramListModel::set: unknown role %s", name.constData());
    return false;
}

bool ProgramListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    const Row *r = rowAt(index);
    if (!r)
        return false;
    // setRole may trigger a sync that changes m_rows; pass a copy.
    const Row copy = *r;
    return setRole(copy, role, value);
}

const Program &ProgramListModel::program() const
{
    return m_document->program();
}

const ProgramListModel::Row *ProgramListModel::rowAt(const QModelIndex &index) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0)
        return nullptr;
    return rowAt(index.row());
}

bool ProgramListModel::canAdd() const
{
    return m_document->isLoaded();
}

int ProgramListModel::add()
{
    return -1;
}

bool ProgramListModel::remove(int)
{
    return false;
}

QString ProgramListModel::removalImpact(int) const
{
    return {};
}

QString ProgramListModel::renamedKey(const QString &key, const QVector<IdRename> &) const
{
    return key;
}

bool ProgramListModel::rebuild(const QVector<IdRename> &)
{
    return true;
}

bool ProgramListModel::setRole(const Row &, int, const QVariant &)
{
    return false;
}

QString ProgramListModel::followRenames(QString key, const QVector<IdRename> &renames,
                                        IdRename::Scope scope)
{
    for (const IdRename &r : renames) {
        if (r.scope == scope && r.from == key)
            key = r.to;
    }
    return key;
}

void ProgramListModel::resetRows(const std::function<void()> &prepare)
{
    beginResetModel();
    if (prepare)
        prepare();
    rebuild({});
    m_rows = buildRows();
    endResetModel();
    emit countChanged();
}

bool ProgramListModel::edit(const QString &text, const std::function<void(Program &)> &change)
{
    return m_document->edit(text, [&](Program &p, QVector<IdRename> &) { change(p); });
}

bool ProgramListModel::editNumber(const QString &text, const QVariant &value,
                                  const std::function<void(Program &, double)> &assign)
{
    double v = 0.0;
    if (!m_document->parseNumber(value, &v))
        return false;
    return edit(text, [&](Program &p) { assign(p, v); });
}

bool ProgramListModel::editInteger(const QString &text, const QVariant &value,
                                   const std::function<void(Program &, int)> &assign)
{
    int v = 0;
    if (!m_document->parseInteger(value, &v))
        return false;
    return edit(text, [&](Program &p) { assign(p, v); });
}

bool ProgramListModel::editRename(const QString &text, const IdRename &rename,
                                  const std::function<void(Program &)> &change)
{
    return m_document->edit(text, [&](Program &p, QVector<IdRename> &renames) {
        change(p);
        renames.append(rename);
    });
}

QString ProgramListModel::describe(const RemovalImpact &impact)
{
    QStringList parts;
    if (impact.components > 0)
        parts << tr("%1 个元件").arg(impact.components);
    if (impact.placements > 0)
        parts << tr("%1 个贴片位").arg(impact.placements);
    if (impact.feeders > 0)
        parts << tr("%1 个送料器").arg(impact.feeders);
    if (impact.instances > 0)
        parts << tr("%1 个子板实例").arg(impact.instances);
    return parts.isEmpty() ? QString()
                           : tr("同时会删除：%1。").arg(parts.join(QStringLiteral("、")));
}

void ProgramListModel::replaceRows(const QVector<Row> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
    emit countChanged();
}

void ProgramListModel::sync(const QVector<IdRename> &renames)
{
    const bool matchable = rebuild(renames);
    const QVector<Row> target = buildRows();
    if (!matchable) {
        replaceRows(target);
        return;
    }

    QHash<QString, int> targetIndex;
    targetIndex.reserve(target.size());
    for (int i = 0; i < target.size(); ++i)
        targetIndex.insert(target.at(i).key, i);
    if (targetIndex.size() != target.size()) {
        replaceRows(target);  // keys not unique: cannot match rows
        return;
    }

    // pos[i]: where m_rows[i] belongs in target, -1 when it is gone. Live rows
    // take their new key and indices right away, so data() is right for them
    // while views handle the structural signals below.
    QVector<int> pos(m_rows.size(), -1);
    QSet<int> claimed;
    for (int i = 0; i < m_rows.size(); ++i) {
        const int t = targetIndex.value(renamedKey(m_rows.at(i).key, renames), -1);
        if (t < 0)
            continue;
        if (claimed.contains(t)) {
            replaceRows(target);  // two old rows map to one new row
            return;
        }
        claimed.insert(t);
        pos[i] = t;
        m_rows[i] = target.at(t);
    }

    // 1. Remove rows that are gone, last run first so indices stay valid.
    for (int end = m_rows.size() - 1; end >= 0;) {
        if (pos.at(end) >= 0) {
            --end;
            continue;
        }
        int start = end;
        while (start > 0 && pos.at(start - 1) < 0)
            --start;
        beginRemoveRows({}, start, end);
        m_rows.remove(start, end - start + 1);
        pos.remove(start, end - start + 1);
        endRemoveRows();
        end = start - 1;
    }

    // 2. Put the remaining rows in target order. An edit moves at most a few
    // rows (board instance up / down); anything bigger is a reset.
    {
        QVector<int> sorted = pos;
        std::sort(sorted.begin(), sorted.end());
        int misplaced = 0;
        for (int i = 0; i < pos.size(); ++i) {
            if (pos.at(i) != sorted.at(i))
                ++misplaced;
        }
        if (misplaced > kMaxMoves) {
            replaceRows(target);
            return;
        }
        for (int i = 0; misplaced > 0 && i < pos.size(); ++i) {
            if (pos.at(i) == sorted.at(i))
                continue;
            const int from = int(std::find(pos.begin() + i, pos.end(), sorted.at(i)) - pos.begin());
            beginMoveRows({}, from, from, {}, i);
            m_rows.move(from, i);
            pos.move(from, i);
            endMoveRows();
        }
    }

    // 3. Insert new rows, in runs at the positions they have in target.
    for (int i = 0; i < target.size();) {
        if (i < pos.size() && pos.at(i) == i) {
            ++i;
            continue;
        }
        int end = i;
        while (end + 1 < target.size() && !claimed.contains(end + 1))
            ++end;
        beginInsertRows({}, i, end);
        for (int t = i; t <= end; ++t) {
            m_rows.insert(t, target.at(t));
            pos.insert(t, t);
        }
        endInsertRows();
        i = end + 1;
    }

    Q_ASSERT(m_rows.size() == target.size());
    // Any value may have changed: counts, references, the edited field.
    if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(m_rows.size() - 1));
    emit countChanged();
}
