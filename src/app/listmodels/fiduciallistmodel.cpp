#include "listmodels/fiduciallistmodel.h"

#include "display/displaytext.h"
#include "model/program.h"

#include <QStringList>

FiducialListModel::FiducialListModel(ProgramDocument *document, Scope scope, QObject *parent)
    : BoardContentModel(document, parent), m_scope(scope)
{
}

QVariant FiducialListModel::data(const QModelIndex &index, int role) const
{
    const Row *r = rowAt(index);
    const QVector<Fiducial> *list = fiducials();
    const Fiducial *f = r && list ? itemAt(*list, r->index) : nullptr;
    if (!f)
        return {};
    switch (role) {
    case IdRole:
        return f->id;
    case XRole:
        return f->x;
    case YRole:
        return f->y;
    case ShapeRole:
        return DisplayText::fiducialShape(f->shape);
    case ShapeIndexRole:
        return static_cast<int>(f->shape);
    case SizeRole:
        return f->size;
    }
    return {};
}

QHash<int, QByteArray> FiducialListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {XRole, "x"},
        {YRole, "y"},
        {ShapeRole, "shape"},
        {ShapeIndexRole, "shapeIndex"},
        {SizeRole, "size"},
    };
}

IdRename::Scope FiducialListModel::scope() const
{
    return m_scope == Scope::Panel ? IdRename::Scope::PanelFiducial
                                   : IdRename::Scope::BoardFiducial;
}

QVector<ProgramListModel::Row> FiducialListModel::buildRows() const
{
    QVector<Row> rows;
    if (const QVector<Fiducial> *list = fiducials()) {
        for (int i = 0; i < list->size(); ++i)
            rows.append({list->at(i).id, i, -1});
    }
    return rows;
}

void FiducialListModel::programReplaced()
{
    if (m_scope == Scope::Board)
        BoardContentModel::programReplaced();
}

QString FiducialListModel::renamedKey(const QString &key, const QVector<IdRename> &renames) const
{
    if (m_scope == Scope::Panel)
        return followRenames(key, renames, IdRename::Scope::PanelFiducial);
    return BoardContentModel::renamedKey(key, renames);
}

bool FiducialListModel::rebuild(const QVector<IdRename> &renames)
{
    return m_scope == Scope::Panel || BoardContentModel::rebuild(renames);
}

const QVector<Fiducial> *FiducialListModel::fiducials() const
{
    if (m_scope == Scope::Panel)
        return &program().panel.fiducials;
    const Board *b = board();
    return b ? &b->fiducials : nullptr;
}

QVector<Fiducial> &FiducialListModel::fiducials(Program &program) const
{
    if (m_scope == Scope::Panel)
        return program.panel.fiducials;
    return program.boards[boardIndex()].fiducials;
}

QString FiducialListModel::undoName() const
{
    return m_scope == Scope::Panel ? tr("拼板 Mark 点") : tr("Mark 点");
}

bool FiducialListModel::setRole(const Row &row, int role, const QVariant &value)
{
    if (!fiducials())
        return false;
    const int i = row.index;
    const QString text = tr("修改%1 %2").arg(undoName(), row.key);
    auto fiducial = [this, i](Program &p) -> Fiducial & { return fiducials(p)[i]; };
    switch (role) {
    case IdRole: {
        const QString id = value.toString().trimmed();
        const QString board = m_scope == Scope::Panel ? QString() : boardId();
        return editRename(tr("重命名%1 %2").arg(undoName(), row.key),
                          {scope(), board, row.key, id},
                          [&](Program &p) { fiducial(p).id = id; });
    }
    case XRole:
        return editNumber(text, value, [&](Program &p, double v) { fiducial(p).x = v; });
    case YRole:
        return editNumber(text, value, [&](Program &p, double v) { fiducial(p).y = v; });
    case ShapeIndexRole: {
        const int shape = value.toInt();
        if (shape < int(FiducialShape::Circle) || shape > int(FiducialShape::Cross))
            return false;
        return edit(text,
                    [&](Program &p) { fiducial(p).shape = static_cast<FiducialShape>(shape); });
    }
    case SizeRole:
        return editNumber(text, value, [&](Program &p, double v) { fiducial(p).size = v; });
    }
    return false;
}

bool FiducialListModel::canAdd() const
{
    return m_scope == Scope::Panel ? ProgramListModel::canAdd() : BoardContentModel::canAdd();
}

int FiducialListModel::add()
{
    const QVector<Fiducial> *list = fiducials();
    if (!list)
        return -1;
    QStringList ids;
    for (const Fiducial &f : *list)
        ids << f.id;
    Fiducial f;
    f.id = ProgramEdits::uniqueId(m_scope == Scope::Panel ? QStringLiteral("PFID")
                                                          : QStringLiteral("FID"),
                                  ids);
    const int row = list->size();
    if (!edit(tr("添加%1 %2").arg(undoName(), f.id), [&](Program &p) { fiducials(p).append(f); }))
        return -1;
    return row;
}

bool FiducialListModel::remove(int row)
{
    const Row *r = rowAt(row);
    if (!r || !fiducials())
        return false;
    const int i = r->index;
    return edit(tr("删除%1 %2").arg(undoName(), r->key),
                [&](Program &p) { fiducials(p).removeAt(i); });
}
