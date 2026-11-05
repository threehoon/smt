#pragma once

#include "listmodels/programlistmodel.h"

#include <QHash>
#include <QString>
#include <QVector>

struct BoardInstance;
struct Placement;

// Placements expanded over the panel: one row per (board instance, placement),
// in instance order then placement order. Coordinates are board coordinates.
class JobPlacementListModel : public ProgramListModel
{
    Q_OBJECT
    // Rows that will be placed: instance enabled, placement enabled, not skipped.
    Q_PROPERTY(int enabledCount READ enabledCount NOTIFY enabledCountChanged)

public:
    enum Role {
        InstanceRole = Qt::UserRole + 1,  // BoardInstance::id
        IdRole,                           // Placement::id
        ComponentRole,
        SideRole,  // display text
        XRole,
        YRole,
        RotationRole,
        FeederRole,   // slots of the feeders for the component, by priority
        SkippedRole,  // in the instance's skip list
        EnabledRole,  // see enabledCount
    };
    Q_ENUM(Role)

    explicit JobPlacementListModel(ProgramDocument *document, QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int enabledCount() const { return m_enabledCount; }
    bool canAdd() const override { return false; }  // rows come from instances x placements

    // Adds the placement to its instance's skip list, or takes it out.
    Q_INVOKABLE bool toggleSkip(int row);

signals:
    void enabledCountChanged();

protected:
    // Row: index = Panel::instances index, sub = Board::placements index.
    QVector<Row> buildRows() const override;
    QString renamedKey(const QString &key, const QVector<IdRename> &renames) const override;
    bool rebuild(const QVector<IdRename> &renames) override;

private:
    const BoardInstance *instance(const Row &row) const;
    const Placement *placement(const Row &row) const;

    QVector<int> m_instanceBoard;  // Panel::instances index -> Program::boards index or -1
    QHash<QString, QString> m_feederSlots;  // Component::id -> slot list
    int m_enabledCount = 0;
};
