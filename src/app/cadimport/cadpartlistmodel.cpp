#include "cadimport/cadpartlistmodel.h"

#include "cadimport/cadimporter.h"

QVariant CadPartListModel::data(const QModelIndex &index, int role) const
{
    const QVector<CadPart> &parts = importer()->parts();
    if (!index.isValid() || index.row() >= parts.size())
        return {};
    const CadPart &part = parts.at(index.row());
    const CadImportSummary &s = importer()->summary();
    const QString component = index.row() < s.partComponents.size()
                                  ? s.partComponents.at(index.row()) : QString();
    const QString package = index.row() < s.partPackages.size()
                                ? s.partPackages.at(index.row()) : QString();

    switch (role) {
    case FootprintRole:
        return part.footprint;
    case ValueRole:
        return part.value;
    case CountRole:
        return part.designators.size();
    case DesignatorsRole:
        return part.designators.join(QStringLiteral(", "));
    case ComponentRole:
        return part.component.isEmpty() ? tr("新建 %1").arg(component) : component;
    case PackageRole:
        return s.newPackages.contains(package) ? tr("新建 %1").arg(package) : package;
    case ComponentIndexRole:
        return part.component.isEmpty() ? 0 : importer()->componentChoices().indexOf(part.component);
    case PackageIndexRole:
        return part.package.isEmpty() ? 0 : importer()->packageChoices().indexOf(part.package);
    case CreatesComponentRole:
        return part.component.isEmpty();
    default:
        return {};
    }
}

QHash<int, QByteArray> CadPartListModel::roleNames() const
{
    return {
        {FootprintRole, "footprint"},
        {ValueRole, "value"},
        {CountRole, "count"},
        {DesignatorsRole, "designators"},
        {ComponentRole, "component"},
        {PackageRole, "package"},
        {ComponentIndexRole, "componentIndex"},
        {PackageIndexRole, "packageIndex"},
        {CreatesComponentRole, "createsComponent"},
    };
}

bool CadPartListModel::set(int row, const QString &role, const QVariant &value)
{
    if (row < 0 || row >= importer()->parts().size())
        return false;
    if (role == QLatin1String("componentIndex")) {
        importer()->setPartComponent(row, value.toInt());
        return true;
    }
    if (role == QLatin1String("packageIndex")) {
        importer()->setPartPackage(row, value.toInt());
        return true;
    }
    return false;
}

QStringList CadPartListModel::keys() const
{
    QStringList keys;
    // \x1f cannot occur in a field of a text file line.
    for (const CadPart &p : importer()->parts())
        keys << p.footprint + QChar(0x1f) + p.value;
    return keys;
}
