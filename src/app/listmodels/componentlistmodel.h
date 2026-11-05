#pragma once

#include "listmodels/programlistmodel.h"

#include <QHash>
#include <QString>

class ComponentListModel : public ProgramListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        PackageRole,
        ValueRole,
        DescriptionRole,
        FeederCountRole,     // feeders assigned to this component, enabled or not
        PlacementCountRole,  // placements across all board definitions
    };
    Q_ENUM(Role)

    using ProgramListModel::ProgramListModel;

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Needs a package to refer to; refuses when there is none.
    int add() override;
    bool remove(int row) override;
    QString removalImpact(int row) const override;

protected:
    QVector<Row> buildRows() const override;
    QString renamedKey(const QString &key, const QVector<IdRename> &renames) const override;
    bool rebuild(const QVector<IdRename> &renames) override;
    bool setRole(const Row &row, int role, const QVariant &value) override;

private:
    QHash<QString, int> m_feederCount;     // Component::id -> count
    QHash<QString, int> m_placementCount;  // Component::id -> count
};
