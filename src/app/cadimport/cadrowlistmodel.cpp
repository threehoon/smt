#include "cadimport/cadrowlistmodel.h"

#include "cadimport/cadimporter.h"
#include "display/displaytext.h"

namespace {

QString statusText(const CadPlacement &p, const CadRowResult &result)
{
    if (!p.isValid())
        return p.error;
    QString text;
    switch (result.outcome) {
    case CadRowOutcome::Added:
        text = CadRowListModel::tr("新增");
        break;
    case CadRowOutcome::Updated:
        text = CadRowListModel::tr("更新");
        break;
    case CadRowOutcome::Unchanged:
        text = CadRowListModel::tr("不变");
        break;
    case CadRowOutcome::NotImported:
        break;
    }
    if (result.outsideBoard)
        text += CadRowListModel::tr("，在板外");
    return text;
}

}  // namespace

QVariant CadRowListModel::data(const QModelIndex &index, int role) const
{
    const QVector<CadPlacement> &rows = importer()->placements();
    if (!index.isValid() || index.row() >= rows.size())
        return {};
    const CadPlacement &p = rows.at(index.row());
    const QVector<CadRowResult> &results = importer()->summary().rowResults;
    const CadRowResult result = index.row() < results.size() ? results.at(index.row())
                                                             : CadRowResult();
    switch (role) {
    case LineRole:
        return p.line;
    case DesignatorRole:
        return p.designator;
    case XRole:
        return p.isValid() ? QVariant(p.x) : QVariant(QString());
    case YRole:
        return p.isValid() ? QVariant(p.y) : QVariant(QString());
    case RotationRole:
        return p.isValid() ? QVariant(p.rotation) : QVariant(QString());
    case SideRole:
        return p.isValid() ? DisplayText::side(p.side) : QString();
    case FootprintRole:
        return p.footprint;
    case ValueRole:
        return p.value;
    case ComponentRole: {
        if (!p.isValid())
            return QString();
        const QVector<CadPart> &parts = importer()->parts();
        const QStringList &ids = importer()->summary().partComponents;
        for (int i = 0; i < parts.size() && i < ids.size(); ++i) {
            if (parts.at(i).footprint == p.footprint && parts.at(i).value == p.value)
                return ids.at(i);
        }
        return QString();
    }
    case StatusRole:
        return statusText(p, result);
    case ErrorRole:
        return !p.isValid();
    default:
        return {};
    }
}

QHash<int, QByteArray> CadRowListModel::roleNames() const
{
    return {
        {LineRole, "line"},
        {DesignatorRole, "designator"},
        {XRole, "x"},
        {YRole, "y"},
        {RotationRole, "rotation"},
        {SideRole, "side"},
        {FootprintRole, "footprint"},
        {ValueRole, "value"},
        {ComponentRole, "component"},
        {StatusRole, "status"},
        {ErrorRole, "error"},
    };
}

QStringList CadRowListModel::keys() const
{
    QStringList keys;
    for (const CadPlacement &p : importer()->placements())
        keys << QString::number(p.line);
    return keys;
}
