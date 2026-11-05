#pragma once

#include "cadimport/cadpreviewmodel.h"

#include <QString>
#include <QVariant>

// The footprint / value groups of the CAD file and the component each one
// becomes. Edited with set(): componentIndex and packageIndex are indices
// into CadImporter::componentChoices / packageChoices (0 = create).
class CadPartListModel : public CadPreviewModel
{
    Q_OBJECT

public:
    enum Role {
        FootprintRole = Qt::UserRole + 1,
        ValueRole,
        CountRole,
        DesignatorsRole,
        ComponentRole,  // display text: the id, or "新建 id"
        PackageRole,    // display text, likewise
        ComponentIndexRole,
        PackageIndexRole,
        CreatesComponentRole,  // bool
    };

    using CadPreviewModel::CadPreviewModel;

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Changes the choice of one part; for RecordForm. Returns false for a
    // read-only role.
    Q_INVOKABLE bool set(int row, const QString &role, const QVariant &value);

protected:
    QStringList keys() const override;
};
