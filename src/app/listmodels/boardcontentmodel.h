#pragma once

#include "listmodels/programlistmodel.h"

struct Board;

// A list that belongs to one board definition (placements, fiducials, bad
// marks). The view selects the board by setting boardIndex.
//
// The board is tracked by id: an edit that inserts or removes a board before
// it, or renames it, keeps the rows. When the board itself is removed the
// model shows the board that took its row until the view sets boardIndex.
class BoardContentModel : public ProgramListModel
{
    Q_OBJECT
    // Row of the board in Program::boards. -1 or out of range gives no rows.
    Q_PROPERTY(int boardIndex READ boardIndex WRITE setBoardIndex NOTIFY boardIndexChanged)

public:
    using ProgramListModel::ProgramListModel;

    int boardIndex() const { return m_boardIndex; }
    void setBoardIndex(int index);

    bool canAdd() const override { return board() != nullptr; }

signals:
    void boardIndexChanged();

protected:
    // nullptr when no valid board is selected.
    const Board *board() const;
    QString boardId() const { return m_boardId; }

    // The rename scope of this model's ids; renames of other boards are ignored.
    virtual IdRename::Scope scope() const = 0;

    void programReplaced() override;
    QString renamedKey(const QString &key, const QVector<IdRename> &renames) const override;
    bool rebuild(const QVector<IdRename> &renames) override;

private:
    QString idAt(int index) const;

    int m_boardIndex = -1;
    QString m_boardId;  // id of the board at m_boardIndex; empty when none
};
