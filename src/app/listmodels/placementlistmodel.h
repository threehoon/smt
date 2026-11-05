#pragma once

#include "listmodels/boardcontentmodel.h"

// Placements of the board selected by boardIndex.
class PlacementListModel : public BoardContentModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        ComponentRole,
        SideRole,       // display text
        SideIndexRole,  // int, Side order: top, bottom
        XRole,
        YRole,
        RotationRole,
        EnabledRole,
    };
    Q_ENUM(Role)

    using BoardContentModel::BoardContentModel;

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Needs a selected board and a component; refuses otherwise.
    int add() override;
    bool remove(int row) override;

protected:
    IdRename::Scope scope() const override { return IdRename::Scope::Placement; }
    QVector<Row> buildRows() const override;
    bool setRole(const Row &row, int role, const QVariant &value) override;
};
