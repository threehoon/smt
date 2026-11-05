#include "listmodels/packagelistmodel.h"

#include "document/programdocument.h"
#include "model/program.h"

#include <QStringList>

namespace {

QStringList packageIds(const Program &p)
{
    QStringList ids;
    for (const Package &pkg : p.packages)
        ids << pkg.id;
    return ids;
}

}  // namespace

QVariant PackageListModel::data(const QModelIndex &index, int role) const
{
    const Row *r = rowAt(index);
    const Package *p = r ? itemAt(program().packages, r->index) : nullptr;
    if (!p)
        return {};
    switch (role) {
    case IdRole:
        return p->id;
    case BodyLengthRole:
        return p->bodyLength;
    case BodyWidthRole:
        return p->bodyWidth;
    case HeightRole:
        return p->height;
    case PinCountRole:
        return p->pinCount;
    case NozzlesRole:
        return p->pick.nozzles.join(QLatin1Char(','));
    case PickDepthRole:
        return p->pick.pickDepth;
    case VacuumCheckRole:
        return p->pick.vacuumCheck;
    case PlaceDepthRole:
        return p->place.placeDepth;
    case SpeedRole:
        return p->place.speed;
    case VisionMethodRole:
        return static_cast<int>(p->vision.method);
    case ToleranceRole:
        return p->vision.tolerance;
    }
    return {};
}

QHash<int, QByteArray> PackageListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {BodyLengthRole, "bodyLength"},
        {BodyWidthRole, "bodyWidth"},
        {HeightRole, "height"},
        {PinCountRole, "pinCount"},
        {NozzlesRole, "nozzles"},
        {PickDepthRole, "pickDepth"},
        {VacuumCheckRole, "vacuumCheck"},
        {PlaceDepthRole, "placeDepth"},
        {SpeedRole, "speed"},
        {VisionMethodRole, "visionMethod"},
        {ToleranceRole, "tolerance"},
    };
}

QVector<ProgramListModel::Row> PackageListModel::buildRows() const
{
    QVector<Row> rows;
    const QVector<Package> &list = program().packages;
    for (int i = 0; i < list.size(); ++i)
        rows.append({list.at(i).id, i, -1});
    return rows;
}

QString PackageListModel::renamedKey(const QString &key, const QVector<IdRename> &renames) const
{
    return followRenames(key, renames, IdRename::Scope::Package);
}

bool PackageListModel::setRole(const Row &row, int role, const QVariant &value)
{
    const int i = row.index;
    const QString text = tr("修改封装 %1").arg(row.key);
    auto package = [i](Program &p) -> Package & { return p.packages[i]; };
    switch (role) {
    case IdRole: {
        const QString id = value.toString().trimmed();
        return editRename(tr("重命名封装 %1").arg(row.key),
                          {IdRename::Scope::Package, {}, row.key, id},
                          [&](Program &p) { ProgramEdits::renamePackage(p, i, id); });
    }
    case BodyLengthRole:
        return editNumber(text, value, [&](Program &p, double v) { package(p).bodyLength = v; });
    case BodyWidthRole:
        return editNumber(text, value, [&](Program &p, double v) { package(p).bodyWidth = v; });
    case HeightRole:
        return editNumber(text, value, [&](Program &p, double v) { package(p).height = v; });
    case PinCountRole:
        return editInteger(text, value, [&](Program &p, int v) { package(p).pinCount = v; });
    case NozzlesRole: {
        QStringList nozzles;
        for (const QString &n : value.toString().split(QLatin1Char(','))) {
            if (!n.trimmed().isEmpty())
                nozzles.append(n.trimmed());
        }
        return edit(text, [&](Program &p) { package(p).pick.nozzles = nozzles; });
    }
    case PickDepthRole:
        return editNumber(text, value, [&](Program &p, double v) { package(p).pick.pickDepth = v; });
    case VacuumCheckRole:
        return edit(text, [&](Program &p) { package(p).pick.vacuumCheck = value.toBool(); });
    case PlaceDepthRole:
        return editNumber(text, value,
                          [&](Program &p, double v) { package(p).place.placeDepth = v; });
    case SpeedRole:
        return editInteger(text, value, [&](Program &p, int v) { package(p).place.speed = v; });
    case VisionMethodRole: {
        const int method = value.toInt();
        if (method < int(VisionMethod::None) || method > int(VisionMethod::Top))
            return false;
        return edit(text, [&](Program &p) {
            package(p).vision.method = static_cast<VisionMethod>(method);
        });
    }
    case ToleranceRole:
        return editNumber(text, value,
                          [&](Program &p, double v) { package(p).vision.tolerance = v; });
    }
    return false;
}

int PackageListModel::add()
{
    Package pkg;
    pkg.id = ProgramEdits::uniqueId(QStringLiteral("PKG"), packageIds(program()));
    pkg.bodyLength = 1.0;
    pkg.bodyWidth = 0.5;
    pkg.height = 0.5;
    const int row = program().packages.size();
    if (!edit(tr("添加封装 %1").arg(pkg.id), [&](Program &p) { p.packages.append(pkg); }))
        return -1;
    return row;
}

bool PackageListModel::remove(int row)
{
    const Row *r = rowAt(row);
    if (!r)
        return false;
    const int i = r->index;
    return edit(tr("删除封装 %1").arg(r->key),
                [&](Program &p) { ProgramEdits::removePackage(p, i); });
}

QString PackageListModel::removalImpact(int row) const
{
    const Row *r = rowAt(row);
    return r ? describe(ProgramEdits::packageRemovalImpact(program(), r->index)) : QString();
}
