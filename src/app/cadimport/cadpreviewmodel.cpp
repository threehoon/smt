#include "cadimport/cadpreviewmodel.h"

CadPreviewModel::CadPreviewModel(CadImporter *importer, QObject *parent)
    : QAbstractListModel(parent), m_importer(importer)
{
}

int CadPreviewModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_keys.size();
}

QVariantMap CadPreviewModel::get(int row) const
{
    QVariantMap values;
    const QModelIndex idx = index(row);
    if (!idx.isValid())
        return values;
    const QHash<int, QByteArray> names = roleNames();
    for (auto it = names.cbegin(); it != names.cend(); ++it)
        values.insert(QString::fromUtf8(it.value()), data(idx, it.key()));
    return values;
}

void CadPreviewModel::refresh()
{
    const QStringList keys = this->keys();
    if (keys == m_keys) {
        if (!keys.isEmpty())
            emit dataChanged(index(0), index(keys.size() - 1));
    } else {
        beginResetModel();
        m_keys = keys;
        endResetModel();
    }
    emit countChanged();
}
