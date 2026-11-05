#pragma once

#include "listmodels/boardcontentmodel.h"

#include <QVector>

struct Fiducial;

// Fiducials of either the panel or the board selected by boardIndex. The scope
// is fixed at construction; boardIndex is ignored for the panel.
class FiducialListModel : public BoardContentModel
{
    Q_OBJECT

public:
    enum class Scope { Board, Panel };
    enum Role {
        IdRole = Qt::UserRole + 1,
        XRole,
        YRole,
        ShapeRole,       // display text
        ShapeIndexRole,  // int, FiducialShape order: circle, square, cross
        SizeRole,
    };
    Q_ENUM(Role)

    FiducialListModel(ProgramDocument *document, Scope scope, QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool canAdd() const override;
    int add() override;
    bool remove(int row) override;

protected:
    IdRename::Scope scope() const override;
    QVector<Row> buildRows() const override;
    void programReplaced() override;
    QString renamedKey(const QString &key, const QVector<IdRename> &renames) const override;
    bool rebuild(const QVector<IdRename> &renames) override;
    bool setRole(const Row &row, int role, const QVariant &value) override;

private:
    // nullptr when the scope is Board and no valid board is selected.
    const QVector<Fiducial> *fiducials() const;
    // The same list in `program`, which is a copy of program() being edited.
    QVector<Fiducial> &fiducials(Program &program) const;
    QString undoName() const;

    Scope m_scope;
};
