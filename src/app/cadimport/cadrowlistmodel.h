#pragma once

#include "cadimport/cadpreviewmodel.h"

// The data rows of the CAD file as read with the current options, and what
// the import does with each: added, updated, unchanged, or why it cannot be
// imported.
class CadRowListModel : public CadPreviewModel
{
    Q_OBJECT

public:
    enum Role {
        LineRole = Qt::UserRole + 1,
        DesignatorRole,
        XRole,
        YRole,
        RotationRole,
        SideRole,
        FootprintRole,
        ValueRole,
        ComponentRole,  // component the placement gets; empty for an error row
        StatusRole,
        ErrorRole,  // bool
    };

    using CadPreviewModel::CadPreviewModel;

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

protected:
    QStringList keys() const override;
};
