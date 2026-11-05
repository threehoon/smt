#pragma once

#include "listmodels/programlistmodel.h"

class BoardListModel : public ProgramListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        WidthRole,
        HeightRole,
        ThicknessRole,
        PlacementCountRole,
        FiducialCountRole,
    };
    Q_ENUM(Role)

    using ProgramListModel::ProgramListModel;

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int add() override;
    bool remove(int row) override;
    QString removalImpact(int row) const override;

protected:
    QVector<Row> buildRows() const override;
    QString renamedKey(const QString &key, const QVector<IdRename> &renames) const override;
    bool setRole(const Row &row, int role, const QVariant &value) override;
};
