#pragma once

#include "listmodels/programlistmodel.h"

// Feeders of the program's FeederSetup; no rows when the program has none.
class FeederListModel : public ProgramListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TypeRole,       // display text
        TypeIndexRole,  // int, FeederType order: tape, tray, tube, bulk; -1 = unknown
        SlotRole,
        ComponentRole,
        PickXRole,
        PickYRole,
        PickZRole,
        RotationRole,
        EnabledRole,
        PriorityRole,
        TapeWidthRole,
        TapePitchRole,
        TapeFeedCountRole,
        TrayRowsRole,
        TrayColumnsRole,
        TrayRowPitchRole,
        TrayColumnPitchRole,
        TrayStartIndexRole,
        TubePitchRole,
    };
    Q_ENUM(Role)

    using ProgramListModel::ProgramListModel;

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Creates the FeederSetup if the program has none. Needs a component.
    int add() override;
    bool remove(int row) override;

protected:
    QVector<Row> buildRows() const override;
    QString renamedKey(const QString &key, const QVector<IdRename> &renames) const override;
    bool setRole(const Row &row, int role, const QVariant &value) override;
};
