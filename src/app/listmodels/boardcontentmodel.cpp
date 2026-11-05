#include "listmodels/boardcontentmodel.h"

#include "model/program.h"

void BoardContentModel::setBoardIndex(int index)
{
    if (index == m_boardIndex)
        return;
    const QString id = idAt(index);
    if (!id.isEmpty() && id == m_boardId) {
        // The view caught up with a board that moved; same rows.
        m_boardIndex = index;
        emit boardIndexChanged();
        return;
    }
    // Assign inside the reset so views never see rows of the old board
    // counted against the new one.
    resetRows([&] {
        m_boardIndex = index;
        m_boardId = id;
    });
    emit boardIndexChanged();
}

const Board *BoardContentModel::board() const
{
    const Board *b = itemAt(program().boards, m_boardIndex);
    return b && b->id == m_boardId ? b : nullptr;
}

QString BoardContentModel::idAt(int index) const
{
    const Board *b = itemAt(program().boards, index);
    return b ? b->id : QString();
}

void BoardContentModel::programReplaced()
{
    m_boardId = idAt(m_boardIndex);
}

QString BoardContentModel::renamedKey(const QString &key, const QVector<IdRename> &renames) const
{
    QString k = key;
    for (const IdRename &r : renames) {
        if (r.scope == scope() && r.board == m_boardId && r.from == k)
            k = r.to;
    }
    return k;
}

bool BoardContentModel::rebuild(const QVector<IdRename> &renames)
{
    QString id = m_boardId;
    for (const IdRename &r : renames) {
        if (r.scope == IdRename::Scope::Board && r.from == id)
            id = r.to;
    }
    if (id.isEmpty()) {
        // Nothing was shown; a board may have appeared at boardIndex.
        m_boardId = idAt(m_boardIndex);
        return m_boardId.isEmpty();
    }
    if (idAt(m_boardIndex) == id) {
        m_boardId = id;
        return true;
    }
    const QVector<Board> &boards = program().boards;
    for (int i = 0; i < boards.size(); ++i) {
        if (boards.at(i).id == id) {
            m_boardIndex = i;
            m_boardId = id;
            emit boardIndexChanged();
            return true;
        }
    }
    // The board was removed: show the one that took its row.
    m_boardId = idAt(m_boardIndex);
    return false;
}
