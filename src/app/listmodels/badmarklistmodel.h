#pragma once

#include "listmodels/boardcontentmodel.h"

// Bad-mark positions of the board selected by boardIndex.
class BadMarkListModel : public BoardContentModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        XRole,
        YRole,
    };
    Q_ENUM(Role)

    using BoardContentModel::BoardContentModel;

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int add() override;
    bool remove(int row) override;

protected:
    IdRename::Scope scope() const override { return IdRename::Scope::BadMark; }
    QVector<Row> buildRows() const override;
    bool setRole(const Row &row, int role, const QVariant &value) override;
};
