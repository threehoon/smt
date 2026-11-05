#pragma once

#include "listmodels/programlistmodel.h"

#include <QVector>

class BoardInstanceListModel : public ProgramListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        BoardRole,
        XRole,
        YRole,
        RotationRole,
        EnabledRole,
        SkipsRole,           // skipped placement ids, comma separated
        PlacementCountRole,  // placements to place: enabled and not skipped
    };
    Q_ENUM(Role)

    using ProgramListModel::ProgramListModel;

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Uses the board of the last instance, or the first board.
    int add() override;
    // Refused for the last instance (the validator requires one).
    bool remove(int row) override;
    // Moves a row up (delta < 0) or down; returns the new row or -1.
    Q_INVOKABLE int move(int row, int delta);

protected:
    QVector<Row> buildRows() const override;
    QString renamedKey(const QString &key, const QVector<IdRename> &renames) const override;
    bool rebuild(const QVector<IdRename> &renames) override;
    bool setRole(const Row &row, int role, const QVariant &value) override;

private:
    QVector<int> m_placementCount;  // per Panel::instances index
};
